from influxdb_client_3 import InfluxDBClient3, Point
from paho.mqtt import client as mqtt_client
import json
import time
import serial
import sensor_pb2
import threading

TEMP = 0
INTENSITY = 1
LIGHTS = 2
FAN = 3
array = [0] * 4

# TESTING PURPOSES
array[LIGHTS] = "OFF"
array[INTENSITY] = 200
array[FAN] = 4
array[TEMP] = 30

broker = 'broker.emqx.io'
port = 1883
rec_topic = 'python/pc'
send_topic = 'python/mqtt'
client_id = f'python-mqtt-UART'
username = 'emqx'
password = 'public'

def connect_mqtt():
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print("Connected to MQTT Broker!")
        else:
            print(f"Failed to connect, return code {reason_code}")

    # Specify the correct callback API version (v5 is default)
    client = mqtt_client.Client(client_id=client_id, protocol=mqtt_client.MQTTv5)
    client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client

def subscribe(client: mqtt_client):
    def on_message(client, userdata, msg):
        received = msg.payload.decode()
        print(f"Received `{received}` from `{msg.topic}` topic")
        
        if array[LIGHTS] == "ON":
            light = 1
        else:
            light = 0
        
        msg = f"Temp: {array[TEMP]}, Lights: {light}, Fan: {received}"
        
        result = client.publish(send_topic, msg)
        status = result.rc
        if status == 0:
           print(f"Send `{msg}` to topic `{send_topic}`")
        else:
           print(f"Failed to send message to topic {send_topic}")

    client.subscribe(rec_topic, qos=0)
    client.on_message = on_message


##################################################
# Decode the NanoPB package
##################################################
def decode_nanopb(line):
    try:
        hex_string = line.split(": ")[1]
        hex_values = bytes.fromhex(hex_string)

        data = sensor_pb2.SensorData()
        data.ParseFromString(hex_values)

        array[TEMP] = data.temp // 100
        print(array[TEMP])
        
        array[INTENSITY] = data.light
        print(array[INTENSITY])
        
        if data.clap == 1:
            #Light is on:
            array[LIGHTS] = "ON"
        else:
            array[LIGHTS] = "OFF"
        print(array[LIGHTS])        

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
# Update the dashboard ever 2 seconds
##################################################
    
def dashboard_sender():
    #ser = serial.Serial('/dev/ttyACM0', 115200)
    #print("Connected to serial port...")
    last = time.time()
    while True:
        while(time.time() - last < 5):
            #data = ser.readline().decode('utf-8').strip()
            x=1
        try:
            #data = ser.readline().decode('utf-8').strip()
            #print(data)
            #result = decode_nanopb(data)
            
            # Prepare the data point for InfluxDB
            point = Point("dash") \
                .tag("node", "project") \
                .field("Temperature", array[TEMP]) \
                .field("Intensity", array[INTENSITY]) \
                .field("Light", array[LIGHTS]) \
                .field("Fan", array[FAN])           
            
            # Write the point to InfluxDB
            client.write(point)
            print(f"Wrote data to InfluxDB")
            last = time.time()

        except Exception as e:
            print(f"Error reading from serial port or writing to InfluxDB: {e}")
            break
    #ser.close()

def mqtt_sender():
    client = connect_mqtt()
    subscribe(client)
    client.loop_forever()

if __name__ == "__main__":
    t1 = threading.Thread(target=dashboard_sender, args=())
    t2 = threading.Thread(target=mqtt_sender, args=())
    t1.start()
    t2.start()