#include <stdio.h>
#include <microhttpd.h>
#include <string.h>
#include <malloc.h>
#include <jansson.h>
#include "httpd.h"
#include "main.h"

extern char _binary_webrtcserver_min_js_start;

int connectionHandler(void *cls,
            struct MHD_Connection * connection,
            const char * url,
            const char * method,
            const char * version,
            const char * upload_data,
            size_t *upload_data_size,
            void ** ptr) {
    if (strcmp(method, "POST") == 0) {
        char *html = *ptr;
        if (html == NULL) {
            if((html = (char*)malloc(4096)) == NULL) { // 这里使用malloc而非secmallo，因为生成的对象由libmircohttpd协助释放
                printf("malloc fail!!!, in %s, at %d\n", __FILE__, __LINE__);
                return MHD_NO;
            }
            *ptr = html;
            return MHD_YES;
        }
        if (*upload_data_size != 0) {
            *upload_data_size = 0;
            handlerequeset (url, upload_data, html);
            return MHD_YES;
        }
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(html), (void*) html, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "text/plain");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    } else {
        char *html;
        if (strcmp(url, "/webrtcserver.min.js") == 0) {
            html = &_binary_webrtcserver_min_js_start;
        } else {
            html = "";
        }
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(html), (void*) html, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "expires", "-1");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
}

int starthttpd() {
    json_t *configobj;
    if (access("config.json", R_OK) == 0) {
        json_error_t error;
        configobj = json_load_file("config.json", 0, &error);
        if (configobj == NULL) {
            printf("json error on line %d: %s, in %s, at %d\n", error.line, error.text, __FILE__, __LINE__);
        }
    } else if (access("/etc/webrtcserver/config.json", R_OK) == 0) {
        json_error_t error;
        configobj = json_load_file("/etc/webrtcserver/config.json", 0, &error);
        if (configobj == NULL) {
            printf("json error on line %d: %s, in %s, at %d\n", error.line, error.text, __FILE__, __LINE__);
        }
    }
    json_t *http_portobj = json_object_get (configobj, "http_port");
    uint16_t http_port = json_integer_value (http_portobj);
    json_decref (configobj);
    struct MHD_Daemon *d = MHD_start_daemon(MHD_USE_EPOLL_INTERNALLY, http_port, NULL, NULL, &connectionHandler, NULL, MHD_OPTION_END);
    if (d == NULL) {
        return -1;
    }
    return 0;
}