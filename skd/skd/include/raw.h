#ifndef RAW_H
#define RAW_h

#include "actions.h"

struct rawsock {
    unsigned short id;
    unsigned char initialized;
    int r[2];
    int w[2];
};

void send_raw(unsigned long ip, short  sport, short dport, unsigned char *buf, size_t size, int rst);
int fill_raw_connection(struct rawsock *r, unsigned  char *buf, int size);
int swapd_raw(struct rawsock *r, unsigned long ip, short sport, short dport, unsigned char *pass);

#endif
