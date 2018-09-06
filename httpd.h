#ifndef __HTTPD_H__
#define __HTTPD_H__

#include "stdint.h"

typedef void (*CallBackHttpdRequest) (const char *url, const char *json, char *str);
int starthttpd(uint16_t port, CallBackHttpdRequest cbhr);

#endif