// vim: sw=2 ts=2 expandtab smartindent

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sha1.h"

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

static char char_base64(unsigned char in) {
  switch (in) {
    case  0: return 'A';
    case  1: return 'B';
    case  2: return 'C';
    case  3: return 'D';
    case  4: return 'E';
    case  5: return 'F';
    case  6: return 'G';
    case  7: return 'H';
    case  8: return 'I';
    case  9: return 'J';
    case 10: return 'K';
    case 11: return 'L';
    case 12: return 'M';
    case 13: return 'N';
    case 14: return 'O';
    case 15: return 'P';
    case 16: return 'Q';
    case 17: return 'R';
    case 18: return 'S';
    case 19: return 'T';
    case 20: return 'U';
    case 21: return 'V';
    case 22: return 'W';
    case 23: return 'X';
    case 24: return 'Y';
    case 25: return 'Z';
    case 26: return 'a';
    case 27: return 'b';
    case 28: return 'c';
    case 29: return 'd';
    case 30: return 'e';
    case 31: return 'f';
    case 32: return 'g';
    case 33: return 'h';
    case 34: return 'i';
    case 35: return 'j';
    case 36: return 'k';
    case 37: return 'l';
    case 38: return 'm';
    case 39: return 'n';
    case 40: return 'o';
    case 41: return 'p';
    case 42: return 'q';
    case 43: return 'r';
    case 44: return 's';
    case 45: return 't';
    case 46: return 'u';
    case 47: return 'v';
    case 48: return 'w';
    case 49: return 'x';
    case 50: return 'y';
    case 51: return 'z';
    case 52: return '0';
    case 53: return '1';
    case 54: return '2';
    case 55: return '3';
    case 56: return '4';
    case 57: return '5';
    case 58: return '6';
    case 59: return '7';
    case 60: return '8';
    case 61: return '9';
    case 62: return '+';
    case 63: return '/';
    default: return '?';
  }
}

static void fbase64(
  FILE *fp,
  unsigned char *data,
  size_t data_len
) {
  for (int i = 0; i < data_len; i += 3) {
    uint32_t input = 0;
    if ((i + 0) < data_len) input |= (data[i + 0] << 16);
    if ((i + 1) < data_len) input |= (data[i + 1] <<  8);
    if ((i + 2) < data_len) input |= (data[i + 2] <<  0);
    size_t bytes_available = ((i + 0) < data_len) +
                             ((i + 1) < data_len) +
                             ((i + 2) < data_len);

    char x1, x2, x3, x4;
    x1 = char_base64((input >> 18) & 63);
    x2 = char_base64((input >> 12) & 63);
    x3 = char_base64((input >>  6) & 63);
    x4 = char_base64((input >>  0) & 63);

    switch (bytes_available) {
      case 3: fprintf(fp, "%c%c%c%c", x1, x2, x3, x4); break;
      case 2: fprintf(fp, "%c%c%c=" , x1, x2, x3    ); break;
      case 1: fprintf(fp, "%c%c=="  , x1, x2        ); break;
    }
  }
}

/* handshake */
static void ws_fwrite_sec_accept(
  FILE *out,
  const char *sec_websocket_key
) {
  static char *websocket_rfc_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  size_t both_size = strlen(sec_websocket_key) + strlen(websocket_rfc_guid) + 1;
  char *both = malloc(both_size);
  strlcpy(both, sec_websocket_key, both_size);
  strlcat(both, websocket_rfc_guid, both_size);

  unsigned char hash[20] = {0};
  /* minus one so it doesn't hash the null terminator */
  SHA1((unsigned char *)both, both_size - 1, hash);
  free(both);

  fbase64(out, hash, 20);
}

/*
 * Sample HTTP response to send.
 */
#define HTML_RES \
"<html>\r\n" \
"<body>\r\n" \
"<p>Test!</p>\r\n" \
"</body>\r\n" \
"</html>\r\n"

