from influxdb_client_3 import InfluxDBClient3, Point
import json
import time
import serial
import sensor_pb2

array = [0.0] * 3

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
        array[1] = data.light
        array[2] = data.clap

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
    database = "PROJECT",
)

##################################################
# Open the serial port
##################################################
ser = serial.Serial('/dev/ttyACM0', 115200)
print("Connected to serial port...")
last = time.time()

##################################################
# Update the dashboard ever 2 seconds
##################################################
while True:
    while(time.time() - last < 2):
        pass
    try:
        data = ser.readline().decode('utf-8').strip()
        result = decode_nanopb(data)
        
        # Prepare the data point for InfluxDB
        point = Point("PROJECT") \
            .tag("node", "project") \
            .field("Temperature", array[0]) \
            .field("Light", array[1]) \
            .field("Light On/Off", array[2])          
        
        # Write the point to InfluxDB
        client.write(point)
        print(f"Wrote data to InfluxDB")
        last = time.time()

    except Exception as e:
        print(f"Error reading from serial port or writing to InfluxDB: {e}")
        break

ser.close()
