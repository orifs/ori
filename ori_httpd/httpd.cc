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

#include "evbufstream.h"

using namespace std;

static struct evhttp *httpd;
/* set if a test needs to call loopexit on a base */
static struct event_base *base;

LocalRepo repository;

int
Httpd_authenticate(struct evhttp_request *req, struct evbuffer *buf)
{
    int n;
    char output[64];
    const char *as = evhttp_find_header(req->input_headers, "Authorization");
    if (as == NULL) {
	goto authFailed;
    }

    // Select auth type

    n = b64_pton(strchr(as,' ') + 1, (u_char *) output, 64);
    output[n] = '\0';
    if (strcmp(output, "ali:test") != 0) {
        goto authFailed;
    }
    return 0;

authFailed:
    evhttp_add_header(req->output_headers, "WWW-Authenticate",
                      "Basic realm=\"Secure Area\"");
    evhttp_send_reply(req, 401, "Authorization Required", buf);
    return -1;
}

void
Httpd_stop(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    if (Httpd_authenticate(req, buf) < 0)
    {
        return;
    }

    evbuffer_add_printf(buf, "Stopping\n");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    event_base_loopexit(base, NULL);
}

void
Httpd_getId(struct evhttp_request *req, void *arg)
{
    string repoId = repository.getUUID();
    struct evbuffer *buf;

    LOG("httpd: getid");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", repoId.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_getVersion(struct evhttp_request *req, void *arg)
{
    string ver = repository.getVersion();
    struct evbuffer *buf;

    LOG("httpd: getversion");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", ver.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_head(struct evhttp_request *req, void *arg)
{
    ObjectHash headId = repository.getHead();
    ASSERT(!headId.isEmpty());
    struct evbuffer *buf;

    LOG("httpd: gethead");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("couldn't allocate evbuffer!");
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "%s", headId.hex().c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_getIndex(struct evhttp_request *req, void *arg)
{
    LOG("httpd: getindex");

    evbufwstream es;

    std::set<ObjectInfo> objects = repository.listObjects();
    es.writeUInt64(objects.size());
    for (std::set<ObjectInfo>::iterator it = objects.begin();
            it != objects.end();
            it++) {
        int status = es.writeInfo(*it);
        if (status < 0) {
            LOG("couldn't write info to evbuffer!");
            evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
            return;
        }
    }

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

void
Httpd_getCommits(struct evhttp_request *req, void *arg)
{
    LOG("httpd: getCommits");

    evbufwstream es;

    vector<Commit> commits = repository.listCommits();
    es.writeUInt32(commits.size());
    for (size_t i = 0; i < commits.size(); i++) {
        std::string blob = commits[i].getBlob();
        if (es.writePStr(blob) < 0) {
            LOG("couldn't write pstr to evbuffer");
            evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
            return;
        }
    }

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

void
Httpd_stringDeleteCB(const void *data, size_t len, void *extra)
{
    string *p = (string *)extra;

    delete p;
}

/*void
Httpd_getObj(struct evhttp_request *req, void *arg)
{
    string sobjId;
    auto_ptr<bytestream> bs;
    string *payload = new string();
    string objInfo;
    struct evbuffer *buf;
    bool includePayload;

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("httpd_getobj: evbuffer_new failed!");
    }

    sobjId = evhttp_request_get_uri(req);

    if (sobjId.substr(0, 6) == "/objs/") {
	sobjId = sobjId.substr(6);
	includePayload = true;
    } else if (sobjId.substr(0, 9) == "/objinfo/") {
	sobjId = sobjId.substr(9);
	includePayload = false;
    } else {
	evhttp_send_reply(req, HTTP_NOTFOUND, "File Not Found", buf);
	return;
    }

    if (sobjId.size() != 64) {
        evhttp_send_reply(req, HTTP_BADREQUEST, "Bad Request", buf);
        return;
    }
    
    LOG("httpd: getobj %s", sobjId.c_str());

    Object::sp obj = repository.getObject(ObjectHash::fromHex(sobjId));
    if (obj == NULL) {
        evhttp_send_reply(req, HTTP_NOTFOUND, "Object Not Found", buf);
        return;
    }

    if (includePayload) {
	bs = obj->getStoredPayloadStream();
	*payload = bs->readAll();
    }
    objInfo = obj->getInfo().getInfo();

    ASSERT(objInfo.size() == 16);

    // Transmit
    evbuffer_add(buf, objInfo.data(), objInfo.size());
    if (includePayload) {
	evbuffer_add_reference(buf, payload->data(), payload->size(),
			       Httpd_stringDeleteCB, payload);
    }

    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);

    return;
}*/

void
Httpd_getObjInfo(struct evhttp_request *req, void *arg)
{
    string url = evhttp_request_get_uri(req);
    string sObjId;
    if (url.substr(0, 9) == "/objinfo/") {
	sObjId = url.substr(9);
    }
    else {
	evhttp_send_error(req, HTTP_NOTFOUND, "File Not Found");
	return;
    }
    
    if (sObjId.size() != 64) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
        return;
    }
    
    LOG("httpd: getobjinfo %s", sObjId.c_str());

    Object::sp obj = repository.getObject(ObjectHash::fromHex(sObjId));
    if (obj == NULL) {
        evhttp_send_reply(req, HTTP_NOTFOUND, "Object Not Found", evbuffer_new());
        return;
    }

    ObjectInfo objInfo = obj->getInfo();

    // Transmit
    evbufwstream es;
    es.writeInfo(objInfo);

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", es.buf());
}

void
Httpd_getObjs(struct evhttp_request *req, void *arg)
{
    // Get object hashes
    evbuffer *buf = evhttp_request_get_input_buffer(req);
    evbufstream in(buf);

    uint32_t numObjs = in.readUInt32();
    fprintf(stderr, "Transmitting %u objects\n", numObjs);
    std::vector<ObjectHash> objs;
    for (size_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);
        objs.push_back(hash);
    }


    // Transmit
    evbufwstream out;
    repository.transmit(&out, objs);

    evhttp_add_header(req->output_headers, "Content-Type",
            "application/octet-stream");
    evhttp_send_reply(req, HTTP_OK, "OK", out.buf());
}

// void
// Httpd_pushObj(struct evhttp_request *req, void *arg)
// {
//     const char *cl = evhttp_find_header(req->input_headers, "Content-Length");
//     uint32_t length;
//     struct evbuffer *body = evhttp_request_get_input_buffer(req);
//     struct evbuffer *outbuf;
//     unsigned char objId[20];
//     void *buf;
// 
//     outbuf = evbuffer_new();
//     if (outbuf == NULL) {
//         LOG("couldn't allocate evbuffer!");
//         evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
//         return;
//     }
// 
//     // Make sure it makes sense
//     if (cl == NULL) {
//         evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
//         return;
//     }
// 
//     /*
//     Dump Headers
// 
//     struct evkeyvalq *kv = evhttp_request_get_input_headers(req);
//     struct evkeyval *iter;
//     TAILQ_FOREACH(iter, kv, next) {
//         printf("%s: %s\n", iter->key, iter->value);
//     }
//     */
// 
//     length = atoi(cl);
//     printf("Push: %d bytes, got %zd bytes\n", length, evbuffer_get_length(body));
// 
//     buf = malloc(length);
//     evbuffer_remove(body, buf, length);
//     // XXX: Add object
//     free(buf);
// 
//     evbuffer_add_printf(outbuf, "%02x%02x%02x%02x"
//                         "%02x%02x%02x%02x%02x%02x%02x%02x"
//                         "%02x%02x%02x%02x%02x%02x%02x%02x\n",
//                         objId[0], objId[1], objId[2], objId[3],
//                         objId[4], objId[5], objId[6], objId[7],
//                         objId[8], objId[9], objId[10], objId[11],
//                         objId[12], objId[13], objId[14], objId[15],
//                         objId[16], objId[17], objId[18], objId[19]);
//     evhttp_send_reply(req, HTTP_OK, "OK", outbuf);
// }
 
void
Httpd_logCB(int severity, const char *msg)
{
#ifdef DEBUG
    const char *sev[] = {
        "Debug", "Msg", "Warning", "Error",
    };
    LOG("%s: %s", sev[severity], msg);
#endif
}

void
Httpd_main(uint16_t port)
{
    base = event_base_new();
    event_set_log_callback(Httpd_logCB);

    httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, "0.0.0.0", port);

    evhttp_set_cb(httpd, "/id", Httpd_getId, NULL);
    evhttp_set_cb(httpd, "/version", Httpd_getVersion, NULL);
    evhttp_set_cb(httpd, "/HEAD", Httpd_head, NULL);
    evhttp_set_cb(httpd, "/index", Httpd_getIndex, NULL);
    evhttp_set_cb(httpd, "/commits", Httpd_getCommits, NULL);
    //evhttp_set_cb(httpd, "/objs", Httpd_pushobj, NULL);
#ifdef DEBUG
    evhttp_set_cb(httpd, "/stop", Httpd_stop, NULL);
#endif
    evhttp_set_cb(httpd, "/getobjs", Httpd_getObjs, NULL);
    // Generic handler provides:
    // getObject: /objs/*
    // getObjectInfo: /objinfo/*
    evhttp_set_gencb(httpd, Httpd_getObjInfo, NULL);

#if !defined(WITHOUT_MDNS)
    // mDNS
    MDNS_Register(port);
#endif

    event_base_dispatch(base);
    evhttp_free(httpd);
}

void
usage()
{
    printf("Ori Distributed Personal File System (%s) - HTTP Server\n\n",
            ORI_VERSION_STR);
    cout << "usage: ori_httpd [options] <repository path>" << endl << endl;
    cout << "Options:" << endl;
    cout << "-p port    Set the HTTP port number (default 8080)" << endl;
    cout << "-h         Show this message" << endl;
}

int
main(int argc, char *argv[])
{
    bool success;
    int ch;
    unsigned long port = 8080;

    while ((ch = getopt(argc, argv, "p:h")) != -1) {
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

    if (argc == 1) {
        success = repository.open(argv[0]);
    } else {
        success = repository.open();
    }
    if (!success) {
        cout << "Could not open the local repository!" << endl;
        cout << "Run ori_httpd -h for help" << endl;
        return 1;
    }

    ori_open_log(repository.getLogPath());
    LOG("libevent %s", event_get_version());

    Httpd_main(port);

    return 0;
}

