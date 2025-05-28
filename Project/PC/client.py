import socket
from paho.mqtt import client as mqtt_client
import random
import time


broker = 'broker.emqx.io'
port = 1883
rec_topic = 'python/pc'
ESP_topic = 'python/mqtt'
M5_topic = 'python/mqtt/M5'
client_id = f'python-mqtt-{random.randint(0, 1000)}'
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
        #publish(client)
        #msg = f"messages: {msg_count}"
        msg = f"Temp: 10, Lights: 0, Fan: {received}"
        result = client.publish(ESP_topic, msg)
        # result: [0, 1]
        status = result.rc
        if status == 0:
           print(f"Send `{msg}` to topic `{ESP_topic}`")
        else:
           print(f"Failed to send message to topic {ESP_topic}")
        #result = client.publish(M5_topic, msg)
        # result: [0, 1]
        #status = result.rc
        #if status == 0:
        #   print(f"Send `{msg}` to topic `{M5_topic}`")
        #else:
        #   print(f"Failed to send message to topic {M5_topic}")

    client.subscribe(rec_topic, qos=0)
    client.on_message = on_message

def unsubscribe(client: mqtt_client):
    client.on_message = None
    client.unsubscribe(rec_topic)

def publish(client):
    msg_count = 0
    while True:
        time.sleep(1)
        msg = f"messages: {msg_count}"
        result = client.publish(ESP_topic, msg)
        # result: [0, 1]
        status = result.rc
        if status == 0:
           print(f"Send `{msg}` to topic `{ESP_topic}`")
        else:
           print(f"Failed to send message to topic {ESP_topic}")
        msg_count += 1

def disconnect(client: mqtt_client):
    client.loop_stop()
    client.disconnect()
    
def on_message(client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")

def run():
    #client = connect_mqtt()
    #client.loop_start()
    #publish(client)
    
    client = connect_mqtt()
    subscribe(client)
    client.loop_forever()


if __name__ == '__main__':
    run()