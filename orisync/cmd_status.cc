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

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <map>

#include <oriutil/debug.h>
#include <oriutil/kvserializer.h>

#include "orisyncconf.h"
#include "repoinfo.h"
#include "hostinfo.h"

// Timeout to detect host down
#define HOST_TIMEOUT            10

using namespace std;
extern map<string, HostInfo *> hosts;

int
cmd_status(int argc, const char *argv)
{
  
    string stat;

    cout << left << setw(32) << "HOST" << "STATUS" << endl;
    for (auto &it : hosts) {
        if ((it.second->getTime() + HOST_TIMEOUT < time(NULL)) && (it.second->getStatus() == "OK")) {
            time_t time = it.second->getTime();
            char timeStr[26];
            ctime_r(&time, timeStr);
            string down("Down. Last connected ");
            string lasttime(timeStr);
            it.second->setStatus((down + lasttime));
        }
        cout << left << setw(32) << it.second->getHost() << it.second->getStatus() << endl;
    }

    return 0;
}
