// vim: sw=2 ts=2 expandtab smartindent

#ifndef server_IMPLEMENTATION

typedef struct {
  /* The head of the linked list of clients */
  Client *last_client;

  int host_fd;

  size_t client_id_i;
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

static int server_ws_handle_request(Server *server, Client *c) {

  if (c->ws_req.opcode != 1)
    return 0;

  char *msg;
  size_t msg_len;
  {
    FILE *tmp = open_memstream(&msg, &msg_len);

    fprintf(tmp, "%zu, ", c->id);
    fwrite(c->ws_req.payload, c->ws_req.payload_len, 1, tmp);

    fclose(tmp);
  }

  for (
    Client *other = server->last_client;
    other;
    other = other->next
  ) {
    if (other->phase != ClientPhase_Websocket) continue;

    client_ws_send_text(other, msg, msg_len);
  }

  free(msg);

  return 0;
}

static int server_step_client(Server *server, Client *client) {

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

  return 0;
}

#endif
