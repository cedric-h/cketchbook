// vim: sw=2 ts=2 expandtab smartindent

/* basics */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* networking */
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

/* non-blocking io */
#include <fcntl.h>
#include <poll.h>

#define DEBUG 0

/* hashing/encoding */
#include "sha1.h"
#include "base64.h"

#include "socket.h"
#include "client.h"
#include "server.h"


static volatile bool killed = false;
void interrupt_handler(int _) { killed = true; }

int main() {

  /* I turned this off before adding support for poll,
   * but after seeing how running the server killed my laptop battery
   * and pegged out my server CPU, I kept this turned off, because
   * anytime we're writing to a pipe that isn't ready is a time we
   * probably weren't aggressive enough with poll */
  signal(SIGPIPE, SIG_IGN);

  /* I just like freeing all the memory at the end
   * so that valgrind says I'm a good boy 😇 */
  signal(SIGINT, interrupt_handler);

  Server server = {0};
  if (server_init(&server) < 0) return 1;

  while (!killed) {
    /**
     * This blocks indefinitely until there's something that needs doing.
     **/
    server_poll(&server);

    /* first see if any clients need responding to */
    for (Client *next = NULL, *c = server.last_client; c; c = next) {
      next = c->next;

      short revents = server_client_get_revents(&server, c);
      if (revents & (POLLHUP | POLLERR)) {
        server_drop_client(&server, c);
      } else {
        server_step_client(&server, c);
      }
    }

    printf("\nCLIENT COUNT: %zu\n", server_client_count(&server));
    for (Client *c = server.last_client; c; c = c->next) {
      printf("client! id: %zu phase: ", c->id);

      switch (c->phase) {
        case ClientPhase_Empty         : printf("ClientPhase_Empty         \n"); continue;
        case ClientPhase_HttpResponding: printf("ClientPhase_HttpResponding\n"); continue;
        case ClientPhase_Websocket     : printf("ClientPhase_Websocket     \n"); continue;
        case ClientPhase_HttpRequesting:
          printf(
            "ClientPhase_HttpRequesting"
              "(bytes_read: %zu, buf_len: %zu)\n",
            c->http_req.bytes_read,
            c->http_req.buf_len
          );
          continue;
        default: printf("Unknown phase!\n"); continue;
      }
    }

    /* now poll for new clients */
    if (server_new_client_revent(&server))
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

  }

  server_free(&server);
}

#define socket_IMPLEMENTATION
#include "socket.h"
#define base64_IMPLEMENTATION
#include "base64.h"
#define server_IMPLEMENTATION
#include "server.h"
#define client_IMPLEMENTATION
#include "client.h"
