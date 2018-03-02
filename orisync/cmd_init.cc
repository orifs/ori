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
#include <stdlib.h>

#include <string>
#include <iostream>

#include <oriutil/oriutil.h>

#include "orisyncconf.h"

using namespace std;

string
generateKey()
{
    int i;
    string val = "";
    const char alphanumeric[37] = "abcdefghijklmnopqrstuvwxyz0123456789";

    for (i = 0; i < 8; i++) {
        val += alphanumeric[rand() % 36];
    }

    return val;
}

int
cmd_init(int argc, const char *argv)
{
    bool isFirst = false;
    string val;
    string clusterName;
    string clusterKey;
    OriSyncConf rc = OriSyncConf();

    do {
        cout << "Is this the first machine in the cluster (y/n)? ";
        cin >> val;
    } while (val != "y" && val != "n");

    if (val == "y") {
        isFirst = true;
    }

    cout << "Enter the cluster name: ";
    cin >> clusterName;

    if (isFirst) {
        clusterKey = generateKey();
    } else {
        cout << "Enter the cluster key: ";
        cin >> clusterKey;
    }

    rc.setCluster(clusterName, clusterKey);
    rc.setUUID(Util_NewUUID());

    cout << endl;
    cout << "Use the following configuration for all other machines:" << endl;
    cout << "Cluster Name: " << clusterName << endl;
    cout << "Cluster Key:  " << clusterKey << endl;
    cout << endl;
    cout << "Now use 'orisync add' to register repositories." << endl;

    return 0;
}

