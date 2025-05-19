# MQTT Image Transfer System

This project demonstrates how to send and receive JPEG images over Wi-Fi using the MQTT protocol. The system uses a **Mosquitto MQTT broker** and **Python clients** based on the `paho-mqtt` library. Images are published by one client, saved and forwarded by an intermediate server-side client, and optionally received and saved again by a receiver client.

---

## Components

- **MQTT Broker**: [Eclipse Mosquitto](https://mosquitto.org/)
- **Language**: Python (`paho-mqtt`)
- **Tools**: `mosquitto_pub`, `mosquitto_sub`, `mosquitto` (broker)

---

## Installation & Setup of Mosquitto Broker
```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
```

## Start the Broker
```bash
mosquitto
```

## Quick Test: Mosquitto Local Pub/Sub
### Terminal 1 (Subscriber)
```bash
mosquitto_sub -h localhost -t "test/topic"
```
### Terminal 2 (Publisher)
```bash
mosquitto_pub -h localhost -t "test/topic" -m "hello"
```
Expected output: 'hello' appears in Terminal 1.

## Project Architecture
```
[ Image Sender (ESP32 or Python script) ]
               |
         (Publishes image to MQTT)
               ↓
     [ Mosquitto MQTT Server (Broker) ]
               ↑
         (Subscribes to image topic)
     [ PC Client (Python) Receives image ]
```

---

## Python Scripts

### `mqtt_image_sender.py`
Publishes a JPEG image to the MQTT topic `image/upload`.

### `mqtt_image_store_and_forward.py`
Subscribes to `image/upload`, saves the image to a local folder (`stored_images/`), and republishes the image on topic `image/stored`.

### `mqtt_image_receiver.py` *(optional)*
Subscribes to `image/stored` and saves the received image locally as `latest_from_server.jpg`.

---

## Local Testing Workflow

### Step 1: Confirm Broker is Running
```bash
sudo systemctl status mosquitto
```
If it's not active:
```bash
sudo systemctl start mosquitto
```

### Step 2: Run the Image Store and Forward Script
```bash
python3 mqtt_image_store_and_forward.py
```
Expected output:
```
Connected with code 0

```

### Step 3: Run the Image Sender Script
Make sure a JPEG file like `sample.jpg` exists in the same directory:
```bash
python3 mqtt_image_sender.py
```
New expected output in terminal 1:
```
Connected with code 0
Image saved: stored_images/image_YYYYMMDD-HHMMSS.jpg
Image re-published on topic 'image/stored'

```

### Step 4 (Optional): Run the Image Receiver Script
```bash
python3 mqtt_image_receiver.py
```

---

## Remote Access (Over Wi-Fi)

To use this setup on a local network across different devices:

1. Replace `"localhost"` with the IP address of the machine running the broker:
   ```python
   client.connect("192.168.1.42", 1883)
   ```

2. Update Mosquitto config to allow remote access:
   ```conf
   listener 1883
   allow_anonymous true
   ```

3. Restart the broker:
   ```bash
   sudo systemctl restart mosquitto
   ```

---

## Folder Structure

```
mqtt_server/
│
├── sample.jpg                      # Sample image to send
├── mqtt_image_sender.py            # Sends image to broker
├── mqtt_image_store_and_forward.py # Stores image and republishes
├── mqtt_image_receiver.py          # Receives republished image
└── stored_images/
    └── image_YYYYMMDD-HHMMSS.jpg   # Stored images
```

---