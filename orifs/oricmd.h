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

#ifndef __ORICMD_H__
#define __ORICMD_H__

class OriPriv;

class OriCommand
{
public:
    OriCommand(OriPriv *priv);
    ~OriCommand();
    std::string process(const std::string &data);
private:
    std::string cmd_fsck(strstream &str);
    std::string cmd_snapshot(strstream &str);
    std::string cmd_snapshots(strstream &str);
    std::string cmd_status(strstream &str);
    std::string cmd_pull(strstream &str);
    std::string cmd_checkout(strstream &str);
    std::string cmd_merge(strstream &str);
    std::string cmd_varlink(strstream &str);
    std::string cmd_remote(strstream &str);
    std::string cmd_branch(strstream &str);
    std::string cmd_version(strstream &str);
    std::string cmd_purgesnapshot(strstream &str);
    OriPriv *priv;
};

#endif /* __ORICMD_H__ */

