// vim: sw=2 ts=2 expandtab smartindent

#ifndef server_IMPLEMENTATION

typedef enum {
  ClientPointAction_None,
  ClientPointAction_Add,
  ClientPointAction_Remove,
} ClientPointAction;
typedef struct {
  ClientPointAction action;
  size_t client_id, path_id;
  double x, y;
} ClientPoint;

#define POINT_COUNT 1000
typedef struct {
  ClientPoint points[POINT_COUNT];
  size_t points_i;

  int host_fd;
  size_t client_id_i;
  /* The head of the linked list of clients */
  Client *last_client;
} Server;

static int server_init(Server *server);

static void server_add_client(Server *server, int net_fd);

static int server_step_client(Server *server, Client *c);
static int server_ws_handle_request(Server *server, Client *c);

static void server_drop_client(Server *server, Client *c);

#endif


#ifdef server_IMPLEMENTATION

static int server_init(Server *server) {
  server->host_fd = socket_host_bind(NULL, "8081");

  if (server->host_fd < 0) {
    return -1;
  }

  return 0;
}

static void server_add_client(Server *server, int net_fd) {
  Client *c = malloc(sizeof(Client));
  client_init(c, net_fd, server->client_id_i++);
  c->next = server->last_client;
  server->last_client = c;
}

static void server_drop_client(Server *server, Client *c) {
  client_drop(c);

  if (server->last_client == c) {
    server->last_client = c->next;
  } else
    for (Client *o = server->last_client; o; o = o->next)
      if (o->next == c) {
        o->next = c->next;
        break;
      }

  free(c);
}

static void clientpoint_fprint(ClientPoint *cp, FILE *f) {
  fprintf(
    f,
    "%d, %zu, %zu, %lf, %lf",
    cp->action,
    cp->client_id,
    cp->path_id,
    cp->x,
    cp->y
  );
}

static int clientpoint_fscan(ClientPoint *cp, FILE *f) {
  if (fscanf(
    f,
    "%zu, %lf, %lf",
    &cp->path_id,
    &cp->x,
    &cp->y
  ) < 3)
    return -1;

  return 0;
}

static void server_broadcast_clientpoint(
  Server *server,
  ClientPoint *cp
) {
  char *msg;
  size_t msg_len;
  {
    FILE *tmp = open_memstream(&msg, &msg_len);

    clientpoint_fprint(cp, tmp);

    fclose(tmp);
  }

  for (
    Client *other = server->last_client;
    other;
    other = other->next
  ) {
    if (other->phase != ClientPhase_Websocket) continue;

    ClientResponse *r = &other->res;
    for (; r->progress < r->buf_len; r = r->next) {
      if (r->next == NULL) {
        r->next = malloc(sizeof(ClientResponse));
        r = r->next;
        break;
      }
    }
    client_ws_send_text(r, msg, msg_len);
  }

  free(msg);
}

static int server_ws_handle_request(Server *server, Client *c) {

  if (c->ws_req.opcode != 1)
    return 0;

  ClientPoint cp = { .action = ClientPointAction_Add, .client_id = c->id };
  {
    FILE *req = fmemopen(c->ws_req.payload, c->ws_req.payload_len, "r");
    if (clientpoint_fscan(&cp, req) < 0)
      return -1;
    fclose(req);
  }

  {
    ClientPoint *sp = &server->points[server->points_i];

    /* broadcast a remove event if there's already an
     * active point at this location in the ring buffer */
    if (sp->action == ClientPointAction_Add) {
      sp->action = ClientPointAction_Remove;
      server_broadcast_clientpoint(server, sp);
    }

    *sp = cp;
    server->points_i = (server->points_i + 1) % POINT_COUNT;
  }

  server_broadcast_clientpoint(server, &cp);

  return 0;
}

static int server_step_client(Server *server, Client *client) {
  ClientPhase phase_before = client->phase;

  restart:
  switch (client_step(client)) {

    case ClientStepResult_Error: {
      server_drop_client(server, client);
      return -1;
    } break;

    case ClientStepResult_NoAction: {
    } break;

    case ClientStepResult_Restart: {
      goto restart;
    } break;

    case ClientStepResult_WsMessageReady: {
      if (server_ws_handle_request(server, client) < 0) {
        server_drop_client(server, client);
        return -1;
      }

      /* let's reset the request so we can
       * start receiving a new one */
      free(client->ws_req.payload);
      memset(&client->ws_req, 0, sizeof(client->ws_req));

      goto restart;
    }; break;
  }

  /* if they've just established a websocket connection,
   * send them the last POINT_COUNT points drawn before
   * they connected */
  if (client->phase != phase_before &&
      client->phase == ClientPhase_Websocket) {

    ClientResponse *last = &client->res;
    for (int i = 0; i < POINT_COUNT; i++) {
      ClientPoint *cp = server->points + i;
      if (!cp->action) break;

      ClientResponse *r = (last->buf) ?
        malloc(sizeof(ClientResponse)) :
        last;

      {
        char *msg;
        size_t msg_len;
        {
          FILE *tmp = open_memstream(&msg, &msg_len);
          clientpoint_fprint(cp, tmp);
          fclose(tmp);
        }

        client_ws_send_text(r, msg, msg_len);

        free(msg);
      }

      if (last != r) {
        last->next = r;
      }
      last = r;
    }

    phase_before = ClientPhase_Websocket;
    goto restart;
  }

  return 0;
}

#endif
