from influxdb_client_3 import InfluxDBClient3, Point
import json
import time
import serial
import sensor_pb2

array = [0] * 4
array[2] = "OFF"
array[1] = 200
array[3] = 4
array[0] = 30

##################################################
# Decode the NanoPB package
##################################################
def decode_nanopb(line):
    try:
        hex_string = line.split(": ")[1]
        hex_values = bytes.fromhex(hex_string)

        data = sensor_pb2.SensorData()
        data.ParseFromString(hex_values)

        array[0] = data.temp
        print(array[0] / 100)
        
        array[1] = data.light
        print(array[1])
        
        if data.clap == 1:
            #Light is on:
            array[2] = "ON"
        else:
            array[2] = "OFF"
        print(array[2])
        
        array[3] = data.fan
        print(array[3])        

    except Exception as e:
        print("[ERROR] Failed to parse line:", e)
        return None

##################################################
# Define Grafana dashboard client
##################################################
client = InfluxDBClient3(
    token="uuTqUJ_ayx7z_eLScuL4-uh1RIY1d4Wm_VtPRcyrvny1tZp2fL2zQZr4NawIQs_3zeXeZc9MaCIgNFmTTSFZig==",
    host = "https://us-east-1-1.aws.cloud2.influxdata.com",
    org = "e50a638ee5109a61",
    database = "dash",
)

##################################################
# Open the serial port
##################################################
ser = serial.Serial('/dev/ttyACM0', 115200)
print("Connected to serial port...")

##################################################
# Update the dashboard ever 2 seconds
##################################################
while True:
    try:
        data = ser.readline().decode('utf-8').strip()
        print(data)
        result = decode_nanopb(data)
        
        # Prepare the data point for InfluxDB
        point = Point("dash") \
            .tag("node", "project") \
            .field("Temperature", array[0]) \
            .field("Intensity", array[1]) \
            .field("Light", array[2]) \
            .field("Fan", array[3])           
        
        # Write the point to InfluxDB
        client.write(point) # NOT WORKING ATM
        print(f"Wrote data to InfluxDB")
        last = time.time()

    except Exception as e:
        print(f"Error reading from serial port or writing to InfluxDB: {e}")
        break

#ser.close()
