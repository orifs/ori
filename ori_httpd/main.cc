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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <resolv.h>

#include <string>
#include <iostream>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/zeroconf.h>
#include <ori/version.h>
#include <ori/localrepo.h>
#include <ori/httpserver.h>

using namespace std;

LocalRepo repository;

void
usage()
{
    printf("Ori Distributed Personal File System (%s) - HTTP Server\n",
            ORI_VERSION_STR);
    cout << "Usage: ori_httpd [OPTIONS] [REPOSITORY PATH]" << endl << endl;
    cout << "Options:" << endl;
    cout << "    -p port    Set the HTTP port number (default: 8080)" << endl;
#if !defined(WITHOUT_MDNS)
    cout << "    -m         Enable mDNS (default)" << endl;
    cout << "    -n         Disable mDNS" << endl;
#endif
    cout << "    -h         Show this message" << endl;
}

int
main(int argc, char *argv[])
{
    int ch;
    bool mDNS_flag = true;
    unsigned long port = 8080;

    while ((ch = getopt(argc, argv, "p:mnh")) != -1) {
        switch (ch) {
            case 'p':
            {
                char *p;
                port = strtoul(optarg, &p, 10);
                if (*p != '\0' || port > 65535) {
                    cout << "Invalid port number '" << optarg << "'" << endl;
                    usage();
                    return 1;
                }
                break;
            }
            case 'm':
                mDNS_flag = true;
                break;
            case 'n':
                mDNS_flag = false;
                break;
            case 'h':
                usage();
                return 0;
            case '?':
            default:
                usage();
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    try {
        if (argc == 1) {
            repository.open(argv[0]);
        } else {
            repository.open();
        }
    } catch (std::exception &e) {
        cout << e.what() << endl;
        cout << "Could not open the local repository!" << endl;
        usage();
        return 1;
    }

    ori_open_log(repository.getLogPath());
    LOG("libevent %s", event_get_version());

    HTTPServer server = HTTPServer(repository, port);
    server.start(mDNS_flag);

    return 0;
}

