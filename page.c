// vim: sw=2 ts=2 expandtab smartindent

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <fcntl.h>
#include <poll.h>

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
static int accept_client(int server_fd) {

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
  return fd;
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
"<!DOCTYPE html>\r\n" \
"<html lang='en'>\r\n" \
"  <head>\r\n" \
"    <meta charset='utf-8' />\r\n" \
"    <title>Cketchbook</title>\r\n" \
"    <style> document, body { margin: 0px; padding: 0px; overflow: hidden; } </style>\r\n" \
"  </head>\r\n" \
"\r\n" \
"  <body>\r\n" \
"    <canvas id='pagecanvas'></canvas>\r\n" \
"    <script>'use strict'; (async () => {\r\n" \
"const ws = new WebSocket('/chat');\r\n" \
"await new Promise(res => ws.onopen = res);\r\n" \
"\r\n" \
"const canvas = document.getElementById('pagecanvas');\r\n" \
"const ctx = canvas.getContext('2d');\r\n" \
"(window.onresize = () => {\r\n" \
"  canvas.width = window.innerWidth*window.devicePixelRatio,\r\n" \
"  canvas.height = window.innerHeight*window.devicePixelRatio\r\n" \
"  canvas.style.width = window.innerWidth + 'px';\r\n" \
"  canvas.style.height = window.innerHeight + 'px';\r\n" \
"})();\r\n" \
"\r\n" \
"let input = {\r\n" \
"  mouse_down: false,\r\n" \
"  local_paths: [],\r\n" \
"  server_paths: new Map(),\r\n" \
"};\r\n" \
"ws.onmessage = msg => {\r\n" \
"  const [user_id, path_id, x, y] = msg\r\n" \
"    .data\r\n" \
"    .split(', ')\r\n" \
"    .map(x => parseInt(x));\r\n" \
"  const path_hash = user_id + '_' + path_id;\r\n" \
"  if (!input.server_paths.has(path_hash))\r\n" \
"    input.server_paths.set(path_hash, []);\r\n" \
"  input.server_paths.get(path_hash).push({ x, y });\r\n" \
"};\r\n" \
"\r\n" \
"canvas.onmousedown = ev => {\r\n" \
"  ev.preventDefault();\r\n" \
"  input.mouse_down = true;\r\n" \
"  input.local_paths.push([]);\r\n" \
"};\r\n" \
"canvas.onmouseup = ev => {\r\n" \
"  ev.preventDefault();\r\n" \
"  input.mouse_down = false;\r\n" \
"};\r\n" \
"canvas.onmousemove = ev => {\r\n" \
"  ev.preventDefault();\r\n" \
"  if (!input.mouse_down) return false;\r\n" \
"  const x = ev.clientX * window.devicePixelRatio;\r\n" \
"  const y = ev.clientY * window.devicePixelRatio;\r\n" \
"  input.local_paths.at(-1).push({ x, y });\r\n" \
"  ws.send(\r\n" \
"    (input.local_paths.length - 1) +\r\n" \
"      ', ' +\r\n" \
"      x.toFixed(0) +\r\n" \
"      ', ' +\r\n" \
"      y.toFixed(0)\r\n" \
"  );\r\n" \
"}\r\n" \
"\r\n" \
"requestAnimationFrame(function render(now) {\r\n" \
"  requestAnimationFrame(render);\r\n" \
"\r\n" \
"  ctx.fillStyle = 'white';\r\n" \
"  ctx.fillRect(0, 0, canvas.width, canvas.height);\r\n" \
"\r\n" \
"  {\r\n" \
"    ctx.beginPath();\r\n" \
"    for (const path of input.server_paths.values()) {\r\n" \
"      for (let i = 0; i < path.length; i++) {\r\n" \
"        const p = path[i];\r\n" \
"        ctx[i ? 'lineTo' : 'moveTo'](p.x, p.y);\r\n" \
"\r\n" \
"        // show verts\r\n" \
"        // ctx.fillStyle = 'purple';\r\n" \
"        // const size = 20;\r\n" \
"        // ctx.fillRect(p.x - size*0.5, p.y - size*0.5, size, size);\r\n" \
"      }\r\n" \
"    }\r\n" \
"    ctx.lineWidth = 4 * window.devicePixelRatio;\r\n" \
"    ctx.stroke();\r\n" \
"    ctx.closePath();\r\n" \
"  }\r\n" \
"})\r\n" \
"\r\n" \
"function lerp(v0, v1, t) { return (1 - t) * v0 + t * v1; }\r\n" \
"function inv_lerp(min, max, p) { return (p - min) / (max - min); }\r\n" \
"    })();</script>\r\n" \
"  </body>\r\n" \
"</html>\r\n"

typedef enum ClientPhase {
  ClientPhase_Empty,
  ClientPhase_HttpRequesting,
  ClientPhase_HttpResponding,
  ClientPhase_Websocket,
} ClientPhase;

typedef struct {

  ClientPhase phase;

  size_t id;
  int net_fd; /* net fd from accept() */

  /* requesting */
  struct {
    FILE *file; /* closed after requesting */
    bool seen_linefeed;

    /* request data goes in here when file is closed */
    char *buf;
    size_t buf_len;
  } http_req;

  struct {
    size_t progress;

    uint8_t fin, opcode, has_mask, payload_len, mask[4];
    char *payload;
  } ws_req;

  struct {
    /* response data goes in here */
    char *buf;
    size_t buf_len, progress;

    ClientPhase phase_after_http;
  } res;

} Client;

static void client_drop(Client *c) {
  c->phase = ClientPhase_Empty;
  /* free everything, remove from parent list, idk? */
}

static int client_ws_handle_request(Client *c, Client clients[10]) {

  for (int i = 0; i < 10; i++) {
    Client *other = clients + i;
    if (other->phase != ClientPhase_Websocket) continue;
    /* right now we can only write out one message at at time */
    if (other->res.progress != 0) continue;

    char *out;
    size_t out_len;
    FILE *tmp = open_memstream(&out, &out_len);
    {

      /* WS frame header */
      {
        uint8_t fin = 1;
        uint8_t opcode = 1;
        uint8_t byte;

        byte = (fin << 7) | (opcode & 0b1111);
        fwrite(&byte, 1, 1, tmp);

        byte = 0; /* put in at end with hack */
        fwrite(&byte, 1, 1, tmp);
      }

      fprintf(tmp, "%zu, ", c->id);

      fwrite(c->ws_req.payload, c->ws_req.payload_len, 1, tmp);

      fclose(tmp);
    }

    out[1] = out_len - 2;

    /* reset response and copy in our new response */
    other->res.buf = out;
    other->res.buf_len = out_len;
  }

  return 0;
}

static int client_http_handle_request(Client *c) {

  char path[31] = {0};
  char key[31] = {0};
  {
    fclose(c->http_req.file);

    /* cannot fscanf a memstream, can fscanf an fmemopen */
    /* (can grow a memstream, cannot grow an fmemopen) */
    FILE *req = fmemopen(c->http_req.buf, c->http_req.buf_len, "r");

    if (fscanf(req, "GET %30s HTTP/1.1\r\n", path) == 0)
      return -1;

    while (fscanf(req, "Sec-WebSocket-Key: %30s\r\n", key) <= 0)
      if (fscanf(req, "%*[^\n]\n") == EOF)
        break; /* no key found */

    fclose(req);
    free(c->http_req.buf);
  }

  fprintf(stderr, "key = \"%s\"\n", key);
  fprintf(stderr, "path = \"%s\"\n", path);

  c->phase = ClientPhase_HttpResponding;
  c->res.phase_after_http = ClientPhase_Empty;

  if (strcmp(path, "/") == 0) {
    FILE *tmp = open_memstream(&c->res.buf, &c->res.buf_len);
    fprintf(tmp, "HTTP/1.0 200 OK\r\n");
    fprintf(tmp, "Content-Length: %lu\r\n", strlen(HTML_RES) - 2);
    fprintf(tmp, "Connection: close\r\n");
    fprintf(tmp, "Content-Type: text/html; charset=iso-8859-1\r\n");
    fprintf(tmp, "\r\n");

    fprintf(tmp, "%s", HTML_RES);

    fclose(tmp);
  } else if (strcmp(path, "/chat") == 0) {
    FILE *tmp = open_memstream(&c->res.buf, &c->res.buf_len);
    fprintf(tmp, "HTTP/1.1 101 Switching Protocols\r\n");
    fprintf(tmp, "Upgrade: websocket\r\n");
    fprintf(tmp, "Connection: Upgrade\r\n");

    fprintf(tmp, "Sec-WebSocket-Accept: ");
    ws_fwrite_sec_accept(tmp, key);
    fprintf(tmp, "\r\n");

    fprintf(tmp, "\r\n");

    fclose(tmp);
    c->res.phase_after_http = ClientPhase_Websocket;
  } else {
    static char *four_oh_four = "HTTP/1.1 404 Not Found\r\n\r\n";
    c->res.buf = four_oh_four;
    c->res.buf_len = strlen(four_oh_four);
  }

  return 0;
}

int main() {

  int fd = host_bind(NULL, "8081");

  if (fd < 0) {
    return 1;
  }

  uint16_t poll_any_event = 0;
  poll_any_event |= /* all the writes */ POLLWRNORM | POLLWRBAND;
  poll_any_event |= /* all the reads  */ POLLPRI | POLLRDNORM | POLLRDBAND;


#define CLIENT_MAX 10
  Client clients[10] = {0};
  size_t client_i = 0;

  /*
   * Poll and look for things to do
   */
  for (;;) {

    /* first poll for new clients */
    {
      struct pollfd new_client = {
        .fd = fd,
        .events = poll_any_event,
      };

      poll(&new_client, 1, 0);

      if (new_client.revents > 0) for (;;) {
        int fd = accept_client(new_client.fd);
        if (fd < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN)
            break;
          continue;
        }

        size_t client_id = client_i++;
        Client *c = clients + (client_id);
        *c = (Client) {
          .id = client_id,
          .phase = ClientPhase_HttpRequesting,
          .net_fd = fd,
        };
        c->http_req.file = open_memstream(
          &c->http_req.buf,
          &c->http_req.buf_len
        );
      }
    }

    /* now see if any clients need responding to */
    {
      struct pollfd client_polls[CLIENT_MAX] = {0};
      for (int i = 0; i < 10; i++) {
        client_polls[i].fd = clients[i].net_fd;
        client_polls[i].events = poll_any_event;
      }

      poll(client_polls, CLIENT_MAX, 0);

      for (int i = 0; i < CLIENT_MAX; i++) {
        Client *c = clients + i;

        restart:
        switch (c->phase) {

          case ClientPhase_Empty:
            continue;

          case ClientPhase_HttpRequesting: {
            for (;;) {
              char byte;
              int read_ret = read(c->net_fd, &byte, 1);
              if (read_ret < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                  perror("client read()");
                  client_drop(c);
                }
                break;
              }

              fprintf(c->http_req.file, "%c", byte);

              /* ignore carriage return */
              if (byte != 0x0D) {
                /* track line feeds */
                if (byte == 0x0A) {
                  if (c->http_req.seen_linefeed) {
                    if (client_http_handle_request(c) < 0) {
                      client_drop(c);
                      break;
                    }
                    goto restart;
                  }
                  c->http_req.seen_linefeed = 1;
                } else {
                  c->http_req.seen_linefeed = 0;
                }
              }
            }
          }; break;

          case ClientPhase_HttpResponding: {

            while (c->res.progress < c->res.buf_len) {
              char byte = c->res.buf[c->res.progress];
              size_t wlen = write(c->net_fd, &byte, 1);

              if (wlen < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                  perror("client write()");
                  client_drop(c);
                }
                break;
              }

              /* important to only increase this if write succeeds */
              c->res.progress += 1;
            }

            if (c->res.progress == c->res.buf_len) {
              if (c->res.phase_after_http == ClientPhase_Empty) {
                client_drop(c);
              } else {
                c->phase = c->res.phase_after_http;
                goto restart;
              }
            }
          }; break;

          case ClientPhase_Websocket: {

            /* first, let's send out anything we can */
            {
              while (c->res.progress < c->res.buf_len) {
                char byte = c->res.buf[c->res.progress];
                size_t wlen = write(c->net_fd, &byte, 1);

                if (wlen < 0) {
                  if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    perror("client write()");
                    client_drop(c);
                  }
                  break;
                }

                /* important to only increase this if write succeeds */
                c->res.progress += 1;
              }

              if (c->res.progress == c->res.buf_len) {
                /* done writing, we can reset response */
                free(c->res.buf);
                memset(&c->res, 0, sizeof(c->res));
              }
            }

            /* now let's see if there's anything to receive */
            for (;;) {
              char byte;
              int read_ret = read(c->net_fd, &byte, 1);
              if (read_ret < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                  perror("client read()");
                  client_drop(c);
                }
                break;
              }

              int progress = ++c->ws_req.progress;

              if (progress == 1) {
                c->ws_req.fin    = (byte >> 7) & 1;
                c->ws_req.opcode = (byte >> 0) & 0b1111;
                continue;
              }

              if (progress == 2) {
                c->ws_req.has_mask    = (byte >> 7) & 1;
                c->ws_req.payload_len = (byte >> 0) & 127;
                if (c->ws_req.payload_len == 127 ||
                    c->ws_req.payload_len == 126) {
                  fprintf(
                    stderr,
                    "WS payloads > 126 bytes are not yet supported!\n"
                  );
                  client_drop(c);
                  break;
                }
                c->ws_req.payload     = malloc(c->ws_req.payload_len);
                continue;
              }

              int mask_progress = progress - 3;
              if (c->ws_req.has_mask && mask_progress < 4) {
                c->ws_req.mask[mask_progress] = byte;
                continue;
              }

              int payload_progress = mask_progress - 4;
              uint8_t mask_byte = c->ws_req.mask[payload_progress % 4];
              c->ws_req.payload[payload_progress] = byte ^ mask_byte;
              if (payload_progress == (c->ws_req.payload_len - 1)) {
                if (client_ws_handle_request(c, clients) < 0)
                  client_drop(c);

                /* let's reset the request so we can
                 * start receiving a new one */
                free(c->ws_req.payload);
                memset(&c->ws_req, 0, sizeof(c->ws_req));

                goto restart;
              }

            }
          }; break;

        }

      }
    }

  }

}
