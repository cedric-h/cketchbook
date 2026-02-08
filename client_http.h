// vim: sw=2 ts=2 expandtab smartindent

/**
 * The HTML client that establishes a websocket connection back to us.
 **/
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
"const ws = new WebSocket(window.location.href + '/chat');\r\n" \
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
"  const [action, user_id, path_id, x, y] = msg\r\n" \
"    .data\r\n" \
"    .split(', ')\r\n" \
"    .map(x => parseInt(x));\r\n" \
"  const path_hash = user_id + '_' + path_id;\r\n" \
"  if (!input.server_paths.has(path_hash))\r\n" \
"    input.server_paths.set(path_hash, []);\r\n" \
"  if (action == 1) input.server_paths.get(path_hash).push({ x, y });\r\n" \
"  else if (action == 2) {\r\n" \
"    input.server_paths.set(\r\n" \
"      path_hash,\r\n" \
"      input\r\n" \
"        .server_paths\r\n" \
"        .get(path_hash)\r\n" \
"        .filter(p => {\r\n" \
"          const delta = Math.sqrt((p.x - x)*(p.x - x) + (p.y - y)*(p.y - y));\r\n" \
"          return delta > 1;\r\n" \
"        })\r\n" \
"    );\r\n" \
"  }\r\n" \
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

static int client_http_respond_to_request(Client *c) {

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
    c->http_req.buf = NULL;
  }
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
    client_ws_fwrite_sec_accept(tmp, key);
    fprintf(tmp, "\r\n");

    fprintf(tmp, "\r\n");

    fclose(tmp);
    c->res.phase_after_http = ClientPhase_Websocket;
  } else {
    FILE *tmp = open_memstream(&c->res.buf, &c->res.buf_len);
    fprintf(tmp, "HTTP/1.1 404 Not Found\r\n\r\n");
    fclose(tmp);
  }

  return 0;
}

static ClientStepResult client_http_read_request(Client *c) {
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
          if (client_http_respond_to_request(c) < 0)
            return ClientStepResult_Error;
          return ClientStepResult_Restart;
        }
        c->http_req.seen_linefeed = 1;
      } else {
        c->http_req.seen_linefeed = 0;
      }
    }
  }

  return ClientStepResult_NoAction;
}

static ClientStepResult client_http_write_response(Client *c) {
  while (c->res.progress < c->res.buf_len) {
    char byte = c->res.buf[c->res.progress];
    size_t wlen = write(c->net_fd, &byte, 1);

    if (wlen < 1) {
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        perror("client write()");
        return ClientStepResult_Error;
      }
      return ClientStepResult_NoAction;
    }

    /* important to only increase this if write succeeds */
    c->res.progress += 1;
  }

  if (c->res.progress == c->res.buf_len) {
    if (c->res.phase_after_http == ClientPhase_Empty) {
      return ClientStepResult_Error;
    } else {
      c->phase = c->res.phase_after_http;

      free(c->res.buf);
      memset(&c->res, 0, sizeof(c->res));

      return ClientStepResult_Restart;
    }
  }

  return ClientStepResult_NoAction;
}
