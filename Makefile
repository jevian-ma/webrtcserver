
webrtcserver: main.o httpd.o secmalloc.o ice.o js.o rtp.o
	gcc -o $@ $^ -lmicrohttpd -ljansson `pkg-config --cflags --libs nice`

main.o: main.c httpd.h ice.h
	gcc -c -o $@ main.c

httpd.o: httpd.c httpd.h main.h
	gcc -c -o $@ httpd.c

secmalloc.o: secmalloc.c secmalloc.h
	gcc -c -o $@ secmalloc.c

ice.o: ice.c secmalloc.h rtp.h
	gcc -c -o $@ ice.c `pkg-config --cflags --libs nice`

rtp.o: rtp.c rtp.h
	gcc -c -o $@ rtp.c

js.o: webrtcserver.min.js
	objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 $^ $@

webrtcserver.min.js: webrtcserver.js
	uglifyjs $^ -m -o $@

clean:
	rm *.o *.min.js
