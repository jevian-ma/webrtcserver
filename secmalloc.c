#include "secmalloc.h"

#ifdef DEBUG

#include <malloc.h>
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int malloctime = 0;

void *secmalloc(size_t num) {
    pthread_mutex_lock(&mutex);
    malloctime++;
    pthread_mutex_unlock(&mutex);
    return malloc(num);
}

void secfree(void *p) {
    pthread_mutex_lock(&mutex);
    malloctime--;
    pthread_mutex_unlock(&mutex);
    free(p);
}

void showmalloctime() {
    printf("malloctime:%d\n", malloctime);
}

#endif