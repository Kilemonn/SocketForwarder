FROM alpine:3.20.0 AS builder

WORKDIR /builder

RUN apk update && apk upgrade && apk add g++ cmake make git bluez-dev glib-dev bluez

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

FROM alpine:3.20.0 AS runner

WORKDIR /socket-forwarder

RUN apk update && apk upgrade && apk add libstdc++

COPY --from=builder /builder/build/SocketForwarder /socket-forwarder/SocketForwarder

ENTRYPOINT ["./SocketForwarder"]
