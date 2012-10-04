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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <set>
#include <string>
#include <iostream>
#include <iomanip>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/scan.h>
#include <ori/peer.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;
    
int
cmd_remote(int argc, const char *argv[])
{
    if (argc == 1) {
	map<string, Peer> peers = repository.getPeers();
	map<string, Peer>::iterator it;

	// print peers
	cout << left << setw(16) << "Name"
	     << left << setw(64) << "Path" << "\nID" << endl;
	for (it = peers.begin(); it != peers.end(); it++)
	{
	    Peer &p = (*it).second;
	    cout << left << setw(16) << (*it).first
		 << left << setw(64) << p.getUrl() << "\n" << p.getRepoId() << endl;
	}

	return 0;
    }

    if (argc == 4 && strcmp(argv[1], "add") == 0) {
	repository.addPeer(argv[2], argv[3]);
	return 0;
    }

    if (argc == 3 && strcmp(argv[1], "del") == 0) {
	repository.removePeer(argv[2]);
	return 0;
    }

    printf("Specify a command.\n");
    printf("usage: ori remote - Show the listings\n");
    printf("usage: ori remote add [NAME] [PATH] - Add a remote repository\n");
    printf("usage: ori remote del [NAME] - Delete a remote repository\n");

    return 1;
}

