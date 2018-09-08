#include <stdio.h>
#include <malloc.h>
#include <pthread.h>

pthread_mutex_t secmalloc_mutex = PTHREAD_MUTEX_INITIALIZER;
int malloctime = 0;

void *secmalloc(size_t num) {
    pthread_mutex_lock(&secmalloc_mutex);
    malloctime++;
    pthread_mutex_unlock(&secmalloc_mutex);
    return malloc(num);
}

void secfree(void *p) {
    pthread_mutex_lock(&secmalloc_mutex);
    malloctime--;
    pthread_mutex_unlock(&secmalloc_mutex);
    free(p);
}

void showmalloctime(char *filename, int line) {
    printf("malloctime:%d,in %s, at %d\n", malloctime, filename, line);
}