/*
 * Copyright (c) 2013 Stanford University
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

#ifndef __ORI_VARLINK_H__
#define __ORI_VARLINK_H__

class VarLink
{
public:
    VarLink();
    ~VarLink();
    void open(const std::string &vardbPath);
    std::string get(const std::string &var) const;
    void set(const std::string &var, const std::string &val);
    std::list<std::string> getVars() const;
    std::string parse(const std::string &link);
private:
    std::string getBlob();
    void fromBlob(const std::string &blob);
    void load();
    void save();
    std::string vardb;
    std::map<std::string, std::string> vars;
};

#endif /* __ORI_VARLINK_H__ */

