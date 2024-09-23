# SocketForwarder
A TCP and UDP forwarder that creates common sessions and forwards incoming packets to all sockets registered for that session.

## Quick Start

### From Command-line

On commandline the application is accepting 2 `unsigned short` arguments as the listening port numbers for TCP and UDP forwarders respectively.

**NOTE:** At the moment from the command line, you must provide a valid TCP port in order to provide and start up the UDP forwarder. This is due to the ordering of the commandline arguments without any flags.

Example command structure:
``` bash
./SocketForwarder <TCP-PORT> <UDP-PORT>
```

Example command:
``` bash
./SocketForwarder 6753 33345
```

*Please review the Docker Image section to understand the available environment variables.*

### From Docker Image

Image available at: https://hub.docker.com/r/kilemon/socket-forwarder

**Make sure you `EXPOSE` the required ports that you wish to use as listener ports. By default the image exposes no ports.**

Running the container allows you to customise the forwarder in specific ways as per the environment variables that you provide on startup to the application.
The environment variables that can be provided are as follows:

---

#### socketforwarder.tcp.port

*If not provided the TCP forwarder will not run.*

This is the port that the TCP forwarder will listen on for new connections to join a forwarder group.

---

#### socketforwarder.udp.port

*If not provided the UDP forwarder will not run.*

This is the port that the UDP forwarder will listen on for all messages.

---

#### socketforwarder.new_client_prefix

*If not provided this will default to **"SOCKETFORWARDER-NEW:"**.*

For TCP:
- When a client connects to the TCP listening port, its first message should begin with this prefix following by an identifier string that is unique to the group that they wish to forward to and receive from.

E.g.
> `SOCKETFORWARDER-NEW:my-group-ID-1234567890`

For UDP:
- When a client wishes to be added to the UDP forwarding group (there is only a single UDP group since we cannot categorise connections and determine who sent what using UDP. For multiple UDP forwarder groups you would need to run multiple instances of this application). The message sent must begin with this prefix then followed by the `port number` that they will be listening to responses from. This allows the forwarder to store this provided port and the incoming address to forward future data from the UDP group to this connection.
- Any message sent to the UDP forwarder listening port that does not begin with this prefix will be assumed that it is a message to be forwarded to all known peers.

E.g.
> `SOCKETFORWARDER-NEW:44567`

---

#### socketforwarder.max_read_in_size

*If not provided this will default to **10240**.*

This will be the maximum read size for all TCP and UDP socket read operations.

---

#### socketforwarder.debug

*If not provided this is 'false' or disabled by default.*

This will enable more logging of messages received, timing taken to forward and the amount of clients in each forwarder group.

---

#### socketforwarder.host_address

*If not provided the value "0.0.0.0" is used.*

This will be the address that the application binds to for both TCP and UDP.

---

#### socketforwarder.tcp.preconfig_addresses


---

#### socketforwarder.udp.preconfig_addresses


---
