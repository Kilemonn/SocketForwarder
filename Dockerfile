FROM alpine:3.20.0 AS builder

WORKDIR /builder

RUN apk update && apk upgrade && apk add g++ cmake make git bluez-dev glib-dev bluez

COPY ./socket-forwarder ./socket-forwarder
COPY ./CMakeLists.txt ./CMakeLists.txt

RUN ["cmake", "-B", "build", "."]

WORKDIR /builder/build
RUN ["make"]

FROM alpine:3.20.0 AS runner

WORKDIR /socket-forwarder

COPY --from=builder /builder/build/SocketForwarder /socket-forwarder/SocketForwarder

RUN ["./SocketForwarder"]
