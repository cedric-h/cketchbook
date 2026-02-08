// vim: sw=2 ts=2 expandtab smartindent

static void client_ws_send_text(Client *c, char *text, size_t text_len) {
  /* right now we can only write out one message at at time */
  if (c->res.progress != 0) return;

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

    fwrite(text, text_len, 1, tmp);

    fclose(tmp);
  }

  out[1] = out_len - 2;

  /* reset response and copy in our new response */
  c->res.buf = out;
  c->res.buf_len = out_len;
}

static ClientStepResult client_ws_step(Client *c) {
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

  return ClientStepResult_NoAction;
}

static void client_ws_fwrite_sec_accept(
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
