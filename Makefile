CC = gcc
FLAGS = -lmicrohttpd `pkg-config --cflags --libs nice`

webrtcserver: mircohttpd.o secmalloc.o
	$(CC) -o $@ $^ $(FLAGS)

mircohttpd.o: mircohttpd.c secmalloc.h
	$(CC) -c -o $@ mircohttpd.c

secmalloc.o: secmalloc.c secmalloc.h
	$(CC) -c -o $@ secmalloc.c

clean:
	rm *.o