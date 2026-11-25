#ifndef SERVER_H
#define SERVER_H

/* Starts the HTTP server on 127.0.0.1:port and runs until /api/exit is called.
   Returns 1 on a clean shutdown, 0 if the server could not start. */
int serverRun(int port, int openBrowser);

#endif
