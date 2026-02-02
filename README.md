# what?

This is a simple C program that, when run, hosts a web server. This webserver serves a single page. The page, once opened, establishes a websocket connection back to the server to share what is being drawn with any other peers connected to the server.

<img src="https://github.com/user-attachments/assets/5845d122-359d-4795-9bb1-aa23daf6b208" height=420 />

# from scratch?

This program hosts an HTTP server using [Berkeley Sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) (`socket()`/`bind()`/`accept()` etc.).

It supports multiple active websockets connections on a single thread using [non-blocking IO](https://www.kegel.com/dkftpbench/nonblocking.html) (`fcntl` with `O_NONBLOCK`) and [`poll.h`](https://man7.org/linux/man-pages/man2/poll.2.html).


# dev

Rebuild whenever any of the code changes

- [`find *.c *.h | entr -rs 'clear && gcc -g page.c && ./a.out'`](https://github.com/eradman/entr)


See what's being sent over the websocket

- [`wscat --connect ws:localhost:8081/chat`](https://github.com/websockets/wscat)
