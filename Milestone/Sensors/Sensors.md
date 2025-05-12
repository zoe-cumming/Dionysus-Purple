# ESP-32
The ESP-32 will be interfaced with the PC.
Upon receiving notification a hand is present, the camera will take a photo.
The camera is able to take a photo which is a buffer of the pixels which will be used as a raw image data as a PIXFORMAT_RGB565 which will be transmitted back to the PC for analysis.
The ESP-32 will receive notification to take the photo via wifi and send the formated pixels via MQTT to the PC for analysis.
The PC will use the PIXFORMAT_RGB565 data type to plug into the machine learning aspect to categorise the photo taken and then rotate the fan (servo) at the correct speed.

# Ultrasonic
The ultrasonic sensor will be used from the Disco board.
The ultrasonic sensor captures a return signal and changes the signal into a distance.
The ultrasonic can be configured for different specificities, however, the sensor will utilise cm range.
This means the data required is cm distance to the object.
The sensor is integrated via wifi from the disco board to the base node, this will then allow the camera to begin operation once a person has entered within range.

# Microphone
The microphone (Knowles SPK0838HT4H) will be used to detect the user clapping.
The Zephyr PDM (Pulse Density Modulation) driver will be used to interface the sensor.
In the zephyr call back raw audio data is captured which can be processesd or stored.
The data can be dervied from the PDM stream via filtering and decimiation into PCM (Pulse Code Modulation) samples which are typically 16-bit integers.
From this a sampling rate of atleast 8kHz will be required, and peaks in apmlitude over short intervals can be determined as claps.
The clap will be analysed on the thingy52 and then claps will be sent via BLE to the base node with the clap in the major.

# Temperature
The HTS221 sensor will be used from the Thingy52 board using the Sensor API.
The sensor value is configured to use the I2C protocol.
It provides a temperature reading of 0.5 degree accuracy, between values of 15 and 40.
It can be accessed via a pre-built zephyr library, which returns the value as a double.
The value is read from the Thingy52 every second, and is sent to the base node via a bluetooth iBeacon packet.
The packet will be sent with the double temperature reading split with the upper half as the Major and lower half the Minor