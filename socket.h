// vim: sw=2 ts=2 expandtab smartindent

#ifndef socket_IMPLEMENTATION
static int socket_host_bind(const char *host, const char *port);
static int socket_accept_client(int server_fd);
#endif

#ifdef socket_IMPLEMENTATION
/*
 * Create a server socket bound to the specified host and port. If 'host'
 * is NULL, this will bind "generically" (all addresses).
 *
 * Returned value is the server socket descriptor, or -1 on error.
 */
static int socket_host_bind(const char *host, const char *port) {
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

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
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
 * Accept a single client on the provided server socket.
 * On error, this returns -1.
 */
static int socket_accept_client(int server_fd) {

  struct sockaddr sa;
  socklen_t sa_len = sizeof sa;
  int fd = accept(server_fd, &sa, &sa_len);
  if (fd < 0) {
    if (errno != EWOULDBLOCK && errno != EAGAIN)
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

  /* note: on macos, this is not necessary - only set it on parent! */
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  return fd;
}
#endif
