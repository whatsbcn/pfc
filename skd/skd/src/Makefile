CC  =   ../../dietlibc/bin/diet gcc
CFLAGS = -Wall -fno-builtin -O2 -I../include -fomit-frame-pointer -nostdinc -DNODIETREF -I../../dietlibc/include $(EXTRA_CFLAGS)
LDFLAGS = -s

all: config client launcher

config: config.c sha1.o 
	$(CC) $(CFLAGS) $(LDFLAGS) config.c sha1.o -o config

client: client.c ../include/config.h sha1.o raw.o
	$(CC) $(CFLAGS) $(LDFLAGS) client.c sha1.o raw.o -o client

launcher: launcher.c ../include/config.h raw.o
	$(CC) $(CFLAGS) $(LDFLAGS) launcher.c raw.o -o launcher

../include/config.h: config
	./config > ../include/config.h

sha1.o: sha1.c ../include/sha1.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c sha1.c -o sha1.o

raw.o: raw.c ../include/raw.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c raw.c -o raw.o

cleanall:
	rm -f *.o ../include/config.h launcher client config *.o

clean:
	rm -f *.o launcher client config *.o
	
