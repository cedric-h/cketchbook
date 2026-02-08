// vim: sw=2 ts=2 expandtab smartindent

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>

#include "sha1.h"
#include "base64.h"

#include "socket.h"
#include "client.h"
#include "server.h"

int main() {

  /* easier to just handle SIGPIPE ourselves */
  signal(SIGPIPE, SIG_IGN);

  Server server = {0};
  if (server_init(&server) < 0) return 1;

  for (;;) {

    /* first poll for new clients */
    for (;;) {
      int fd = socket_accept_client(server.host_fd);
      if (fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          break;
        else
          continue;
      }

      server_add_client(&server, fd);
    }

    /* now see if any clients need responding to */
    for (Client *next = NULL, *c = server.last_client; c; c = next)
      if (c) {
        next = c->next;
        server_step_client(&server, c);
      }
  }

}

#define socket_IMPLEMENTATION
#include "socket.h"
#define base64_IMPLEMENTATION
#include "base64.h"
#define server_IMPLEMENTATION
#include "server.h"
#define client_IMPLEMENTATION
#include "client.h"
