ARG builder=master
FROM cyberway/builder:$builder as builder
ARG symbol=CYBER
ARG compiletype=RelWithDebInfo
ARG versionstring

ARG SONAR_SCANNER_HOME=/opt/sonar-scanner
ARG BUILD_WRAPPER_HOME=/opt/build-wrapper
ARG SONAR_LOGIN

ENV SONAR_SCANNER_HOME=${SONAR_SCANNER_HOME} \
    SONAR_SCANNER_VERSION=4.2.0.1873 \
    SONAR_LOGIN=${SONAR_LOGIN} \
    BUILD_WRAPPER_HOME=${BUILD_WRAPPER_HOME} \
    PATH=${SONAR_SCANNER_HOME}/bin:${BUILD_WRAPPER_HOME}:${PATH} \
    SONAR_SCANNER_OPTS="-server"

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates git wget unzip xz-utils pylint \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN wget -q -O /opt/sonar-scanner-cli.zip https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-${SONAR_SCANNER_VERSION}-linux.zip \
    && unzip sonar-scanner-cli.zip \
    && rm sonar-scanner-cli.zip \
    && mv sonar-scanner-${SONAR_SCANNER_VERSION}-linux ${SONAR_SCANNER_HOME} \
    && wget -q -O /opt/build-wrapper.zip https://sonarcloud.io/static/cpp/build-wrapper-linux-x86.zip \
    && unzip build-wrapper.zip \
    && rm build-wrapper.zip \
    && mv build-wrapper-linux-x86 ${BUILD_WRAPPER_HOME}

COPY . ./cyberway

RUN cd cyberway \
    && cmake -H. -B"/tmp/build" \
        -GNinja \
        -DVERSION_STRING=$versionstring \
        -DCMAKE_BUILD_TYPE=$compiletype \
        -DWASM_ROOT=/opt/wasm \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_INSTALL_PREFIX=/opt/cyberway \
        -DBUILD_MONGO_DB_PLUGIN=false \
        -DCORE_SYMBOL_NAME=$symbol \
    && build-wrapper-linux-x86-64 --out-dir bw-output cmake --build /tmp/build --target install

RUN sonar-scanner \
  -Dsonar.organization=cyberway \
  -Dsonar.projectKey=cyberway_cyberway \
  -Dsonar.sources=. \
  -Dsonar.host.url=https://sonarcloud.io \
  -Dsonar.cfamily.build-wrapper-output=bw-output \
  -Dsonar.login=${SONAR_LOGIN}

FROM ubuntu:18.04
ARG revision

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates libusb-1.0-0-dev libcurl4-gnutls-dev \
    && rm -rf /var/lib/apt/lists/* \
    && echo "$revision" > /etc/cyberway-version

COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /cyberway/Docker/config.ini /

COPY --from=builder /opt/cyberway /opt/cyberway
COPY --from=builder /cyberway/Docker/nodeosd.sh /opt/cyberway/bin/nodeosd.sh
COPY --from=builder /cyberway/Docker/run-with-events.sh /opt/cyberway/startup/run-with-events.sh
COPY --from=builder /cyberway/Docker/docker-compose-events.yml /opt/cyberway/startup/docker-compose-events.yml
COPY --from=builder /cyberway/Docker/mongodb-exporter /opt/cyberway/startup/mongodb-exporter
COPY --from=builder /cyberway/Docker/nats /opt/cyberway/startup/nats
COPY --from=builder /cyberway/tests/test_api/ /opt/cyberway/tests/test_api/

ENV CYBERWAY_ROOT=/opt/cyberway
RUN chmod +x /opt/cyberway/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/cyberway/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
