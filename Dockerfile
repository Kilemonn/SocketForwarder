FROM alpine:3.20.0

RUN apk update && apk upgrade && apk add g++ cmake make


