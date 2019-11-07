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

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <iostream>
#include <iomanip>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>
#include <oriutil/runtimeexception.h>

#include "fuse_cmd.h"

using namespace std;

extern UDSRepo repository;

int
cmd_log(int argc, char * const argv[])
{
    ObjectHash commit;
    if (argc >= 2) {
        commit = ObjectHash::fromHex(argv[1]);
    } else {
        commit = repository.getHead();
    }

    while (commit != EMPTY_COMMIT) {
        Commit c;
        try {
            c = repository.getCommit(commit);
        } catch (std::ios_base::failure e) {
            cout << "Error: could not read commit " << commit.hex() << endl;
            break;
        } catch (RuntimeException& e) {
            if (e.getCode() == ORIEC_BSCORRUPT) {
                cout << "Error: could not read commit " << commit.hex() << endl;
            } else {
                DLOG("Exception: %s", e.what());
            }
            break;
        } catch (...) {
            DLOG("Unexpected exception trying to get commit");
            break;
        }

        pair<ObjectHash, ObjectHash> p = c.getParents();
        time_t timeVal = c.getTime();
        char timeStr[26];

        ctime_r(&timeVal, timeStr);

        cout << "Commit:    " << commit.hex() << endl;
        cout << "Parents:   " << (p.first.isEmpty() ? "" : p.first.hex())
                              << " "
                              << (p.second.isEmpty() ? "" : p.second.hex())
                              << endl;
        cout << "Tree:      " << c.getTree().hex() << endl;
        cout << "Author:    " << c.getUser() << endl;
        cout << "Date:      " << timeStr;
        if (c.hasSignature()) {
            /*
            map<string, PublicKey> keys = repository.getPublicKeys();
            map<string, PublicKey>::iterator it;

            for (it = keys.begin(); it != keys.end(); it++) {
                if (c.verify(it->second)) {
                    cout << "Signature: Verified (" << it->first << ")" << endl;
                    goto foundKey;
                }
            }
            cout << "Signature: Unverified" << endl;
            */
            cout << "Signature: UNSUPPORTED" << endl;
        }
        //cout << "Status:    " << repository.getMetadata().getMeta(commit,
        //        "status") << endl;
        if (!c.getGraftCommit().isEmpty()) {
            cout << "Graft:   from " << c.getGraftRepo().first << ":"
                                     << c.getGraftRepo().second << endl;
            cout << "         Commit " << c.getGraftCommit().hex() << endl;
        }
        cout << endl << c.getMessage() << endl << endl;

        commit = c.getParents().first;
        // XXX: Handle merge cases
    }

    return 0;
}

