/*
 * First proof of concept of antidebugging techniques.
 */

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"
#include "common.h"


__inline__ void antidebug1() {
    int status;
#if ANTIDEBUG    
    if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
        debug("Antidebug1 reached!\n");
        exit(0);
    }
    waitpid(-1, &status,0);
#endif
}

#if 0
void handler(int n)
{
}

__inline__ void antidebug2() {
#if ANTIDEBUG
    signal(handler, SIGTRAP);
    __asm__("int3");
    signal(SIG_DFL, SIGTRAP);
#endif
}
#endif

__inline__ void antidebug3() {
    __asm__("jmp antidebug3_1 + 2\n"
            "antidebug3_1:\n"
            ".short 0xc606\n"
            );
}

// one time tests
__inline__ void antidebug4(char *argv0) {
#if ANTIDEBUG
    // No more fd opened
    if( close(3) != -1) {
        exit(0);
    }

    // Var _ not modified
    if(strcmp(argv0, (char *)getenv("_"))) {
        exit(0);
    }

#endif
}

// Checksum the functions in memory and check that they are not modified.
// If a breakpoint is added, then the checksum will fail.


