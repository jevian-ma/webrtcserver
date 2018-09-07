#include <stdio.h>
#include <microhttpd.h>
#include <string.h>
#include "secmalloc.h"
#include "httpd.h"
#include "main.h"

int ahc_echo(void *cls,
            struct MHD_Connection * connection,
            const char * url,
            const char * method,
            const char * version,
            const char * upload_data,
            size_t *upload_data_size,
            void ** ptr) {
    char *html = *ptr;
    if (html == NULL) {
        if((html = (char*)secmalloc(4096)) == NULL) {
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
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(html), (void*) html, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, "Content-Type", "text/plain");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    secfree(html);
    return ret;
}

int starthttpd(uint16_t port) {
    struct MHD_Daemon *d = MHD_start_daemon(MHD_USE_EPOLL_INTERNALLY, port, NULL, NULL, &ahc_echo, NULL, MHD_OPTION_END);
    if (d == NULL) {
        return -1;
    }
    return 0;
}