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
        printf("%s, in %s, at %d\n", res, __FILE__, __LINE__);
        return;
    }
    json_t *actobj = json_object_get (obj, "act");
    const char *act = json_string_value(actobj);
    if (strcmp(act, "createliveroom") == 0) {
        createliveroom (obj, res);
    } else if (strcmp(act, "enterliveroom") == 0) {
        ;
    } else {
        strcpy(res, "{\"errcode\":-2,\"errmsg\":\"act is unknown\"}");
        printf("%s, in %s, at %d\n", res, __FILE__, __LINE__);
    }
    json_decref (obj);
}

int main(int argc, char *argv[]) {
    networkinginit();
    starthttpd(80);
    createicd();
}
