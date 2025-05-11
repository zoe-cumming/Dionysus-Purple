## Key Performance Indicators
---

| KPI  | Description                                                              |
|------|--------------------------------------------------------------------------|
| 1    | **Accuracy.** Image Recognition: The machine learning algorithm must be able to correctly identify gestures with minimum **70% accuracy.** |
| 2    | **Accuracy.** Audio Recognition: Clapping must be correctly identified with minimum **60% accuracy.** |
| 3    | **Latency.** Sensors: Ultrasonic sensor able to take **10 measurements per second.** Temperature and light sensors able to take **2 measurements per second.** ESP32 able to capture and transmit **5 frames per second.** PDM able to detect **2 distinct claps per second.** |
| 4    | **Latency.** Communication Protocols: ESP32 publishes image data to server **within 5 s.** PC reads image data from server, performs image recognition and communicates to M5 Core2 **within 2 s.** Thingy52 transmits light setting via BLE **within 0.5 s.** Disco L475-IOT01A transmits ultrasonic proximity via wifi **within 0.5 s.** Disco L475-IOT01A receives fan setting via wifi from M5 Core2 **within 0.5 s.** |
| 5    | **Displays.** M5 Core2: Fan setting display updated within 2 s of the PC reading image data from the server (when necessary). Grafana Dashboard: Updated every 5 seconds with the current temperature, light, fan setting and light setting. |
| 6    | **Robustness.** Image and audio recognition remain reliable across different users and different environmental conditions (including windy and noisy conditions). 25% variation in accuracy is acceptable in image and audio recognition accuracies for different users and environmental conditions. |

---