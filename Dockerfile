# Build image with tag

# MAKE SURE VERSION IN main.cpp MATCHES

# docker build -t kilemon/socket-forwarder:0.1.0 .
# docker tag kilemon/socket-forwarder:0.1.0 kilemon/socket-forwarder:latest
#
# Push image to remote
# docker push kilemon/socket-forwarder:0.1.0

FROM alpine:3.20.2 AS builder

WORKDIR /builder

RUN apk update && apk upgrade && apk add g++ cmake make git bluez-dev glib-dev bluez gdb

COPY ./socket-forwarder ./socket-forwarder
COPY ./tests ./tests
COPY ./CMakeLists.txt ./CMakeLists.txt

RUN ["cmake", "-B", "build", "."]

WORKDIR /builder/build
RUN ["make", "all"]

# RUN echo "$(ls -l SocketForwarder)"
# RUN echo $(ls -l tests/SocketForwarderTests)

# Run the test executable directly so output logs are sent to stdout
WORKDIR /builder/build/tests
RUN ["./SocketForwarderTests"]

FROM alpine:3.20.2 AS runner

WORKDIR /socket-forwarder

RUN apk update && apk upgrade && apk add libstdc++

COPY --from=builder /builder/build/SocketForwarder /socket-forwarder/SocketForwarder

ENTRYPOINT ["./SocketForwarder"]
