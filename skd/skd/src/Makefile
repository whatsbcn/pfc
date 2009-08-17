CC  =   ../../dietlibc/bin/diet gcc
CFLAGS = -Wall -fno-builtin -Os -I../include -fomit-frame-pointer -nostdinc -DNODIETREF -I../../dietlibc/include $(EXTRA_CFLAGS) 
LDFLAGS = -s

all: config client launcher

common.o: common.c ../include/config.h ../include/common.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c common.c -o common.o

antidebug.o: antidebug.c ../include/config.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c antidebug.c -o antidebug.o

rc4.o: rc4.c ../include/rc4.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c rc4.c -o rc4.o

config: config.c sha1.o 
	$(CC) $(CFLAGS) $(LDFLAGS) config.c sha1.o -o config

client: client.c common.o ../include/config.h sha1.o raw.o rc4.o
	$(CC) $(CFLAGS) $(LDFLAGS) client.c sha1.o common.o raw.o rc4.o -o client

launcher: launcher.c common.o antidebug.o ../include/config.h raw.o rc4.o
	$(CC) $(CFLAGS) $(LDFLAGS) launcher.c raw.o common.o antidebug.o rc4.o -o launcher

../include/config.h: config
	./config > ../include/config.h

sha1.o: sha1.c ../include/sha1.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c sha1.c -o sha1.o

raw.o: raw.c ../include/raw.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c raw.c -o raw.o

cleanall:
	rm -f *.o ../include/config.h launcher client config *.o

clean:
	rm -f *.o launcher client config
	