int main() {

  /* generate the HTTP response we will provide to clients */
  // size_t http_response_len;
  // char  *http_response;
  // {
  //   FILE *tmp = open_memstream(&http_response, &http_response_len);
  //   fprintf(tmp, "HTTP/1.0 200 OK\r\n");
  //   fprintf(tmp, "Content-Length: %lu\r\n", strlen(HTML_RES) - 2);
  //   fprintf(tmp, "Connection: close\r\n");
  //   fprintf(tmp, "Content-Type: text/html; charset=iso-8859-1\r\n");
  //   fprintf(tmp, "\r\n");

  //   fprintf(tmp, "%s", HTML_RES);

  //   fclose(tmp);
  // }

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

    size_t reqbuf_len = 0;
    char *reqbuf = NULL;
    {
      FILE *req = open_memstream(&reqbuf, &reqbuf_len);

      int seen_linefeed = 0;
      for (;;) {
        unsigned char x;

        if (sock_read(cfd, &x, 1) < 0) {
          goto client_drop;
        }

        fprintf(req, "%c", x);

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

      fclose(req);
    }

    char path[31] = {0};
    char key[31] = {0};
    {
      FILE *req = fmemopen(reqbuf, reqbuf_len, "r");

      if (fscanf(req, "GET %30s HTTP/1.1\r\n", path) == 0)
        return 1;

      while (fscanf(req, "Sec-WebSocket-Key: %30s\r\n", key) <= 0)
        if (fscanf(req, "%*[^\n]\n") == EOF)
          return 1;

      fclose(req);
    }
    fprintf(stderr, "key = \"%s\"\n", key);
    fprintf(stderr, "path = %s\n", path);

    free(reqbuf);


    size_t http_response_len;
    char  *http_response;
    {
      FILE *tmp = open_memstream(&http_response, &http_response_len);
      fprintf(tmp, "HTTP/1.1 101 Switching Protocols\r\n");
      fprintf(tmp, "Upgrade: websocket\r\n");
      fprintf(tmp, "Connection: Upgrade\r\n");

      fprintf(tmp, "Sec-WebSocket-Accept: ");
      ws_fwrite_sec_accept(tmp, key);
      fprintf(tmp, "\r\n");

      fprintf(tmp, "\r\n");

      fclose(tmp);
    }

    sock_write(cfd, (const unsigned char*)http_response, http_response_len);
    free(http_response);

    for (;;) {
      unsigned char first;
      unsigned char second;

      if (sock_read(cfd, &first,  1) < 0) goto client_drop;
      if (sock_read(cfd, &second, 1) < 0) goto client_drop;

      uint8_t fin    = (first >> 7) & 1;
      uint8_t opcode = (first >> 0) & 0b1111;

      uint8_t has_mask = (second >> 7) & 1;
      uint8_t pl_len   = (second >> 0) & 127;

      fprintf(stderr, "> frame received\n");
      fprintf(stderr, "fin: %d\n", fin);
      fprintf(stderr, "opcode: 0x%x\n", opcode);
      fprintf(stderr, "has_mask: %d\n", has_mask);
      fprintf(stderr, "pl_len: %d\n", pl_len);

      uint8_t mask[4] = {0};
      if (has_mask) {
        if (sock_read(cfd, mask, sizeof(mask)) < 0) goto client_drop;
      }

      uint8_t *payload = malloc(pl_len);
      if (sock_read(cfd, payload, pl_len) < 0) goto client_drop;
      for (int i = 0; i < pl_len; i++) payload[i] ^= mask[i % 4];

      fprintf(stderr, "payload = \"");
      for (int i = 0; i < pl_len; i++) {
        fprintf(stderr, "%c", payload[i]);
      }
      fprintf(stderr, "\"\n");

      uint8_t intro = first;
      if (sock_write(cfd, &intro, 1) < 0) goto client_drop;

      uint8_t len = pl_len*2;
      if (sock_write(cfd, &len, 1) < 0) goto client_drop;

      if (sock_write(cfd, payload, pl_len) < 0) goto client_drop;
      if (sock_write(cfd, payload, pl_len) < 0) goto client_drop;

      free(payload);
    }

  client_drop:
    close(cfd);

  }
}
