#!/usr/bin/env python3

import multiprocessing
import os
import requests
import sys
import time

# Globals

PROCESSES = 1
REQUESTS  = 1
VERBOSE   = False
URL       = None

# Functions

def usage(status=0):
    print('''Usage: {} [-p PROCESSES -r REQUESTS -v] URL
    -h              Display help message
    -v              Display verbose output

    -p  PROCESSES   Number of processes to utilize (1)
    -r  REQUESTS    Number of requests per process (1)
    '''.format(os.path.basename(sys.argv[0])))
    sys.exit(status)

def do_request(pid):
    ''' Perform REQUESTS HTTP requests and return the average elapsed time. '''
    average = 0
    for number in range(REQUESTS):
        init = time.time()
        r = requests.get(URL)
        if VERBOSE:
            print(r.text)
        done = time.time()
        average = average + (done - init)
        print('Process: {:d}, Requests: {:d}, Elapsed Time: {:.2f}'.format(pid, number, done - init))

    print('Process: {:d}, AVERAGE:   , Elapsed Time: {:.2f}'.format(pid, average / REQUESTS))

    return average

# Main execution

if __name__ == '__main__':

    if len(sys.argv) is 1:
        usage(1)
    # Parse command line arguments
    ARGUMENTS = sys.argv[1:]
    while ARGUMENTS and ARGUMENTS[0].startswith('-') and len(ARGUMENTS[0]) > 1:
        arg = ARGUMENTS.pop(0)
        if arg == '-h':
            usage(0);
        elif arg == '-v':
            VERBOSE = True
        elif arg == '-p':
            PROCESSES = int(ARGUMENTS.pop(0))
        elif arg == '-r':
            REQUESTS = int(ARGUMENTS.pop(0))
        else:
            usage(1)

    # Constants
    URL = ARGUMENTS.pop(0)

    # Create pool of workers and perform requests
    pool = multiprocessing.Pool(PROCESSES)
    totaltime = pool.map(do_request, range(PROCESSES))
    print('TOTAL AVERAGE ELAPSED TIME: {:.2f}'.format(sum(totaltime) / (PROCESSES * REQUESTS)))

# vim: set sts=4 sw=4 ts=8 expandtab ft=python:
