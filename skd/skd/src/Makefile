CC  =   ../../dietlibc/bin/diet gcc
CFLAGS = -Wall -fno-builtin -O2 -I../include -fomit-frame-pointer -nostdinc -DNODIETREF -I../../dietlibc/include $(EXTRA_CFLAGS)
LDFLAGS = -s

config: config.c
	$(CC) $(CFLAGS) $(LDFLAGS) config.c -o config

../include/config.h: config
	./config > ../include/config.h

clean:
	rm -f *.o ../include/config.h
