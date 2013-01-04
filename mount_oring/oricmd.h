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

#ifndef __ORICMD_H__
#define __ORICMD_H__

class OriPriv;

class OriCommand
{
public:
    OriCommand(OriPriv *priv);
    ~OriCommand();
    int readSize();
    int read(char *buf, size_t size, size_t offset);
    int write(const char *buf, size_t size, size_t offset);
    void printf(const char *fmt, ...);
private:
    int cmd_fsck(int argc, const char *argv[]);
    int cmd_log(int argc, const char *argv[]);
    int cmd_show(int argc, const char *argv[]);
    int cmd_status(int argc, const char *argv[]);
    int cmd_tip(int argc, const char *argv[]);
    OriPriv *priv;
    std::string resultBuffer;
};

#endif /* __ORICMD_H__ */

