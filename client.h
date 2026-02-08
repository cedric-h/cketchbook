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

typedef enum {
  ClientStepResult_Error,
  ClientStepResult_NoAction,
  ClientStepResult_Restart,
  ClientStepResult_WsMessageReady,
} ClientStepResult;
static ClientStepResult client_step(Client *c);

static void client_drop(Client *c);

static int client_http_respond_to_request(Client *c);
static void client_ws_send_text(Client *c, char *text, size_t text_len);

#endif


#ifdef client_IMPLEMENTATION

#include "client_ws.h"
#include "client_http.h"

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

  switch (c->phase) {

    case ClientPhase_Empty:
      return ClientStepResult_NoAction;

    case ClientPhase_HttpRequesting:
      return client_http_read_request(c);

    case ClientPhase_HttpResponding:
      return client_http_write_response(c);

    case ClientPhase_Websocket:
      return client_ws_step(c);

  }

  return ClientStepResult_NoAction;
}

#endif
