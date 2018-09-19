/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

/* Internal Declarations */
HTTPStatus handle_browse_request(Request *request);
HTTPStatus handle_file_request(Request *request);
HTTPStatus handle_cgi_request(Request *request);
HTTPStatus handle_error(Request *request, HTTPStatus status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
HTTPStatus  handle_request(Request *r) {
    HTTPStatus result;

    /* Parse request */
    int i = parse_request(r);
    if (i == -1){
        fprintf(stderr, "Parse request method failed: %s\n", strerror(errno));
        result = HTTP_STATUS_BAD_REQUEST;
        goto error;
    }
    else if (i == -2){
        fprintf(stderr, "Parse request header failed: %s\n", strerror(errno));
        result = HTTP_STATUS_BAD_REQUEST;
        goto error;
    }

    /* Determine request path */
    r->path = determine_request_path(r->uri);
    if (r->path == NULL){
        fprintf(stderr, "Determining request path(%s) failed: %s\n", r->path, strerror(errno));
        result = HTTP_STATUS_NOT_FOUND;
        goto error;
    }
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    struct stat s;
    lstat(r->path, &s);
    if ((s.st_mode & S_IFMT) == S_IFDIR){
        result = handle_browse_request(r);
    }
    else if ((s.st_mode & S_IFMT) == S_IFREG){
        if (access(r->path, X_OK) == 0){
            result = handle_cgi_request(r);
        }
        else { result = handle_file_request(r); }
    }
    else {
        result = HTTP_STATUS_BAD_REQUEST;
    }

    if (result != 0){
        result = handle_error(r, result);
    }
    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    return result;

error:
    result = handle_error(r, result);
    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    return result;
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
HTTPStatus  handle_browse_request(Request *r) {
    struct dirent **entries;
    int n;

    /* Open a directory for reading or scanning */
    n = scandir(r->path, &entries, NULL, alphasort);
    if (n < 0){
        fprintf(stderr, "scandir failed: %s\n", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }
    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\r\n");
    fprintf(r->file, "Content-Type: text/html\r\n");
    fprintf(r->file, "\r\n");

    /* For each entry in directory, emit HTML list item */
    char *base = NULL;
    fprintf(r->file, "<ul>\r\n");
    for (int i = 0; i < n; i++) {
        if (!streq(entries[i]->d_name, ".")){
            if (!streq(r->uri, "/")){
                base = basename(r->path);
                fprintf(r->file, "<li><a href=\"/%s/%s\">%s</a></li>\r\n", base, entries[i]->d_name, entries[i]->d_name);
            } else { fprintf(r->file, "<li><a href=\"/%s\">%s</a></li>\r\n", entries[i]->d_name, entries[i]->d_name); }
        }
        free(entries[i]);
    }
    fprintf(r->file, "</ul>\r\n");
    free(entries);

    /* Flush socket, return OK */
    if (fflush(r->file) != 0){
        fprintf(stderr, "flush socket failed: %s\n", strerror(errno));
        return HTTP_STATUS_NOT_FOUND;
    }

    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
HTTPStatus  handle_file_request(Request *r) {
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    /* Open file for reading */
    fs = fopen(r->path, "r");
    if (!fs) {
        fprintf(stderr, "fdopen failed: %s\n", strerror(errno));
        goto fail;
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->file, "HTTP/1.0 200 OK\r\n");
    fprintf(r->file, "Content-Type: %s\r\n", mimetype);
    fprintf(r->file, "\r\n");

    /* Read from file and write to socket in chunks */
    while ((nread = fread(buffer, sizeof(char), BUFSIZ, fs))){
        fwrite(buffer, sizeof(char), nread, r->file);
    }

    /* Close file, flush socket, deallocate mimetype, return OK */
    fclose(fs);
    if (fflush(r->file) != 0){
        fprintf(stderr, "flush socket failed: %s\n", strerror(errno));
        goto fail;
    }
    free(mimetype);

    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    if (fs != NULL){
        fclose(fs);
    }
    free(mimetype);
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
HTTPStatus handle_cgi_request(Request *r) {
    FILE *pfs;
    char buffer[BUFSIZ];
    /* Export CGI environment variables from request structure:
    * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    setenv("DOCUMENT_ROOT", RootPath, 1);
    if (r->query != NULL){
       setenv("QUERY_STRING", r->query, 1);
    } else { setenv("QUERY_STRING", "", 1); }
    if (r->host != NULL){
        setenv("REMOTE_ADDR", r->host, 1);
    } else { setenv("REMOTE_ADDR", "", 1); }
    if (r->port != NULL){
        setenv("REMOTE_PORT", r->port, 1);
    } else { setenv("REMOTE_PORT", "", 1); }
    setenv("REQUEST_METHOD", r->method, 1);
    setenv("REQUEST_URI", r->uri, 1);
    setenv("SCRIPT_FILENAME", r->path, 1);
    setenv("SERVER_PORT", Port, 1);

    /* Export CGI environment variables from request headers */
    for (struct header *temp = r->headers; temp != NULL; temp = temp->next){

        if (streq(temp->name, "Host")){
            if (temp->value != NULL){
                setenv("HTTP_HOST", temp->value, 1);
            } else { setenv("HTTP_HOST", "", 1); }
        }
        else if (streq(temp->name, "Accept")){
            setenv("HTTP_ACCEPT", temp->value, 1);
        }
        else if (streq(temp->name, "Accept-Language")){
            setenv("HTTP_ACCEPT_LANGUAGE", temp->value, 1);
        }
        else if (streq(temp->name, "Accept-Encoding")){
            setenv("HTTP_ACCEPT_ENCODING", temp->value, 1);
        }
        else if (streq(temp->name, "Connection")){
            setenv("HTTP_CONNECTION", temp->value, 1);
        }
        else if (streq(temp->name, "User-Agent")){
            setenv("HTTP_USER_AGENT", temp->value, 1);
        }

    }

    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if (pfs == NULL){
        fprintf(stderr, "popen failed: %s\n", strerror(errno));
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    /* Copy data from popen to socket */
    while (fgets(buffer, BUFSIZ, pfs)){
        fputs(buffer, r->file);
    }

    /* Close popen, flush socket, return OK */
    fclose(pfs);
    if (fflush(r->file) != 0){
        fprintf(stderr, "flush socket failed: %s\n", strerror(errno));
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
HTTPStatus  handle_error(Request *r, HTTPStatus status) {
    const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.0 %s\r\n", status_string);
    fprintf(r->file, "Content-Type: %s\r\n", "text/html");
    fprintf(r->file, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->file, "<h1>%s</h1>\r\n", status_string);

    /* Return specified status */
    if (fflush(r->file) != 0){
        fprintf(stderr, "flush socket failed: %s\n", strerror(errno));
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
