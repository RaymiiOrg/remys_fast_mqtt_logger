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
    rsyslog \
    supervisor \
    mosquitto-clients \
    && rm -rf /var/lib/apt/lists/*

COPY docker-rsyslog.conf /etc/rsyslog.conf
COPY docker-supervisord.conf /etc/supervisor/supervisord.conf

RUN mkdir build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make \
    && make install

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]
