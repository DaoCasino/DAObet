ARG registry
FROM ${registry}/haya:latest-builder as builder
ARG symbol=BET
ARG build_type

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install ccache

ADD . daobet
WORKDIR daobet
ENV CCACHE_DIR /daobet/.ccache

RUN git submodule init && git submodule update --init --recursive

RUN echo "$(git rev-parse --abbrev-ref HEAD):$(git rev-parse HEAD)" > /etc/daobet-version \
    && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=${build_type} -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
       -DENABLE_TX_SPONSORSHIP=1 \
    && cmake --build /tmp/build --target install


FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates libcurl4-openssl-dev libusb-1.0 gdb && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/daobet/bin
COPY --from=builder /daobet/Docker/config.ini /
COPY --from=builder /etc/daobet-version /etc
COPY --from=builder /daobet/Docker/daobet-node.sh /opt/daobet/bin/daobet-node.sh
ENV EOSIO_ROOT=/opt/daobet
RUN chmod +x /opt/daobet/bin/daobet-node.sh
RUN ulimit -c unlimited
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/daobet/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
