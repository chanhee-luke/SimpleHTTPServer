/* forking.c: Forking HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

/**
 * Fork incoming HTTP requests to handle the concurrently.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Exit status of server (EXIT_SUCCESS).
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
int forking_server(int sfd) {

    /* Accept and handle HTTP request */
    while (true) {
      	/* Accept request */
        Request *client_request = accept_request(sfd);
        if (!client_request) {
            continue;
        }
  	     /* Ignore children */
        signal(SIGCHLD, SIG_IGN);

  	    /* Fork off child process to handle request */
        pid_t pid = fork();
        if (pid < 0) {
            debug("fork failed %s", strerror(errno));
            close(sfd);
            continue;
        }
        if (pid == 0) { // Child
            /* Handle client request */
            debug("Handling client request");
            HTTPStatus status = handle_request(client_request);
            close(sfd);
            exit(status != 0);
        }
        else {        // Parent
            free_request(client_request);
        }

    }

    /* Close server socket */
    close(sfd);
    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
