/* spidey: Simple HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

/* Global Variables */
char *Port	      = "9898";
char *MimeTypesPath   = "/etc/mime.types";
char *DefaultMimeType = "text/plain";
char *RootPath	      = "www";

/**
 * Display usage message and exit with specified status code.
 *
 * @param   progname    Program Name
 * @param   status      Exit status.
 */
void usage(const char *progname, int status) {
    fprintf(stderr, "Usage: %s [hcmMpr]\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h            Display help message\n");
    fprintf(stderr, "    -c mode       Single or Forking mode\n");
    fprintf(stderr, "    -m path       Path to mimetypes file\n");
    fprintf(stderr, "    -M mimetype   Default mimetype\n");
    fprintf(stderr, "    -p port       Port to listen on\n");
    fprintf(stderr, "    -r path       Root directory\n");
    exit(status);
}

/**
 * Parse command-line options.
 *
 * @param   argc        Number of arguments.
 * @param   argv        Array of argument strings.
 * @param   mode        Pointer to ServerMode variable.
 * @return  true if parsing was successful, false if there was an error.
 *
 * This should set the mode, MimeTypesPath, DefaultMimeType, Port, and RootPath
 * if specified.
 */
bool parse_options(int argc, char *argv[], ServerMode *mode) {

    int argind = 1;

    while (argind < argc && strlen(argv[argind]) > 1 && argv[argind][0] == '-') {
        char *arg = argv[argind];
        if (streq(arg, "-h")){
            usage(argv[0], 0);
            return true;
        }
        else if (streq(arg, "-c")){
            char *ptr = argv[++argind];
            if (ptr == NULL){
                return false;
            }
            if (streq(ptr, "Single")){
                *mode = SINGLE;
            }
            else if (streq(ptr, "Forking")){
                *mode = FORKING;
            }
            else { return false; }
            argind++;
        }
        else if (streq(arg, "-m")){
            char *ptr = argv[++argind];
            if (ptr == NULL){
                return false;
            }
            if (ptr[0] == '-'){
                return false;
            }
            MimeTypesPath = argv[argind];
            argind++;
        }
        else if (streq(arg, "-M")){
            char *ptr = argv[++argind];
            if (ptr == NULL){
                return false;
            }
            if (ptr[0] == '-'){
                return false;
            }
            DefaultMimeType = argv[argind];
            argind++;
        }
        else if (streq(arg, "-p")){
            char *ptr = argv[++argind];
            if (ptr == NULL){
                return false;
            }
            if (ptr[0] == '-'){
                return false;
            }
            Port = argv[argind];
            argind++;
        }
        else if (streq(arg, "-r")){
            char *ptr = argv[++argind];
            if (ptr == NULL){
                return false;
            }
            if (ptr[0] == '-'){
                return false;
            }
            RootPath = argv[argind];
            argind++;
        }
    }
    return true;
}

/**
 * Parses command line options and starts appropriate server
 **/
int main(int argc, char *argv[]) {
    ServerMode mode;

    /* Parse command line options */
    if (!parse_options(argc, argv, &mode)){
        usage(argv[0], 1);
        return EXIT_FAILURE;
    }
    /* Listen to server socket */
    int server_fd = socket_listen(Port);
    if (server_fd < 0) {
        return EXIT_FAILURE;
    }
    /* Determine real RootPath */
    char buffer[BUFSIZ];
    RootPath = realpath(RootPath, buffer);
    if (RootPath == NULL){
        fprintf(stderr, "Get realpath failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    log("Listening on port %s", Port);
    debug("RootPath        = %s", RootPath);
    debug("MimeTypesPath   = %s", MimeTypesPath);
    debug("DefaultMimeType = %s", DefaultMimeType);
    debug("ConcurrencyMode = %s", mode == SINGLE ? "Single" : "Forking");

    /* Start either forking or single HTTP server */
    if (mode != SINGLE){
        forking_server(server_fd);
    }
    else { single_server(server_fd); }

    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
