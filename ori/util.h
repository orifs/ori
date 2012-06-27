/*
 * Copyright (c) 2012 Stanford University
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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <vector>

bool Util_FileExists(const std::string &path);
bool Util_IsDirectory(const std::string &path);
size_t Util_FileSize(const std::string &path);
std::string Util_RealPath(const std::string &path);
char *Util_ReadFile(const std::string &path, size_t *flen);
bool Util_WriteFile(const char *blob, size_t len, const std::string &path);
int Util_CopyFile(const std::string &origPath, const std::string &newPath);
int Util_MoveFile(const std::string &origPath, const std::string &newPath);
std::string Util_HashString(const std::string &str);
std::string Util_HashFile(const std::string &path);
std::vector<std::string> Util_PathToVector(const std::string &path);
std::string Util_GetFullname();
int Util_SetBlocking(int fd, bool block);

void Util_PrintHex(const std::string &data, off_t off = 0, size_t limit = 0);
std::string Util_NewUUID();

#endif /* __UTIL_H__ */

