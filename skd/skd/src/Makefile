CC  =   ../../dietlibc/bin/diet gcc
CFLAGS = -Wall -fno-builtin -O2 -I../include -fomit-frame-pointer -nostdinc -DNODIETREF -I../../dietlibc/include $(EXTRA_CFLAGS)
LDFLAGS = -s

all: config client launcher

config: config.c
	$(CC) $(CFLAGS) $(LDFLAGS) config.c -o config

client: client.c ../include/config.h
	$(CC) $(CFLAGS) $(LDFLAGS) client.c -o client

launcher: launcher.c ../include/config.h
	$(CC) $(CFLAGS) $(LDFLAGS) launcher.c -o launcher

../include/config.h: config
	./config > ../include/config.h

cleanall:
	rm -f *.o ../include/config.h launcher client config

clean:
	rm -f *.o launcher client config
	
