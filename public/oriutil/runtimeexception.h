/*
 * Copyright (c) 2012-2013 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __RUNTIMEEXCEPTION_H__
#define __RUNTIMEEXCEPTION_H__

#include <string.h>

#ifndef _WIN32
#include <errno.h>
#endif

#include <string>
#include <exception>

enum OriErrorCode {
    ORIEC_INVALIDARGS,
    ORIEC_UNSUPPORTEDVERSION,
    ORIEC_INDEXDIRTY,
    ORIEC_INDEXCORRUPT,
    ORIEC_INDEXNOTFOUND,
    ORIEC_BSCORRUPT
};

class RuntimeException : public std::exception
{
public:
    RuntimeException(OriErrorCode code, const std::string &msg) {
        errorString = "Exception: ";
        errorString += msg;
        errorCode = code;
    }
    RuntimeException(OriErrorCode code, const char *msg) {
        errorString = "Exception: ";
        errorString += msg;
        errorCode = code;
    }
    virtual ~RuntimeException() throw()
    {
    }
    virtual OriErrorCode getCode() throw()
    {
        return errorCode;
    }
    virtual const char* what() const throw()
    {
        return errorString.c_str();
    }
private:
    std::string errorString;
    OriErrorCode errorCode;
};

#endif /* __RUNTIMEEXCEPTION_H__ */
