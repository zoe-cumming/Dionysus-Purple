# Smart Environment Interaction System

An intelligent, multi-sensor embedded system that responds to user gestures, audio cues, and environmental data in real time. The system is designed to detect claps, measure temperature, sense presence, and recognize hand gestures to control devices like a fan and update visual displays.

---

## Project and Scenario Description

This project simulates a **smart, gesture- and audio-controlled environment** that uses sensor nodes and edge ML to provide an intuitive user experience.

Imagine a future smart home or clinical workspace where interaction with appliances or devices doesn't require physical buttons or screens. Instead, simple gestures or sounds—like a hand motion or a clap—can adjust fan speed, signal a call, or update the environment display.

**Use case example**:  
A person enters the room. The system detects proximity using an ultrasonic sensor, captures a hand gesture using a camera, and classifies it as a command—e.g., adjust fan speed or show a call icon. At the same time, ambient temperature is measured and displayed, and if a clap is detected, the system toggles feedback via LED. All of this happens through a seamless flow of wireless and wired communication between edge devices.

---

## Project Overview

This system uses a combination of BLE, UART, Wi-Fi, and MQTT-based communication to enable real-time environmental interaction. It integrates:
- Presence detection
- Gesture recognition
- Temperature monitoring
- Audio (clap) detection
- Visual display output
- Actuation via a servo-controlled fan

---

## System Architecture

### Hardware Components

| Component               | Function                                                               |
|------------------------|------------------------------------------------------------------------|
| **Thingy:52**           | BLE sensor node capturing audio (clap) and temperature data            |
| **HTS221 Sensor**       | I2C temperature sensor                                                 |
| **Knowles SPK0838HT4H** | PDM microphone sensor for clap detection                               |
| **nRF52840 DK**         | BLE base node, relays data to PC via UART                              |
| **ESP32-CAM**           | Captures JPEG images for gesture recognition                           |
| **DISCO L475-IOT1A**    | Hosts HC-SR04 ultrasonic sensor and controls servo fan                 |
| **HC-SR04 Sensor**      | Detects proximity via digital I/O                                      |
| **M5 Core2**            | Displays temperature, fan speed, or call icon                          |
| **MQTT Server**         | Transmits image and command data to/from PC                            |
| **PC**                  | Central controller and ML inference engine                             |
| **Servo Motor**         | Controlled via PWM to emulate a fan                                    |

---

## Communication Protocols

| Source → Destination     | Protocol      | Description                                     |
|--------------------------|---------------|-------------------------------------------------|
| Thingy:52 → nRF52840 DK  | BLE           | Sends temperature and clap data                 |
| nRF52840 DK → PC         | UART          | Sends sensor data                               |
| DISCO L475 → ESP32-CAM   | Wi-Fi         | Sends presence signal to trigger image capture  |
| ESP32-CAM → MQTT Server  | Wi-Fi (MQTT)  | Transmits captured image to cloud               |
| PC ↔ M5 Core2            | Wi-Fi         | Sends fan speed and icons for display           |
| M5 Core2 → DISCO L475    | Wi-Fi         | Sends fan speed setting                         |

---

## Functional Logic Flow

### Gesture Recognition & Fan Control

1. **Proximity Detection**
   - HC-SR04 (DISCO L475) detects object within 30 cm
   - Triggers ESP32-CAM to capture image

2. **Image Capture & Processing**
   - JPEG sent to MQTT server
   - PC retrieves image and performs ML classification

3. **Action Based on Image**
   - If fan gesture → PC sends speed to M5 Core2 and nRF52840 (servo control)
   - If phone symbol → PC sends icon command to M5 Core2

### Audio & Temperature Monitoring

1. **Sensor Data Collection**
   - Thingy:52 captures temp via HTS221 and audio via PDM microphone

2. **Clap Detection**
   - If a clap is detected → nRF52840 toggles LED
   - If no clap detected → continually forwards temperature reading to PC (even with clap detected)

3. **Display**
   - PC updates M5 Core2 to show current temperature
   - LED toggled on nRF52840 DK base node

---
