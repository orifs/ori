
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

#include "debug.h"
#include "localrepo.h"
#include "util.h"
#include "zeroconf.h"

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
        printf("Failed");
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
        LOG("httpd_gethead: evbuffer_new failed!");
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
        LOG("httpd_getversion: evbuffer_new failed!");
        return;
    }

    evbuffer_add_printf(buf, "%s", ver.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_head(struct evhttp_request *req, void *arg)
{
    string headId = repository.getHead();
    struct evbuffer *buf;

    LOG("httpd: gethead");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("httpd_gethead: evbuffer_new failed!");
        return;
    }

    evbuffer_add_printf(buf, "%s", headId.c_str());
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_getIndex(struct evhttp_request *req, void *arg)
{
    set<ObjectInfo> objs = repository.listObjects();
    set<ObjectInfo>::iterator it;
    struct evbuffer *buf;

    LOG("httpd: getindex");

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("httpd_getindex: evbuffer_new failed!");
        return;
    }

    for (it = objs.begin(); it != objs.end(); it++) {
        int status;
        string objInfo = (*it).getInfo();

        LOG("hash = %s\n", (*it).hash.c_str());

        status = evbuffer_add(buf, (*it).hash.c_str(), (*it).hash.size());
        if (status != 0) {
            assert(status == -1);
            LOG("evbuffer_add failed while adding hash!");
            evbuffer_free(buf);
            buf = evbuffer_new();
            evhttp_send_reply(req, HTTP_INTERNAL, "INTERNAL ERROR", buf);
            return;
        }
        status = evbuffer_add(buf, objInfo.data(), objInfo.size());
        if (status != 0) {
            assert(status == -1);
            LOG("evbuffer_add failed while adding objInfo!");
            evbuffer_free(buf);
            buf = evbuffer_new();
            evhttp_send_reply(req, HTTP_INTERNAL, "INTERNAL ERROR", buf);
            return;
        }
    }
    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
Httpd_stringDeleteCB(const void *data, size_t len, void *extra)
{
    string *p = (string *)extra;

    delete p;
}

void
Httpd_getObj(struct evhttp_request *req, void *arg)
{
    string objId;
    auto_ptr<bytestream> bs;
    string *payload = new string();
    Object *obj;
    string objInfo;
    struct evbuffer *buf;

    objId = evhttp_request_get_uri(req);
    objId = objId.substr(6);

    buf = evbuffer_new();
    if (buf == NULL) {
        LOG("httpd_getobj: evbuffer_new failed!");
    }

    if (objId.size() != 64) {
        evhttp_send_reply(req, HTTP_BADREQUEST, "Bad Request", buf);
        return;
    }
    
    LOG("httpd: getobj %s", objId.c_str());

    obj = repository.getObject(objId.c_str());
    if (obj == NULL) {
        evhttp_send_reply(req, HTTP_NOTFOUND, "Object Not Found", buf);
        return;
    }

    bs = obj->getStoredPayloadStream();
    *payload = bs->readAll();
    objInfo = obj->getInfo().getInfo();

    assert(objInfo.size() == 16);

    // Transmit
    evbuffer_add(buf, objInfo.data(), objInfo.size());
    evbuffer_add_reference(buf, payload->data(), payload->size(),
                           Httpd_stringDeleteCB, payload);

    evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);

    delete obj;
    return;
}

void
Httpd_pushObj(struct evhttp_request *req, void *arg)
{
    const char *cl = evhttp_find_header(req->input_headers, "Content-Length");
    uint32_t length;
    struct evbuffer *body = evhttp_request_get_input_buffer(req);
    struct evbuffer *outbuf;
    unsigned char objId[20];
    void *buf;

    outbuf = evbuffer_new();
    if (outbuf == NULL) {
        printf("Failed");
    }

    // Make sure it makes sense
    if (cl == NULL) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
        return;
    }

    /*
    Dump Headers

    struct evkeyvalq *kv = evhttp_request_get_input_headers(req);
    struct evkeyval *iter;
    TAILQ_FOREACH(iter, kv, next) {
        printf("%s: %s\n", iter->key, iter->value);
    }
    */

    length = atoi(cl);
    printf("Push: %d bytes, got %zd bytes\n", length, evbuffer_get_length(body));

    buf = malloc(length);
    evbuffer_remove(body, buf, length);
    // XXX: Add object
    free(buf);

    evbuffer_add_printf(outbuf, "%02x%02x%02x%02x"
                        "%02x%02x%02x%02x%02x%02x%02x%02x"
                        "%02x%02x%02x%02x%02x%02x%02x%02x\n",
                        objId[0], objId[1], objId[2], objId[3],
                        objId[4], objId[5], objId[6], objId[7],
                        objId[8], objId[9], objId[10], objId[11],
                        objId[12], objId[13], objId[14], objId[15],
                        objId[16], objId[17], objId[18], objId[19]);
    evhttp_send_reply(req, HTTP_OK, "OK", outbuf);
}

void
Httpd_logCB(int severity, const char *msg)
{
    const char *sev[] = {
        "Debug", "Msg", "Warning", "Error",
    };
    LOG("%s: %s", sev[severity]);
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
    //evhttp_set_cb(httpd, "/objs", Httpd_pushobj, NULL);
    evhttp_set_cb(httpd, "/stop", Httpd_stop, NULL);
    evhttp_set_gencb(httpd, Httpd_getObj, NULL); // getObj: /objs/*

    // mDNS
    struct event *mdns_evt = MDNS_Start(port, base);
    if (mdns_evt)
        event_add(mdns_evt, NULL);

    event_base_dispatch(base);
    if (mdns_evt)
        event_free(mdns_evt);
    evhttp_free(httpd);
}

void
usage()
{
    cout << "usage: ori_httpd [options] <repository path>" << endl << endl;
    cout << "Options:" << endl;
    cout << "-p port    Set the HTTP port number (default 8080)" << endl;
    cout << "-h         Show this message" << endl;
}

int
main(int argc, char *argv[])
{
    bool success;
    int bflag, ch;
    unsigned long port = 8080;

    bflag = 0;
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
        return 1;
    }

    ori_open_log(&repository);

    Httpd_main(port);

    return 0;
}

