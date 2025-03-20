# Remy's Fast MQTT Logger 

**Remy's Fast MQTT Logger** is a C++ 17 application that connects 
to an MQTT broker, subscribes to a topic, 
and logs messages received from the broker 
to **syslog** (with configurable facility) and 
optionally to **stdout**. 

Every message is logged on a separate thread, which
is what makes is fast and able to process
thousands of messages per second. Using Pipe Viewer
to measure lines per second from a test broker:

    $ ./remys_fast_mqtt_logger -b test.mosquitto.org:1883 | pv --line-mode --rate --average-rate >/dev/null
    [4.79k/s] (3.35k/s)

This is similar to `mosquitto_sub`:

    $ mosquitto_sub -h test.mosquitto.org -p 1883 -t "#"  | pv --line-mode --rate --average-rate >/dev/null
    [3.39k/s] (3.37k/s)

It is designed for use cases where logging MQTT 
message data needs to be centralized or handled 
by a logging service like **syslog**.

## Features
- Connects to an MQTT broker with user authentication (username and password).
- Subscribes to a topic (`#` by default, can be customized via argument).
- Logs received MQTT messages to **syslog** with customizable log **facility**.
- Optionally logs messages to **stdout**.

## Dependencies
The following libraries are required to build and 
run the application:

1. `PAHO MQTT C++ Client`: The application uses the [Paho MQTT C++ library](https://www.eclipse.org/paho/).
2. `syslog.h`: For logging messages to **syslog**.


### Building
**Install dependencies**:

- On **Ubuntu/Debian**:

```bash
      sudo apt install libpaho-mqttpp3-dev libsyslog-dev build-essential
```
- On **RedHat/CentOS**:

```bash
      sudo yum install paho-mqtt-c++ syslog-devel gcc-c++ make
```

**Clone or download the repository**:
-  Clone the repository from GitHub or download the source code to your local system.

**Build the application**:

 To compile the application, use CMake:

    cd remys_fast_mqtt_logger
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release  ..

Or manually compile using `g++`:

```bash
g++ -std=c++11 -o remys_fast_mqtt_logger main.cpp -lpaho-mqttpp3 -lsyslog -lstdc++ -pthread
```

## Usage

The binary accepts the following arguments:

```bash
remys_fast_mqtt_logger [OPTIONS]
```

### Options:

The program can be configured with command line 
options or ENVIRONMENT variables. The latter don't end up
in your shell history.

#### Command line arguments

- `-b BROKER, --broker=BROKER`
    - The MQTT broker URL (including the port) to connect to. 
    - Example: `test.mosquitto.org:1883`.

- `-t TOPIC, --topic=TOPIC`
    - The MQTT topic to subscribe to. 
    - Default is `#` (subscribe to all topics).

- `-u USERNAME, --username=USERNAME`
    - Username for authenticating with the MQTT broker.

- `-p PASSWORD, --password=PASSWORD`
    - Password for authenticating with the MQTT broker.

- `-f FACILITY, --facility=FACILITY`
    - The syslog facility to log to. Must prefix with `LOG_`. 
    - Default is `LOG_LOCAL6`.

- `-s, --no-log-to-stdout`
    - Disable logging to stdout. 
    - Default is **disabled**, meaning messages will be logged to both **stdout** and **syslog** unless specified.


#### Environment Variables

You can configure the application using the following 
environment variables:

- `BROKER`: MQTT broker URL (e.g., mqtt.example.com:1883).
- `USERNAME`: MQTT username.
- `PASSWORD`: MQTT password.
- `TOPIC`: MQTT topic to subscribe to (default: #).
- `FACILITY`: Syslog facility (default: LOG_LOCAL6).

### Example Commands:

**Basic usage**:

```bash
./remys_fast_mqtt_logger -b "mqtt.example.com:1883" \
    -t "home/livingroom/temperature" \
    -u "user" \
    -p "password"
```

**Log to a specific syslog facility**:

```bash
./remys_fast_mqtt_logger -b "mqtt.example.com:1883" \
    -t "home/livingroom/temperature" \
    -u "user" \
    -p "password" \
    -f "LOG_INFO"
```

**Disable logging to stdout**:

```bash
./remys_fast_mqtt_logger -b "mqtt.example.com:1883" \
    -t "home/livingroom/temperature" \
    -u "user" \
    -p "password" \
    -s
```


### Syslog Facilities
By default, logs are written to the **`LOG_LOCAL6`** facility. 
You can change this using the `-f` argument.

Below is a list of valid syslog facilities that can be used:

- `LOG_AUTH`
- `LOG_AUTHPRIV`
- `LOG_CRON`
- `LOG_DAEMON`
- `LOG_FTP`
- `LOG_KERN`
- `LOG_LPR`
- `LOG_MAIL`
- `LOG_NEWS`
- `LOG_SYSLOG`
- `LOG_USER`
- `LOG_UUCP`
- `LOG_LOCAL0`
- `LOG_LOCAL1`
- `LOG_LOCAL2`
- `LOG_LOCAL3`
- `LOG_LOCAL4`
- `LOG_LOCAL5`
- `LOG_LOCAL6` (default)
- `LOG_LOCAL7`


### Example of Viewing Logs in Syslog:
To view logs for `LOG_LOCAL6` in **syslog** or **journalctl**:

```bash
journalctl SYSLOG_FACILITY=22
```

Or to view logs directly in **syslog**:

```bash
tail -f /var/log/syslog
```

### rsyslog and logrotate config

You can save logs to `/var/log/mqtt_msgs.log` if you have configured **rsyslog** accordingly:

    sudo nano /etc/rsyslog.d/30-local6.conf

Add:

    local6.*    /var/log/mqtt_msgs.log

Then:

    sudo touch /var/log/mqtt_msgs.log
    sudo chmod 644 /var/log/mqtt_msgs.log
    sudo chown syslog:adm /var/log/mqtt_msgs.log


Configure `logrotate` to not delete these logs:

    sudo nano /etc/logrotate.d/remys_fast_mqtt_logger

Contents:

    /var/log/mqtt_msgs.log {
        missingok
        notifempty
        size 100M
        rotate 9999
        compress
        delaycompress
        create 0644 root root
        sharedscripts
        postrotate
            systemctl reload rsyslog > /dev/null 2>&1 || true
        endscript
    }


## Build and Run with Docker

To build and run the application with Docker, 
use the following commands inside the git checkout.

Build the Docker image:

    docker build -t mqtt-logger .

Or, use my image and run the Docker container:

    docker run -d --name remys_fast_mqtt_logger --env BROKER=test.mosquitto.org raymii/remys_fast_mqtt_logger:latest 
    docker logs -f remys_fast_mqtt_logger

There is also a `docker-compose.yml` file included
which you can edit and use:

    docker compose up -d
    docker compose logs -f


    [+] Running 1/1
    ✔ Container mqtt_logger-remys-fast-mqtt-logger-1  Created0.0s
    Attaching to remys-fast-mqtt-logger-1
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/current', qos='0', retained='true', msg='1.87'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/efficiency', qos='0', retained='true', msg='95.015'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/frequency', qos='0', retained='true', msg='49.96'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/power', qos='0', retained='true', msg='442.2'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/powerfactor', qos='0', retained='true', msg='0.999'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/temperature', qos='0', retained='true', msg='28.6'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/voltage', qos='0', retained='true', msg='236.3'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/yieldday', qos='0', retained='true', msg='5547'
    remys-fast-mqtt-logger-1  | topic='/116484256345/0/reactivepower', qos='0', retained='true', msg='0.3'
    remys-fast-mqtt-logger-1  | topic='SHRDZM/483FDA46C2EE/483FDA46C2EE/sensor', qos='0', retained='false', msg='{
    remys-fast-mqtt-logger-1  | "lasterror":"cipherkey not set!",
    Gracefully stopping... (press Ctrl+C again to force)
    [+] Stopping 1/1


## License

This project is licensed under the GNU AGPLv3 License. 
See the [LICENSE](LICENSE) file for more details.

