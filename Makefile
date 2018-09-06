CC = gcc
FLAGS = -lmicrohttpd -ljansson `pkg-config --cflags --libs nice`

webrtcserver: main.o httpd.o secmalloc.o
	$(CC) -o $@ $^ $(FLAGS)

main.o: main.c httpd.h
	$(CC) -c -o $@ main.c

httpd.o: httpd.c httpd.h secmalloc.h
	$(CC) -c -o $@ httpd.c

secmalloc.o: secmalloc.c secmalloc.h
	$(CC) -c -o $@ secmalloc.c

clean:
	rm *.o
