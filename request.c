/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Newly allocated Request structure.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request * accept_request(int sfd) {
    Request *r;
    struct sockaddr raddr;
    socklen_t rlen = sizeof(struct sockaddr);
    int client_fd;

    r = calloc(1, sizeof(Request));

    /* Allocate request struct (zeroed) */
    r->fd = 0;
    r->file = NULL;
    r->method = NULL;
    r->uri = NULL;
    r->path = NULL;
    r->query = NULL;
    r->headers = NULL;
    /* Accept a client */
    if ((client_fd = accept(sfd, &raddr, &rlen)) < 0) {
        fprintf(stderr, "accept failed: %s\n", strerror(errno));
        goto fail;
    }
    /* Lookup client information */
    socklen_t hostlen = 0;
    socklen_t servlen = 0;
    int errcode = -1;
    if ((errcode = getnameinfo(&raddr, rlen, r->host, hostlen, r->port, servlen, 0)) != 0){
        fprintf(stderr, "getnameinfo failed %s\n", gai_strerror(errcode));
        goto fail;
    }
    /* Open socket stream */
    r->file = fdopen(client_fd, "w+");
    if (!r->file) {
        fprintf(stderr, "fdopen failed: %s\n", strerror(errno));
        close(client_fd);
        goto fail;
    }

    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    /* Deallocate request struct */
    free(r->headers);
    free(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * @param   r           Request structure.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
void free_request(Request *r) {
    if (!r) {
        return;
    }

    /* Close socket or fd */
    close(r->fd);
    fclose(r->file);

    /* Free allocated strings */
    free(r->method);
    free(r->uri);
    free(r->path);
    free(r->query);

    /* Free headers */
    if (r->headers != NULL){
        free(r->headers->next);
        free(r->headers);
    }

    /* Free request */
    free(r);
}

/**
 * Parse HTTP Request.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int parse_request(Request *r) {

    /* Parse HTTP Request Method */
    /* Parse URI */
    if (parse_request_method(r) != 0){
        //fprintf(stderr, "parse_request_method: %s\n", strerror(errno));
        //free_request(r);
        return -1;
    }

    /* Parse HTTP Requet Headers*/
    if (parse_request_headers(r) != 0){
        //fprintf(stderr, "parse_request_headers: %s\n", strerror(errno));
        return -2;
    }

    return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r) {
    char buffer[BUFSIZ];
    char *method;
    char *uri;
    char *query;

    /* Read line from socket */
    if (fgets(buffer, BUFSIZ, r->file) == NULL) {
        debug("fgets failed");
        goto fail;
    }
    /* Parse method and uri */
    method = strtok(buffer, WHITESPACE);
    if (method == NULL){
        goto fail;
    }
    uri    = strtok(NULL, WHITESPACE);
    if (uri == NULL){
        goto fail;
    }

    /* Parse query from uri */
    char *temp  = strchr(uri, '?');
    if (temp != NULL){
        query = ++temp;
        *(uri + strlen(uri) - strlen(temp) - 1) = '\0';
    } else { query = temp; }
    debug("METHOD = %s", method);
    debug("URI    = %s", uri);
    /* Record method, uri, and query in request struct */
    r->method = strdup(method);
    r->uri = strdup(uri);
    if (query != NULL){
        r->query = strdup(query);
    }

    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;

fail:
    return -1;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <VALUE>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, value = buffer.split(':')
 *      header      = new Header(name, value)
 *      headers.append(header)
 **/
int parse_request_headers(Request *r) {
    Header *curr = NULL;
    char buffer[BUFSIZ];
    char *name;
    char *value;

    /* Parse headers from socket */

    while(fgets(buffer, BUFSIZ, r->file)){
        if (buffer == NULL){
            goto fail;
        }
        if (streq(buffer,"\n") || streq(buffer,"\r\n")){
            break;
        }
        name = buffer;
        char *split = strchr(name, ':');
        if (split != NULL){
            split = skip_nonwhitespace(split);
            value = skip_whitespace(split);
            char *back = value+strlen(value)-1;
            while(value < back && isspace(*back)){
                back--;
            }
            *(back+1) = 0;
        } else { goto fail; }
        Header *temp = calloc(1, sizeof(Header));
        if (temp == NULL){
            fprintf(stderr, "calloc failed: %s\n", strerror(errno));
            goto fail;
        }
        if (value != NULL){
            temp->value = strdup(value);
        } else { goto fail; }
        if (name != NULL && value != NULL){
            *(name + strlen(name) - strlen(value) - 2) = '\0';
        }

        temp->name = strdup(name);
        temp->next = NULL;
        if (r->headers == NULL){
            r->headers = temp;
        }
        else if (r->headers->next == NULL){
            curr = temp;
            r->headers->next = curr;
        } else { curr->next = temp; }
    }


#ifndef NDEBUG
    for (struct header *header = r->headers; header != NULL; header = header->next) {
        debug("HTTP HEADER %s = %s", header->name, header->value);
    }
#endif
    return 0;

fail:
    return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
