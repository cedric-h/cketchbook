# what?

This is a simple C program that, when run, hosts a web server. This webserver serves a single page. The page, once opened, establishes a websocket connection back to the server to share what is being drawn with any other peers connected to the server.

<img src="https://github.com/user-attachments/assets/5845d122-359d-4795-9bb1-aa23daf6b208" height=420 />

# from scratch?

This program hosts an HTTP server using [Berkeley Sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) (`socket()`/`bind()`/`accept()` etc.).

It supports multiple active websockets connections on a single thread using [non-blocking IO](https://www.kegel.com/dkftpbench/nonblocking.html) (`fcntl` with `O_NONBLOCK`).

My argument to support the claim that using Berkeley Sockets is making a webserver "from scratch" is the sockets API is provided by the operating system, rather than a third party. They're about as low-level as you can go while still having a program that works across platforms. Indeed, one could implement a similar collaborative drawing program using e.g. Express in Node, which would still make use of Berkeley sockets internally through libuv. More generally, you can find Berkeley sockets at the core of most programs that host HTTP servers, regardless of what language they're implemented in.

You can go lower-level and make use of operating system specific APIs for doing networking - or perhaps I should make my own RTOS and NIC driver - but choosing to use C and Berkeley sockets maintains some degree of applicability to the higher level software to make webservers that I use day-to-day, which makes it more valuable as an educational exercise.

# dev

Rebuild whenever any of the code changes

- [`find *.c *.h | entr -rs 'clear && gcc -g page.c && ./a.out'`](https://github.com/eradman/entr)


See what's being sent over the websocket

- [`wscat --connect ws:localhost:8081/chat`](https://github.com/websockets/wscat)
