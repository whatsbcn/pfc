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


#define VN 0x04
#define CONNECT 0x01
#define BIND	0x02
#define USERID	"pdflush"
#define DEBUG 	1
#define BUFSIZE 256

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
	printf("VN: %x\n", m->vn);
	printf("CD: %x\n", m->cd);
	printf("DSTOPRT: %d\n", ntohs(m->dstport));
	printf("VN: %x\n", m->dstip);
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
        printf("Received connection!\n");
        if (!fork()) {
            close(sock);
			socksd(sock_con);
			exit(0);
        }
        close(sock_con);
    }
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

void socks_forward(int sock1, int sock2) {
	char buf[BUFSIZE];
    while (1) {
        fd_set  fds;
        int count;

        // put the fd to watch
        FD_ZERO(&fds);
        FD_SET(sock1, &fds);
        FD_SET(sock2, &fds);

        if (select(sock1  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

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
}

void socksd(int sock) {
	struct message req;
	char userid[64];
	read(sock, &req, sizeof(struct message));
	char buf[4];
	read(sock, buf, 4);
	print_message(&req);
	printf("USERID: %s\n", buf);

	int sock2;
	if ((sock2 = launcher_rcon(req.dstip, req.dstport)) > 0) {
		debug("Connected!\n");
		req.cd = 90;
		debug("Enviant resposta\n");
		write(sock, &req, sizeof(req));
		socks_forward(sock, sock2);
	} else {
		debug("ERROR!\n");
		req.cd = 91;
		debug("Enviant resposta\n");
		write(sock, &req, sizeof(req));
	} 
	close(sock);
	close(sock2);
}

int main(int argc, char *argv[]) {
	tcp_daemon(9999);
	return 0;	
}
