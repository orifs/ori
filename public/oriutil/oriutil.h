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

#ifndef __ORI_ORIUTIL_H__
#define __ORI_ORIUTIL_H__

#include <stdint.h>

#include <string>
#include <vector>

// GCC Only
#define UNUSED __attribute__((unused))

bool Util_IsValidName(const std::string &path);

std::vector<std::string> Util_PathToVector(const std::string &path);
std::string Util_GetFullname();
std::string Util_GetHome();
std::string Util_GetOSType();
std::string Util_GetMachType();
int Util_SetBlocking(int fd, bool block);

std::string Util_NewUUID();
bool Util_IsPathRemote(const std::string &path);

std::string Util_SystemError(int status);

#endif /* __ORI_ORIUTIL_H__ */

