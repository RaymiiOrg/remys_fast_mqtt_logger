FROM gcc:14
WORKDIR /app

# Copy only project files required for build. Adjust as needed if you add files.
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY include/ ./include/

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       cmake \
    && rm -rf /var/lib/apt/lists/*

# Build
RUN mkdir -p build && cd build \
    && cmake .. \
    && cmake --build . -- -j$(nproc)

# Binary output location may vary depending on your CMakeLists; adjust CMD accordingly
CMD ["/app/build/remys_fast_mqtt_logger"]
