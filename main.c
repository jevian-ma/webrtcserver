#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "httpd.h"

void handlerequeset (const char *url, const char *json, char *res) {
    json_error_t err;
    json_t *obj = json_loads(json, 0, &err);
    if (obj == NULL) {
        strcpy(res, "json parse fail\n");
    }
    json_t *jevianobj = json_object_get (obj, "jevian");
    const char *jevian = json_string_value(jevianobj);
    strcpy(res, jevian);
    json_decref (obj);
}

int main(int argc, char *argv[]) {
    starthttpd(80, handlerequeset);
    getchar();
}