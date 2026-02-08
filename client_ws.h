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
