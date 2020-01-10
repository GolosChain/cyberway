ARG cw_imagetag=stable
ARG system_contracts_tag=stable

FROM cyberway/cyberway.contracts:$system_contracts_tag as cyberway.contracts
FROM cyberway/cyberway:$cw_imagetag as cyberway

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install python3 python3-pymongo python3-requests python3-testfixtures socat \
    && rm -rf /var/lib/apt/lists/*

COPY --from=cyberway.contracts /opt/cyberway.contracts/base-genesis/ /opt/cyberway/genesis

RUN mkdir /opt/cyberway/data-dir

