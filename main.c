#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "httpd.h"
#include "ice.h"

void handlerequeset (const char *url, const char *json, char *res) {
    json_error_t err;
    json_t *obj = json_loads(json, 0, &err);
    if (obj == NULL) {
        strcpy(res, "{\"errcode\":-1,\"errmsg\":\"json parse fail\"}");
        return;
    }
    json_t *actobj = json_object_get (obj, "act");
    const char *act = json_string_value(actobj);
    if (strcmp(act, "createroom") == 0) {
        createicd (obj);
        strcpy(res, "{\"errcode\":0,\"errmsg\":\"success\"}");
    } else if (strcmp(act, "enterroom") == 0) {
        ;
    }
    json_decref (obj);
}

int main(int argc, char *argv[]) {
    networkinginit();
    starthttpd(80);
    getchar();
}
