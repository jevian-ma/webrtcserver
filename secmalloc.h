#ifndef __SECMALLOC_H__
#define __SECMALLOC_H__

void *secmalloc(size_t num);
void secfree(void *p);
void showmalloctime();

#endif
