
webrtcserver: main.o httpd.o secmalloc.o ice.o
	gcc -o $@ $^ -lmicrohttpd -ljansson `pkg-config --cflags --libs nice`

main.o: main.c httpd.h ice.h
	gcc -c -o $@ main.c `pkg-config --cflags --libs nice`

httpd.o: httpd.c httpd.h secmalloc.h main.h
	gcc -c -o $@ httpd.c

secmalloc.o: secmalloc.c secmalloc.h
	gcc -c -o $@ secmalloc.c

ice.o: ice.c ice.h secmalloc.h
	gcc -c -o $@ ice.c `pkg-config --cflags --libs nice`

clean:
	rm *.o
