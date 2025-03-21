FROM gcc:14
WORKDIR /app
COPY . .
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    bash \
    libssl-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    syslog-ng \
    mosquitto-clients \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make \
    && make install

CMD ["/usr/bin/remys_fast_mqtt_logger"]
