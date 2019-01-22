# Chapter 4: Elementary TCP Sockets

![Socket functions for elementary TCP client/server](pics/4.1.png)   

-------------------
## `socket` function
``` c
// From sys/socket.h
int socket(int family, int type, int protocal); // Returns non-negative descriptor if OK, -1 on error.
```   

`family` sepcifies the protocal family, this argument is also referred as `domain`; `type` is the socket type; `protocal` is a valid protocal type, or set to `0` to select the system's default for the given combination of `family` and `type`.   

This table is for argument `family`:

| family   |   Description   |
|----------|-----------------|
| AF_INET  | IPv4 protocals  |
| AF_INET6 | IPv6 protocals  |
| AF_LOCAL | Unix domain protocals |
| AF_ROUTE | Routing sockets |
| AF_KEY   | Key socket      |

This table is for argument `type`:

|        type         |             Description                 |
|---------------------|-----------------------------------------|
|     SOCK_STREAM     |         stream socket                   |
|     SOCK_DGRAM      |         data socket                     |
|     SOCK_SEQPACKET  |         sequenced packet socket         |
|     SOCK_RAW        |         raw socket                      |

This table is for argument `protocal`, only the ones for `AF_INET` and `AF_INET6` are listed:

|    protocal      |   Descripption   |
|------------------|------------------|
|  IPPROTO_TCP     | TCP transport protocal |
|  IPPROTO_UDP     | UDP transport protocal |
|  IPPROTO_SCTP    | SCTP transport protocal |


Combinations of `family` and `type` for the socket function.

|                |    AF_INET    |    AF_INET6    |    AF_LOCAL    |    AF_ROUTE    |    AF_KEY    |
|----------------|---------------|----------------|----------------|----------------|--------------|
|   SOCK_STREAM  | TCP|SCTP      |   TCP|SCTP     | Yes            | | |
|   SOCK_DGRAM   | UDP           |   UDP          | Yes | | |
| SOCK_SEQPACKET | SCTP          | SCTP           | Yes | | | |
| SOCK_RAW       | IPv4          | IPv6           |  | Yes  | Yes  |    

The first argument for `socket` may be PF_xxx, it will be explained later.   

`AF_UNIX` (The historical UNIX name) may be encountered instead of `AF_LOCAL`.   

Linux provides `SOCK_PATCKET` type, used to access the datalink.   

There are also other `family`s and `type`s for `socket` function.   

The non-negative return value of `socket` is a `socket descriptor` or `sockfd`, similar to a file descriptor.   

The family `AF_xxx` stands for `address family` and `PF_xxx` stands for `protocal family`. They are of the same value in most situations. To avoid confusion, we only use `AF_xxx` now.   

-------------
## `connect` function
``` c
// From sys/socket.h
int connect(int sockfd, const struct sockaddr *servaddr, socklen_t addrlen);
```

`sockfd` is the return value of function `socket`. the `servaddr` and `addrlen` must be setted before calling. `servaddr` is the socket structure contains server ip and corresponding port, `addrlen` is the length of the socket structure. It is not needed to call `bind` before calling `connect` on client, because the kernel will set a ephemeral port and the source IP address if necessary.   

`connect` initiates the TCP three-way handshake and only returns when the connection is successfully established or an error occurs. Its possible return values are stated below:

| Return value | Description |
|--------------|-------------|
| 0 | OK |
| ETIMEDOUT | The server does not respond. (On 4.4BSD if the client does not get a ACK from server after 75 seconds from the sending of SYN, this value is returned) |
| ECONNREFUSED | The server sends a reset (RST) back, in order to reject the connection. |
| EHOSTUNREACH/ENETUNREACH | When ICMP "destination unreachable" returned, it is considered a "soft error". The kernel keeps sending SYN. After a fixed time of trying (75 seconds on 4.4BSD), if it is still failed, it returns this value. And, the `ENETUNREACH` is obsolete now. |
| EADDRINUSE | Address already in use. |

Function `connect` moves the TCP state from `CLOSED` to `SYN_SENT`. If it succeeds, the state is moved to `ESTABLISHED`. If it fails, this socket is no longer usable and closed.   

We cannot call `connect` twice on one socket. If the previous `connect` fails, the socket must not be reused.     

-----------------------
## `bind` function

``` c
// From sys/socket.h
int bind(int sockfd, const struct sockaddr *myaddr, socklen_t);
```

The man page of `bind` function says "bind assagins a name to an unnamed socket." And in fact, `bind` has nothing to do with name, it assigns a protocal address to a socket, and what that protocal address means depends on the protocal.   

If `bind` is not called on a socket, the kernel will assign a ephemeral port to it when `connect` or `listen` is called.   

The IP address used for `bind`ing must be a valid ip of the host.   

If TCP server does not bind an IP address to the socket, the kernel uses the IP address specified by the incoming SYN.   

For the addr and port specified by proceses, we have the following results of `bind`:   

