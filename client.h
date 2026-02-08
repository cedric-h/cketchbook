// vim: sw=2 ts=2 expandtab smartindent

#ifndef client_IMPLEMENTATION

typedef enum ClientPhase {
  ClientPhase_Empty,
  ClientPhase_HttpRequesting,
  ClientPhase_HttpResponding,
  ClientPhase_Websocket,
} ClientPhase;

/**
 * If any HTTP or Websocket message over this size,
 * we drop the client.
 **/
#define MAX_MESSAGE_SIZE (1 << 13)
typedef struct Client {
  struct Client *next;

  ClientPhase phase;

  size_t id;
  int net_fd; /* net fd from accept() */

  /* requesting */
  struct {
    FILE *file; /* closed after requesting */
    bool seen_linefeed;
    size_t bytes_read;

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

static void client_init(Client *c, int net_fd, size_t client_id);
static int client_http_handle_request(Client *c);
static void client_ws_send_text(Client *c, char *text, size_t text_len);

typedef enum {
  ClientStepResult_Error,
  ClientStepResult_NoAction,
  ClientStepResult_WsMessageReady,
} ClientStepResult;
static ClientStepResult client_step(Client *c);

static void client_drop(Client *c);

#endif


#ifdef client_IMPLEMENTATION

static void client_init(Client *c, int net_fd, size_t client_id) {
  *c = (Client) {
    .id = client_id,
    .phase = ClientPhase_HttpRequesting,
    .net_fd = net_fd,
  };
  c->http_req.file = open_memstream(
    &c->http_req.buf,
    &c->http_req.buf_len
  );
}

static void client_drop(Client *c) {
  c->phase = ClientPhase_Empty;

  if (c->http_req.buf     != NULL) free(c->http_req.buf);
  if (c->  ws_req.payload != NULL) free(c->ws_req.payload);
  if (c->     res.buf     != NULL) free(c->res.buf);

  close(c->net_fd);
}

static ClientStepResult client_step(Client *c) {
  restart:
  switch (c->phase) {

    case ClientPhase_Empty:
      return ClientStepResult_NoAction;

    case ClientPhase_HttpRequesting: {
      for (;;) {
        char byte;
        int read_ret = read(c->net_fd, &byte, 1);
        if (read_ret < 0) {
          if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("client read()");
            return ClientStepResult_Error;
          }
          break;
        }

        fwrite(&byte, 1, 1, c->http_req.file);
        c->http_req.bytes_read++;

        if (c->http_req.bytes_read > MAX_MESSAGE_SIZE)
          return ClientStepResult_Error;

        /* ignore carriage return */
        if (byte != 0x0D) {
          /* track line feeds */
          if (byte == 0x0A) {
            if (c->http_req.seen_linefeed) {
              if (client_http_handle_request(c) < 0)
                return ClientStepResult_Error;
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
            return ClientStepResult_Error;
          }
          break;
        }

        /* important to only increase this if write succeeds */
        c->res.progress += 1;
      }

      if (c->res.progress == c->res.buf_len) {
        if (c->res.phase_after_http == ClientPhase_Empty) {
          return ClientStepResult_Error;
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
              return ClientStepResult_Error;
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
        if (read_ret == 0) break;
        else if (read_ret < 0) {
          if (errno != EWOULDBLOCK && errno != EAGAIN) {
            return ClientStepResult_Error;
          }
          break;
        }

        int progress = ++c->ws_req.progress;

        if (c->ws_req.progress > MAX_MESSAGE_SIZE)
          return ClientStepResult_Error;

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
            return ClientStepResult_Error;
          }
          c->ws_req.payload = malloc(c->ws_req.payload_len);
          continue;
        }

        int mask_progress = progress - 3;
        if (c->ws_req.has_mask && mask_progress < 4) {
          c->ws_req.mask[mask_progress] = byte;
          continue;
        }

        /* unmask the payload */
        int payload_progress = mask_progress - 4;
        uint8_t mask_byte = c->ws_req.mask[payload_progress % 4];
        c->ws_req.payload[payload_progress] = byte ^ mask_byte;

        /* handle the final payload */
        if (payload_progress == (c->ws_req.payload_len - 1))
          return ClientStepResult_WsMessageReady;

      }
    }; break;

  }

  return ClientStepResult_NoAction;
}

#include "client_ws.h"
#include "client_http.h"

#endif
