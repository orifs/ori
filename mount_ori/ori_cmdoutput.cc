#include "ori_fuse.h"

#include <stdarg.h>
#include <stdlib.h>

#define OUTPUT_MAX 1024

void ori_priv::printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[OUTPUT_MAX];
    vsnprintf(buffer, OUTPUT_MAX, fmt, args);

    outputBuffer += buffer;
    ::printf("Output: %s\n", buffer);

    va_end(args);
}

size_t ori_priv::readOutput(char *buf, size_t n)
{
    if (outputBuffer.size() == 0)
        return 0;

    size_t to_read = n;
    if (to_read > outputBuffer.size())
        to_read = outputBuffer.size();

    memcpy(buf, &outputBuffer[0], to_read);

    size_t newSize = outputBuffer.size() - to_read;
    memmove(&outputBuffer[0], &outputBuffer[to_read], newSize);
    outputBuffer.resize(newSize);
    return to_read;
}
