
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

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

static struct evhttp *httpd;
/* set if a test needs to call loopexit on a base */
static struct event_base *base;

int httpd_authenticate(struct evhttp_request *req, struct evbuffer *buf)
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

void httpd_stop(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;
    buf = evbuffer_new();
    if (buf == NULL) {
        printf("Failed");
    }

    if (httpd_authenticate(req, buf) < 0)
    {
        return;
    }

    evbuffer_add_printf(buf, "Stopping\n");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    event_base_loopexit(base, NULL);
}

void httpd_getid(struct evhttp_request *req, void *arg)
{
}

void httpd_getversion(struct evhttp_request *req, void *arg)
{
}

void httpd_head(struct evhttp_request *req, void *arg)
{
}

void httpd_getindex(struct evhttp_request *req, void *arg)
{
}

void httpd_getobj(struct evhttp_request *req, void *arg)
{
}

void httpd_pushobj(struct evhttp_request *req, void *arg)
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

void httpd_generic(struct evhttp_request *req, void *arg)
{
    int fd;
    struct stat sb;
    struct evbuffer *buf;
    buf = evbuffer_new();
    if (buf == NULL) {
        printf("Failed");
    }

    fd = open(evhttp_request_get_uri(req) + 1, O_RDONLY);
    if (fd < 0) {
        evbuffer_add_printf(buf, "File not found %s\n",
                            evhttp_request_get_uri(req));
        evhttp_send_reply(req, HTTP_NOTFOUND, "File Not Found", buf);
        return;
    }

    fstat(fd, &sb);
    if (S_ISREG(sb.st_mode)) {
        evhttp_add_header(req->output_headers, "Content-Type", "text/plain");
        evbuffer_add_file(buf, fd, 0, sb.st_size);
    } else if (S_ISDIR(sb.st_mode)) {
        evbuffer_add_printf(buf, "Directory\n");
    } else {
        evbuffer_add_printf(buf, "Something else\n");
    }
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void httpd_logcb(int severity, const char *msg)
{
    const char *sev[] = {
        "Debug", "Msg", "Warning", "Error",
    };
    printf("%s: %s\n", sev[severity], msg);
}

int main(int argc, char *argv[])
{
    base = event_base_new();
    event_set_log_callback(httpd_logcb);

    // XXX: Open repository

    httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, "0.0.0.0", 8080);

    evhttp_set_cb(httpd, "/id", httpd_getid, NULL);
    evhttp_set_cb(httpd, "/version", httpd_getid, NULL);
    evhttp_set_cb(httpd, "/HEAD", httpd_head, NULL);
    evhttp_set_cb(httpd, "/index", httpd_getindex, NULL);
    evhttp_set_cb(httpd, "/objs/*", httpd_getobj, NULL);
    evhttp_set_cb(httpd, "/objs", httpd_pushobj, NULL);
    evhttp_set_cb(httpd, "/stop", httpd_stop, NULL);
    evhttp_set_gencb(httpd, httpd_generic, NULL);

    event_base_dispatch(base);
    evhttp_free(httpd);
}

