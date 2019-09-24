ARG registry
FROM ${registry}/haya:latest-builder as builder
ARG symbol=SYS

ADD . daobet
WORKDIR daobet

RUN git submodule init && git submodule update --init --recursive

RUN echo "$(git rev-parse --abbrev-ref HEAD):$(git rev-parse HEAD)" > /etc/daobet-version \
    && cmake -H. -B"/opt/daobet" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/opt/daobet -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
    && cmake --build /opt/daobet --target install \
    && cp /daobet/Docker/config.ini / && ln -s /opt/daobet/contracts /contracts && cp /daobet/Docker/nodeosd.sh /opt/daobet/bin/nodeosd.sh && ln -s /daobet/tutorials /tutorials

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates vim psmisc python3-pip libcurl4-openssl-dev libusb-1.0 && rm -rf /var/lib/apt/lists/*
RUN pip3 install numpy
ENV EOSIO_ROOT=/opt/daobet
RUN chmod +x /opt/daobet/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/daobet/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
