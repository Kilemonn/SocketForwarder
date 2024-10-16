# SocketForwarder
A TCP and UDP forwarder that creates common sessions and forwards incoming packets to all sockets registered for that session.

Please review the [Wiki](https://github.com/Kilemonn/SocketForwarder/wiki) for more specific argument and configuration notes.

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

Please see the [Wiki](https://github.com/Kilemonn/SocketForwarder/wiki) for more detail on the arguments and configuration options.

For quick start you need to match the exposed TCP and UDP ports respectively to the environment variables:
- `socketforwarder.tcp.port`
- `socketforwarder.udp.port`

This will tell the application which ports to list on for new connections. More information about the underlying workings of the forwarder and its groups are in the wiki.

E.g. Dockerfile:

```dockerfile
FROM kilemon/socket-forwarder:latest

# Matches exposed ports below
ENV socketforwarder.tcp.port=34765
ENV socketforwarder.udp.port=11234

EXPOSE 34765/tcp
EXPOSE 11234/udp
```
