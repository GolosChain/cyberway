ARG cw_imagetag=stable
ARG cdt_imagetag=stable

FROM cyberway/cyberway:$cw_imagetag as cyberway
FROM cyberway/cyberway.cdt:$cdt_imagetag as cdt
FROM cyberway/builder:$cw_imagetag as builder

COPY --from=cdt /opt/cyberway.cdt /opt/cyberway.cdt
COPY --from=cyberway /opt/cyberway /opt/cyberway

ARG contract_branch=master

RUN git clone -b $contract_branch https://github.com/cyberway/cyberway.contracts --recursive

RUN cd cyberway.contracts \
    && echo ":$(git rev-parse HEAD)" > version \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/cyberway.contracts/ \
        -Dcyberway.cdt_DIR=/opt/cyberway.cdt/lib/cmake/cyberway.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
    && cmake --build build --target install 

ENV CYBERWAY_CONTRACTS /opt/cyberway.contracts/
ENV CYBERWAY /opt/cyberway/

RUN ldconfig

RUN /opt/cyberway/tests/test_api/genesis/create-genesis.sh    
    
FROM ubuntu:18.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates python3 python3-numpy python3-bson python3-pymongo python3-requests python3-testfixtures socat mc mcedit libusb-1.0-0-dev libcurl4-gnutls-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/cyberway/ /opt/cyberway/

COPY --from=builder /usr/local/lib/libbson-1.0.so.0.0.0 /usr/local/lib/
RUN ldconfig

ENV PATH /opt/cyberway/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN mkdir /opt/cyberway/data-dir

