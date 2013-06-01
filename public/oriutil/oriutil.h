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

#ifndef __ORI_ORIUTIL_H__
#define __ORI_ORIUTIL_H__

#include <stdint.h>

#include <string>
#include <vector>

#include "objecthash.h"

// GCC Only
#define UNUSED __attribute__((unused))

bool Util_FileExists(const std::string &path);
bool Util_IsDirectory(const std::string &path);
int Util_MkDir(const std::string &path);
int Util_RmDir(const std::string &path);
size_t Util_FileSize(const std::string &path);
std::string Util_RealPath(const std::string &path);
std::string Util_ReadFile(const std::string &path);
bool Util_WriteFile(const std::string &blob, const std::string &path);
bool Util_WriteFile(const char *blob, size_t len, const std::string &path);
bool Util_AppendFile(const std::string &blob, const std::string &path);
bool Util_AppendFile(const char *blob, size_t len, const std::string &path);
int Util_CopyFile(const std::string &origPath, const std::string &newPath);
int Util_MoveFile(const std::string &origPath, const std::string &newPath);
int Util_DeleteFile(const std::string &path);
int Util_RenameFile(const std::string &from, const std::string &to);

ObjectHash Util_HashString(const std::string &str);
ObjectHash Util_HashBlob(const uint8_t *data, size_t len);
ObjectHash Util_HashFile(const std::string &path);

std::vector<std::string> Util_PathToVector(const std::string &path);
std::vector<std::string> Util_StringSplit(const std::string &str, char sep);
std::string Util_StringJoin(const std::vector<std::string> &str, char sep);
std::string Util_GetFullname();
std::string Util_GetHome();
int Util_SetBlocking(int fd, bool block);

void Util_PrintHex(const std::string &data, off_t off = 0, size_t limit = 0);
void Util_PrintBacktrace();
void Util_LogBacktrace();
std::string Util_NewUUID();
bool Util_IsPathRemote(const std::string &path);
std::string Util_ResolveHost(const std::string &hostname);

std::string StrUtil_Basename(const std::string &path);
std::string StrUtil_Dirname(const std::string &path);
bool StrUtil_StartsWith(const std::string &str, const std::string &part);
bool StrUtil_EndsWith(const std::string &str, const std::string &part);

#endif /* __ORI_ORIUTIL_H__ */

