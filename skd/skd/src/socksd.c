#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../include/config.h"
#include "../include/actions.h"

#define VN 0x04
#define CONNECT 0x01
#define BIND	0x02

struct message {
	unsigned char vn;
	unsigned char cd;
	unsigned short dstport;
	unsigned int dstip;
};

// Debug function
__inline__ void debug(char * format, ...){
#if DEBUG
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
}

void print_message(struct message *m) {
#if DEBUG
    struct in_addr in;
    in.s_addr = m->dstip;
	debug("SOCKSv%d ", m->vn);
    if (m->cd == 1) {
	    debug("CONNECT to %s:%d\n", inet_ntoa(in), ntohs(m->dstport));
    } else if (m->cd == 2) {
	    debug("BIND %s:%d\n", inet_ntoa(in), ntohs(m->dstport));
    } else {
        debug("INVALID COMMAND %d\n", m->cd);
    }
#endif
}

int launcher_rcon(unsigned long ip, unsigned short port) {
    int sock;
    struct sockaddr_in cli;

    sock = socket(AF_INET, SOCK_STREAM, 6);
    if (sock < 0) exit(-1);

    memset(&cli, 0, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_port = port;
    cli.sin_addr.s_addr = ip;

    if (connect(sock, (struct sockaddr *) &cli, sizeof(cli)) < 0) {
        close(sock);
        debug("Failed to connect to destination port %d\n", port);
        exit(-1);
    }
	perror("connect");

    return sock;
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

void *pthread_socks(void *sock) {
	struct message req;
	char userid[64];
	read((int)sock, &req, sizeof(struct message));
	char buf[BUFSIZE];
	read((int)sock, buf, 4);
	print_message(&req);

    // Only socksv4 and connect command are implemented
    if (req.vn == VN && req.cd == CONNECT) { 
	    int sock2;
	    if ((sock2 = launcher_rcon(req.dstip, req.dstport)) > 0) {
	    	req.cd = 90;
	    	write((int)sock, &req, sizeof(req));
            // Forwarding traffic
            while (1) {
                fd_set  fds;
                int count;

                // put the fd to watch
                FD_ZERO(&fds);
                FD_SET(sock1, &fds);
                FD_SET(sock2, &fds);

                if (select(max(sock1, sock2)  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

                /* stdin => shell */
                if (FD_ISSET(sock1, &fds)) {
                    count = read(sock1, buf, BUFSIZE);
                    if ((count <= 0) && (errno != EINTR)) break;
                    if (write(sock2, buf, count) <= 0 && (errno != EINTR)) break;    
                    /* shell => stdout */
                } 
	        
	        	if (FD_ISSET(sock2, &fds)) {
                    count = read(sock2, buf, BUFSIZE);
                    // TODO: enviar char especial per setejar tamany tty, timeout, etc.
                    if ((count <= 0) && (errno != EINTR)) break;
                    else if (write(sock1, buf, count) <= 0 && (errno != EINTR)) break;
                }
            }
	    } else {
	    	req.cd = 91;
	    	write((int)sock, &req, sizeof(req));
	    } 
	    close(sock2);
    }
	close((int)sock);
    pthread_exit(NULL);
}

void tcp_daemon(int port) {
    int sock, sock_con, one = 1, bytes;
    struct sockaddr_in tcp;
    struct sockaddr_in cli;
    unsigned int slen = sizeof(cli);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        debug("Can't allocate tcp socket\n");
        exit(-1);
    }

    memset((char *) &tcp, 0, sizeof(tcp));
    tcp.sin_family = AF_INET;
    tcp.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp.sin_port = htons(port);

    // We want to reuse the source port port (avoiding bind errors)
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int)) < 0)
        debug("Error setsockopt: reuseaddr\n");

    if (bind(sock, (struct sockaddr *) &tcp, sizeof(tcp)) < 0) {
        debug("Error: bind\n");
        exit(-1);
    }

    if (listen(sock, 1) < 0) {
        debug("Error: listen\n");
        exit(-1);
    }

    debug("Listening to port %d\n", port);
    while (1) {
        sock_con = accept(sock, (struct sockaddr *) &cli, &slen);
        if (sock_con < 0) {
            debug("Error: accept\n");
            exit(-1);
        }
        debug("Received connection!\n");
        pthread_t thread;
        pthread_create(&thread, 0, pthread_socks, (void *)sock_con);
    }
}



int main(int argc, char *argv[]) {
	tcp_daemon(9999);
    pthread_exit(NULL);
	return 0;	
}

