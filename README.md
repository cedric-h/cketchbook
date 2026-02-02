# what?

This is a simple C program that, when run, hosts a web server. This webserver serves a single page. The page, once opened, establishes a websocket connection back to the server to share what is being drawn with any other peers connected to the server.

<img src="https://github.com/user-attachments/assets/4ddfe3c3-e7b9-443e-bcd2-7f99bffcabd6" height=420 />

# from scratch?

This program hosts HTTP server using Berkeley Sockets (`socket()`/`bind()`/`accept()` etc.).

It supports multiple active websockets connections on a single thread using non-blocking IO (`fcntl` with `O_NONBLOCK`) and `poll.h`.


# dev

Rebuild whenever any of the code changes

- `find page.c | entr -rs 'clear && gcc -g page.c && ./a.out'`


See what's being sent over the websocket

- `wscat --connect ws:localhost:8081/chat`