| IP address | port | Result |
|------------|------|--------|
| Wildcard   |   0  | Kernel chooses IP address, address and port |
| Wildcard   | nonzero | Kernel chooses IP address, process specifies port |
| Local IP address | 0 | Process specifies IP address, kernel chooses port |
| Local IP address | nonzero | Process specifies IP address and port |

In IPv4, the wildcard IP address is `INADDR_ANY`, usually a 32-bit constant 0, defined in "/include/uapi/linux/in.h" on Linux. The IPv6 wildcard address is a variable `in6addr_any`, defined in "/include/linux/in6.h" on Linux.     

If we let `bind` choose one ephemeral port for us, we cannot get the chosen port from the socket struct because the argument for socket is constant in `bind`. If we want to get the port, we need to use function `getsockname`. We can also use `getsockname` on server to get the IP address of the interface from which the client's package flows in. Be aware that the interface of the IP address where we get the client's message is usually not the remote host IP of our socket.   

---------------
## `listen` function
``` c
// From sys/socket.h
int listen(int sockfd, int backlog);
```
This function return 0 if OK.    

This function turns an unconnected socket into a passive socket. This function moves socket from `CLOSED` state to `SYN_REVD` state and then `LISTEN` state.    

The argument `backlog` indicates the maximum number of connections the kernel should queue for this socket: the sockets that are created by this listening socket but not `accept`ed yet.   

In some implememtations, the real `backlog` is the 1.5 times the `backlog` passed to this function.   

Do not use 0 `backlog`, on some platforms this leads to problems. If no connection from this socket is expected, just close this socket.   

On some old platforms, this maximum `backlog` allowd for this argument is 5.    

On most busy web servers the sockets left in `SYN_RCVD` are quite many. The `listen` function mantains two queues, one for sockets in `SYN_RCVD` state waiting for the host respond the remote clients with a 'SYN, ACK' message, and another one for the sockets successfully `ESTABLISHED` but not been `accept`ed. The `backlog` is the maximum sum of the lengths of these two queues. And if it is full, TCP ignores the following incoming `SYN`s. It won't send `RST` because when the client retransmit the `SYN`, the queues may not be full anymore. Before being `accept`ed, if any datagram sent in, it would be stored in the socket's buffer.     

This function is normally called after both the `socket` and `bind` (if `bind` is needed) functions and must be called before calling the `accept` function.   

---------------------
## `accept` function
`accept` is called by a TCP server to return the next completed connection from the front of the completed connection queue. If the completed connection queue is empty, the process is put to sleep (assuming the default to a blocking socket).   
``` c
// From sys/socket.h
int accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
```

This function returns a non-negative descriptor if OK, -1 on error.   

The arguments `cliaddr` and `addrlen` are used to return the protocal address of the connected peer process (the client). And before calling, we set `addrlen` to the actual size of the structure pointed by `cliaddr`.   

We name the socket passed to `accept` the `listening socket`, and we name the socket it returns the `connected socket`.   

If we are not interested in the socket returned, we can set `cliaddr` and `addrlen` to NULL, then `accept` will not set any socket structure but only return the descriptor.   
------------------
## `close` function
The `close` function is used to terminate the TCP connection. It is just the normal `close` function we use to close a file descriptor.   

``` c
//From unistd.h
int close(int fd);
```

This function returns 0 of OK.   

After the socket is actually `close`d (The socket descriptor's reference count is decreased to 0), it cannot be used for `read`ing or `write`-ing, but that does not mean the TCP communitation stopped.   

Like normal `close`-ing usage, this function decreased the descriptor's reference count, only make a destruction when the count reaches 0. If we really want to stop a TCP connection, we can use the `shutdown` function. And, because of the reference count, if we use a concurrent server program and in which a main thread is for listening, other threads are for connecting, we need to be careful with the connection sockets' refrence count. It is common both the listening thread and the connecting thread hold one descriptor, which was returned from `accept`. If this happends, the listening thread must `close` the connection socket. If we don't do so, the listening thread may eventually hold too many descriptors and reaches the maximum count set by the OS, and then OS will prevent it from acquiring more sockets; and the connection thread's `close`-ing will not actually close the TCP connection, leaving it hanging until the listening thread's corresponding `close` called or the client closes it. So one of the best choice is, after the `accept` returns, we fork/vfork/clone, and then we immediately close it in the listening thread.     

---------------------------
## `getsockname` and `getpeername` functions
They are really intuitive:
``` c
//From sys/socket.h
int getsockname(int sockfd, struct sockaddr *localaddr, socklen_t *addrlen);
int getpeername(int sockfd, struct sockaddr *peeraddr, socklen_t *addrlen);
```

Thses functions return 0 if OK.   

And, still, we need to allocate the target `sockaddr` structs for them, pass the pointers as `localaddr`/`peeraddr`, and set ```*addrlen``` to their lengths. After returning, the struct will be filled with the information we required, and the ```*addrlen```s will be set to the used length.   

