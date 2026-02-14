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

/* hashing/encoding */
#include "sha1.h"
#include "base64.h"

#include "socket.h"
#include "client.h"
#include "server.h"

short poll_fd_get_revents(struct pollfd *fds, size_t fd_count, int fd) {

  for (int i = 0; i < fd_count; i++)
    if (fds[i].fd == fd)
      return fds[i].revents;
  return 0;
}

int main() {

  /* I turned this off before adding support for poll,
   * but after seeing how running the server killed my laptop battery
   * and pegged out my server CPU, I keep this turned off, because
   * anytime we're writing to a pipe that isn't ready is a time we
   * probably weren't aggressive enough with poll */
  // signal(SIGPIPE, SIG_IGN);

  Server server = {0};
  if (server_init(&server) < 0) return 1;

  for (;;) {
    nfds_t fd_count = 1 + server_client_count(&server);
    struct pollfd *fds = calloc(fd_count, sizeof(struct pollfd));

    {
      struct pollfd *fd_w = fds;
      *fd_w++ = (struct pollfd) { .events = POLLIN, .fd = server.host_fd };

      for (Client *c = server.last_client; c; c = c->next) {
        short events = 0;
        short events_writes = POLLWRNORM | POLLWRBAND          ;
        short events_reads  = POLLRDNORM | POLLRDBAND | POLLPRI;

        switch (c->phase) {
          case ClientPhase_Empty: {
            puts("empty client ...");
          } break;
          case ClientPhase_HttpRequesting: {
            events = events_writes;
          } break;
          case ClientPhase_HttpResponding: {
            events = events_reads;
          } break;
          case ClientPhase_Websocket: {
            events = events_reads;
            if (c->res.buf_len > 0) events |= events_writes;
          } break;
        }

        *fd_w++ = (struct pollfd) {
          .events = events,
          .fd = c->net_fd
        };
      }

      printf("polling ... %lu\n", time(NULL));
      size_t updated = poll(fds, fd_count, -1);
      if (updated < 0) {
        perror("poll():");
        free(fds);
        continue;
      }
    }

    /* first poll for new clients */
    if (poll_fd_get_revents(fds, fd_count, server.host_fd)) for (;;) {
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
    for (Client *next = NULL, *c = server.last_client; c; c = next) {
      short revents = poll_fd_get_revents(fds, fd_count, c->net_fd);
      if (revents & (POLLHUP | POLLERR)) {
        server_drop_client(&server, c);
      } else {
        next = c->next;
        server_step_client(&server, c);
      }
    }

    // printf("\nCLIENT COUNT: %zu\n", server_client_count(&server));
    // for (Client *c = server.last_client; c; c = c->next) {
    //   printf("client! id: %zu phase: ", c->id);

    //   switch (c->phase) {
    //     case ClientPhase_Empty         : printf("ClientPhase_Empty         \n"); continue;
    //     case ClientPhase_HttpRequesting: printf("ClientPhase_HttpRequesting(bytes_read: %zu, buf_len: %zu)\n", c->http_req.bytes_read, c->http_req.buf_len); continue;
    //     case ClientPhase_HttpResponding: printf("ClientPhase_HttpResponding\n"); continue;
    //     case ClientPhase_Websocket     : printf("ClientPhase_Websocket     \n"); continue;
    //     default: printf("Unknown phase!\n"); continue;
    //   }
    // }

    free(fds);
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
