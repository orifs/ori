#include <cassert>

#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "stream.h"

/*
 * diskstream
 */

// TODO: error checking

diskstream::diskstream(int fd, off_t offset, size_t length)
    : fd(fd), offset(offset), length(length), left(length)
{
    lseek(fd, offset, SEEK_SET);
}

bool diskstream::ended() {
    return left == 0;
}

size_t diskstream::read(uint8_t *buf, size_t n) {
    size_t final_size = MIN(n, left);
retry_read:
    ssize_t read_bytes = ::read(fd, buf, final_size);
    if (read_bytes < 0) {
        if (errno == EINTR)
            goto retry_read;
        // TODO error
        return 0;
    }
    left -= read_bytes;
    return read_bytes;
}

/*
 * lzmastream
 */

lzmastream::lzmastream(bytestream *source)
    : source(source), output_ended(false)
{
    assert(source != NULL);

    lzma_stream strm2 = LZMA_STREAM_INIT;
    memcpy(&strm, &strm2, sizeof(lzma_stream));
    lzma_stream_decoder(&strm, UINT64_MAX, 0);
}

lzmastream::~lzmastream() {
    delete source;
}

bool lzmastream::ended() {
    return output_ended;
}

size_t lzmastream::read(uint8_t *buf, size_t n) {
    if (output_ended) return 0;

    lzma_action action = source->ended() ? LZMA_FINISH : LZMA_RUN;
    size_t begin_total = strm.total_out;

    strm.next_out = buf;
    strm.avail_out = n;
    while (strm.avail_out > 0) {
        if (output_ended) break;

        if (strm.avail_in == 0) {
            size_t read_bytes = source->read(in_buf, XZ_READ_BY);
            action = read_bytes == 0 ? LZMA_FINISH : LZMA_RUN;

            strm.next_in = in_buf;
            strm.avail_in = read_bytes;
        }

        lzma_ret ret = lzma_code(&strm, action);
        if (ret == LZMA_STREAM_END) {
            output_ended = true;
            lzma_end(&strm);
        }
        else if (ret != LZMA_OK) {
            printf("lzma_code: %d\n", ret);
            return 0;
        }
    }

    return strm.total_out - begin_total;
}
