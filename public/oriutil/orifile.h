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

#ifndef __ORI_ORIFILE_H__
#define __ORI_ORIFILE_H__

#include <stdint.h>

#include <string>

bool OriFile_Exists(const std::string &path);
bool OriFile_IsDirectory(const std::string &path);
int OriFile_MkDir(const std::string &path);
int OriFile_RmDir(const std::string &path);
size_t OriFile_GetSize(const std::string &path);
std::string OriFile_RealPath(const std::string &path);
std::string OriFile_ReadFile(const std::string &path);
bool OriFile_WriteFile(const std::string &blob, const std::string &path);
bool OriFile_WriteFile(const char *blob, size_t len, const std::string &path);
bool OriFile_Append(const std::string &blob, const std::string &path);
bool OriFile_Append(const char *blob, size_t len, const std::string &path);
int OriFile_Copy(const std::string &origPath, const std::string &newPath);
int OriFile_Move(const std::string &origPath, const std::string &newPath);
int OriFile_Delete(const std::string &path);
int OriFile_Rename(const std::string &from, const std::string &to);

std::string OriFile_Basename(const std::string &path);
std::string OriFile_Dirname(const std::string &path);

#endif /* __ORI_ORIFILE_H__ */

