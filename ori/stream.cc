#include <cassert>

#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

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
    ssize_t read_bytes = ::read(fd, buf, final_size);
    if (read_bytes < 0) {
        // TODO error
        return 0;
    }
    left -= read_bytes;
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

bool lzmastream::ended() {
    return source->ended();
}

size_t lzmastream::read(uint8_t *buf, size_t n) {
    while (buffer.size() < n) {
        if (!_readMore()) break;
    }

    size_t read_n = MIN(n, buffer.size());
    memcpy(buf, &buffer[0], read_n);
    return read_n;
}

#define READ_BY 4096

bool lzmastream::_readMore() {
    if (output_ended) return false;

    uint8_t buf[READ_BY];
    size_t read_n = source->read(buf, READ_BY);
    lzma_action action = read_n == 0 ? LZMA_FINISH : LZMA_RUN;

    strm.next_in = buf;
    strm.avail_in = read_n;

    do {
        off_t last = buffer.size();
        buffer.resize(last + READ_BY);
        strm.next_out = &buffer[last];
        strm.avail_out = READ_BY;

        lzma_ret ret = lzma_code(&strm, action);
        if (ret == LZMA_STREAM_END) {
            output_ended = true;
            break;
        }
    } while(strm.avail_out == 0);

    if (strm.avail_out > 0) {
        buffer.resize(buffer.size() - strm.avail_out);
    }

    if (output_ended) {
        lzma_end(&strm);
        return false;
    }

    return true;
}
