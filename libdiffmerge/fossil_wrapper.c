
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "fossil_wrapper.h"

void fossil_panic(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fossil_exit(1);
}

void fossil_fatal(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fossil_exit(1);
}

void fossil_warning(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

