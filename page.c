// vim: sw=2 ts=2 expandtab smartindent

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/*
 * Create a server socket bound to the specified host and port. If 'host'
 * is NULL, this will bind "generically" (all addresses).
 *
 * Returned value is the server socket descriptor, or -1 on error.
 */
static int host_bind(const char *host, const char *port) {
  struct addrinfo hints, *si, *p;
  int fd;
  int err;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  err = getaddrinfo(host, port, &hints, &si);
  if (err != 0) {
    fprintf(
      stderr,
      "ERROR: getaddrinfo(): %s\n",
      gai_strerror(err)
    );
    return -1;
  }
  fd = -1;
  for (p = si; p != NULL; p = p->ai_next) {
    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    size_t sa_len;
    void *addr;
    int opt;

    struct sockaddr *sa = (struct sockaddr *)p->ai_addr;
    if (sa->sa_family == AF_INET) {
      sa4 = *(struct sockaddr_in *)sa;
      sa = (struct sockaddr *)&sa4;
      sa_len = sizeof sa4;
      addr = &sa4.sin_addr;
      if (host == NULL) {
              sa4.sin_addr.s_addr = INADDR_ANY;
      }
    } else if (sa->sa_family == AF_INET6) {
      sa6 = *(struct sockaddr_in6 *)sa;
      sa = (struct sockaddr *)&sa6;
      sa_len = sizeof sa6;
      addr = &sa6.sin6_addr;
      if (host == NULL) {
              sa6.sin6_addr = in6addr_any;
      }
    } else {
      addr = NULL;
      sa_len = p->ai_addrlen;
    }

    /* print out the address we're bound to */
    {
      char tmp[INET6_ADDRSTRLEN + 50] = {0};

      if (addr != NULL) {
        const char *addr_str = inet_ntop(p->ai_family, addr, tmp, sizeof tmp);
        if (addr_str == NULL) {
          perror("inet_ntop");
        }

        if (sa->sa_family == AF_INET6) {
          fprintf(stderr, "binding to \"http://[%s]:%s\"\n", tmp, port);
        } else {
          fprintf(stderr, "binding to \"http://%s:%s\"\n", tmp, port);
        }
      } else {
        fprintf(stderr, "<unknown family: %d>", (int)sa->sa_family);
      }
    }

    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) {
      perror("socket()");
      continue;
    }
    opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    opt = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt);
    if (bind(fd, sa, sa_len) < 0) {
      perror("bind()");
      close(fd);
      continue;
    }
    break;
  }
  if (p == NULL) {
    freeaddrinfo(si);
    fprintf(stderr, "ERROR: failed to bind\n");
    return -1;
  }
  freeaddrinfo(si);
  if (listen(fd, 5) < 0) {
    perror("listen()");
    close(fd);
    return -1;
  }
  fprintf(stderr, "bound.\n");
  return fd;
}

/*
 * Accept a single client on the provided server socket. This is blocking.
 * On error, this returns -1.
 */
static int accept_client(int server_fd) {

    struct sockaddr sa;
    socklen_t sa_len = sizeof sa;
    int fd = accept(server_fd, &sa, &sa_len);
    if (fd < 0) {
      perror("accept()");
      return -1;
    }

    const char *name = NULL;
    char tmp[INET6_ADDRSTRLEN + 50];
    switch (sa.sa_family) {
      case AF_INET:
        name = inet_ntop(
          AF_INET,
          &((struct sockaddr_in *)&sa)->sin_addr,
          tmp,
          sizeof tmp
        );
        break;
      case AF_INET6:
        name = inet_ntop(
          AF_INET6,
          &((struct sockaddr_in6 *)&sa)->sin6_addr,
          tmp,
          sizeof tmp
        );
        break;
    }

    if (name == NULL) {
      sprintf(tmp, "<unknown: %lu>", (unsigned long)sa.sa_family);
      name = tmp;
    }
    fprintf(stderr, "accepting connection from: %s\n", name);
    return fd;
}

static int sock_read(int fd, unsigned char *buf, size_t len) {
  for (;;) {
    ssize_t rlen;

    rlen = read(fd, buf, len);
    if (rlen <= 0) {
      if (rlen < 0 && errno == EINTR) {
        continue;
      }
      return -1;
    }
    return (int)rlen;
  }
}

static int sock_write(int fd, const unsigned char *buf, size_t len) {
  for (;;) {
    size_t wlen = write(fd, buf, len);
    if (wlen <= 0) {
      if (wlen < 0 && errno == EINTR) {
        continue;
      }
      return -1;
    }
    return (int)wlen;
  }
}

/*
 * Sample HTTP response to send.
 */
static const char *HTTP_RES =
"HTTP/1.0 200 OK\r\n"
"Content-Length: 46\r\n"
"Connection: close\r\n"
"Content-Type: text/html; charset=iso-8859-1\r\n"
"\r\n"
"<html>\r\n"
"<body>\r\n"
"<p>Test!</p>\r\n"
"</body>\r\n"
"</html>\r\n";

int main() {
  int fd = host_bind(NULL, "8081");

  if (fd < 0) {
    return 1;
  }

  /*
  * Process each client, one at a time.
  */
  for (;;) {

    int cfd = accept_client(fd);
    if (cfd < 0) {
      return 1;
    }

    int seen_linefeed = 0;
    for (;;) {
      unsigned char x;

      if (sock_read(cfd, &x, 1) < 0) {
        goto client_drop;
      }

      /* ignore carriage return */
      if (x == 0x0D) {
        continue;
      }

      /* track line feeds */
      if (x == 0x0A) {
        if (seen_linefeed) break;
        seen_linefeed = 1;
      } else {
        seen_linefeed = 0;
      }
    }

    sock_write(cfd, (unsigned char *)HTTP_RES, strlen(HTTP_RES));

  client_drop:
    close(cfd);

  }
}
