What sensors are used? What type of data is required? How are
the sensors integrated?

# ESP-32
The ESP-32 will be interfaced with the base node.
Upon receiving notification a hand is present, the camera will take a photo.
The camera is able to take a photo as a jpeg file which will be transmitted back to the base node.
The base node will use the jpeg data type to plug into the machine learning aspect to categorise the photo taken.

# Ultrasonic
The ultrasonic sensor will be used from the Disco board.
The ultrasonic sensor captures a return signal and changes the signal into a distance.
The ultrasonic can be configured for different specificities, however, the sensor will utilise cm range.
This means the data required is cm distance to the object.
The sensor is integrated via wifi from the disco board to the base node, this will then allow the camera to begin operation once a person has entered within range.

# Microphone

# Temperature
The HTS221 sensor will be used from the Thingy52 board using the Sensor API.
The sensor value is configured to use the I2C protocol.
It provides a temperature reading of 0.5 degree accuracy, between values of 15 and 40.
It can be accessed via a pre-built zephyr library, which returns the value as a double.
The value is read from the Thingy52 every second, and is sent to the base node via a bluetooth iBeacon packet.
The packet will be sent with the double temperature reading split with the upper half as the Major and lower half the Minor