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

#include "orisyncconf.h"

#include <ori/localrepo.h>

using namespace std;

extern OriSyncConf rc;
extern RWLock rcLock; 

int
cmd_hostremove(int mode, const char *argv)
{
  if (mode == 0) {
    OriSyncConf rc = OriSyncConf();

    /*
    if (argc != 2)
    {
        cout << "Specify a static host to remove" << endl;
        cout << "usage: orisync hostremove <HOSTNAME>" << endl;
    }
    */

    rc.removeHost(argv);
  } else {
    RWKey::sp key = rcLock.writeLock();
    rc.removeHost(argv);
  }

  return 0;
}

