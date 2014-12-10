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


using namespace std;
extern map<string, HostInfo *> hosts;

int
cmd_status(int mode, const char *argv)
{
  
    string stat;

    if (mode == 0) {
        cout << "Orisync is off" << endl;
    } else {
      cout << left << setw(32) << "HOST" << "STATUS" << endl;
      for (auto &it : hosts) {
        cout << left << setw(32) << it.second->getHost() << it.second->getStatus() << endl;
      }
    }

    return 0;
}
