// vim: sw=2 ts=2 expandtab smartindent

#ifndef client_IMPLEMENTATION

typedef enum ClientPhase {
  ClientPhase_Empty,
  ClientPhase_HttpRequesting,
  ClientPhase_HttpResponding,
  ClientPhase_Websocket,
} ClientPhase;

typedef struct ClientResponse {
  struct ClientResponse *next;
  /* used during ClientPhase_HttpResponding to decide where
   * to go when the request is finished being sent out. */
  ClientPhase phase_after_http;

  /* response data goes in here */
  char *buf;
  size_t buf_len, progress;
} ClientResponse;

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

  /* used for dropping clients that aren't doing anything */
  time_t last_activity, last_ping;

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

  ClientResponse res;

} Client;

static void client_init(Client *c, int net_fd, size_t client_id);

/**
 * Tells hte server which events are worth waking up for.
 * Important not to subscribe to an event you don't handle,
 * otherwise the server will keep waking up and asking you
 * to handle it, which will burn a lot of CPU cycles.
 **/
static short client_events_subscription(Client *c);

typedef enum {
  ClientStepResult_Error,
  ClientStepResult_NoAction,
  ClientStepResult_Restart,
  ClientStepResult_WsMessageReady,
} ClientStepResult;
static ClientStepResult client_step(Client *c);

static void client_drop(Client *c);

static int client_http_respond_to_request(Client *c);
static void client_ws_send_text(
  Client *c,
  char *text,
  size_t text_len
);

#endif



#ifdef client_IMPLEMENTATION

#include "client_ws.h"
#include "client_http.h"

static void client_init(Client *c, int net_fd, size_t client_id) {
  *c = (Client) {
    .id = client_id,
    .last_activity = time(NULL),
    .last_ping = time(NULL),
    .phase = ClientPhase_HttpRequesting,
    .net_fd = net_fd,
  };
  c->http_req.file = open_memstream(
    &c->http_req.buf,
    &c->http_req.buf_len
  );
}

static void client_drop(Client *c) {
  if (c->phase == ClientPhase_HttpRequesting) {
    if (c->http_req.file != NULL)
      fclose(c->http_req.file);
    if (c->http_req.buf != NULL)
      free(c->http_req.buf);
  }

  c->phase = ClientPhase_Empty;

  /* free any lingering queued ClientResponses */
  if (c->res.next) {
    ClientResponse *last = c->res.next;
    for (
      ClientResponse *next = last->next;
      last;
      last = next
    ) {
      next = last->next;
      free(last->buf);
      free(last);
    }
  }

  if (c->  ws_req.payload != NULL) free(c->ws_req.payload);
  if (c->     res.buf     != NULL) free(c->res.buf);

  close(c->net_fd);
}

static short client_events_subscription(Client *c) {
  short events = 0;
  short events_writes = POLLWRNORM | POLLWRBAND          ;
  short events_reads  = POLLRDNORM | POLLRDBAND | POLLPRI;

  switch (c->phase) {
    case ClientPhase_Empty: {
      /* this probably shouldn't happen */
      fprintf(stderr, "empty client!?\n");
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

  return events;
}

static ClientStepResult client_step(Client *c) {

  if ((c->phase == ClientPhase_HttpRequesting) ||
      (c->phase == ClientPhase_HttpRequesting)) {
    long int time_since_io = time(NULL) - c->last_activity;

    /* timeout */
    if (time_since_io > 1)
      return ClientStepResult_Error;
  }

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
