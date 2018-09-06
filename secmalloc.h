#ifndef __SECMALLOC_H__
#define __SECMALLOC_H__

// #define DEBUG

#ifdef DEBUG
#include <stdio.h>

void *secmalloc(size_t num);
void secfree(void *p);
void showmalloctime();
#else
#include <malloc.h>

#define secmalloc(x) malloc(x)
#define secfree(x)   free(x)
#define showmalloctime()
#endif

#endif