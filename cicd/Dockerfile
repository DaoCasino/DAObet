FROM ubuntu:18.04

COPY ./*.deb /tmp/prepare/

RUN apt-get update && \
      apt install -y /tmp/prepare/*.deb && \
      rm -rf /tmp/prepare && \
      rm -rf /var/lib/apt/lists/*
