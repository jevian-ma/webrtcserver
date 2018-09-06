#include <microhttpd.h>
#include <string.h>
#include "secmalloc.h"

struct POSTDATALIST {
    char *key;
    char *data;
    struct POSTDATALIST *tail;
};

struct Request {
    struct MHD_PostProcessor *pp;
    struct POSTDATALIST *postdatalist;
};

int post_iterator (void *cls,
	       enum MHD_ValueKind kind,
	       const char *key,
	       const char *filename,
	       const char *content_type,
	       const char *transfer_encoding,
	       const char *data,
           uint64_t off,
           size_t size) {
    struct POSTDATALIST *postdata = (struct POSTDATALIST*)secmalloc(sizeof(struct POSTDATALIST));
    postdata->key = (char*)secmalloc(strlen(key) + 1);
    strcpy(postdata->key, key);
    postdata->data = (char*)secmalloc(strlen(data) + 1);
    strcpy(postdata->data, data);
    postdata->tail = NULL;
    struct Request *rq = cls;
    struct POSTDATALIST *postdatalist = rq->postdatalist;
    if (postdatalist == NULL) {
        postdatalist = postdata;
        rq->postdatalist = postdatalist;
    } else {
        struct POSTDATALIST *p = postdatalist;
        while (p->tail != NULL) {
            p = p->tail;
        }
        p->tail = postdata;
    }
    return MHD_YES;
}

int ahc_echo(void * cls,
            struct MHD_Connection * connection,
            const char * url,
            const char * method,
            const char * version,
            const char * upload_data,
            size_t *upload_data_size,
            void ** ptr) {
    if (strcmp (method, MHD_HTTP_METHOD_POST) == 0) {
        struct Request *rq = *ptr;
        if (rq == NULL) {
            rq = (struct Request*)secmalloc(sizeof(struct Request));
            rq->postdatalist = NULL;
            rq->pp = MHD_create_post_processor (connection, 1024, &post_iterator, rq);
            *ptr = rq;
            return MHD_YES;
        }
        MHD_post_process (rq->pp, upload_data, *upload_data_size);
        if (*upload_data_size != 0) {
            *upload_data_size = 0;
            return MHD_YES;
        }
        MHD_destroy_post_processor (rq->pp);
        struct POSTDATALIST *p = rq->postdatalist;
#ifdef DEBUG
        printf ("{");
#endif
        while (p != NULL) {
            struct POSTDATALIST *temp = p;
#ifdef DEBUG
            printf ("\"%s\":\"%s\"", p->key, p->data);
#endif
            p = p->tail;
#ifdef DEBUG
            if (p != NULL) {
                printf (",");
            }
#endif
            secfree(temp->key);
            secfree(temp->data);
            secfree(temp);
        }
#ifdef DEBUG
        printf ("}\n");
#endif
        secfree(rq);
    }
    char page[] = "hello world!!";
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(page), (void*) page, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, "Content-Type", "text/plain");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    showmalloctime();
    return ret;
}

int main(int argc, char *argv[]) {
    showmalloctime();
    struct MHD_Daemon *d = MHD_start_daemon(MHD_USE_EPOLL_INTERNALLY, 80, NULL, NULL, &ahc_echo, NULL, MHD_OPTION_END);
    if (d == NULL) {
        printf("error!!!");
        return -1;
    }
    getchar();
    MHD_stop_daemon(d);
    return 0;
}