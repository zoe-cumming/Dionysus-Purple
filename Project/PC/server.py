import socket
from paho.mqtt import client as mqtt_client
import random
import time


broker = 'broker.emqx.io'
port = 1883
topic = 'python/pc'
#client_id = f'python-mqtt-{random.randint(0, 1000)}'
client_id = 'zephyr_subscriber'
username = 'emqx'
password = 'public'

def connect_mqtt():
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print("Connected to MQTT Broker!")
        else:
            print(f"Failed to connect, return code {reason_code}")

    # Specify the correct callback API version (v5 is default)
    #client = mqtt_client.Client(client_id=client_id, protocol=mqtt_client.MQTTv5)
    client = mqtt_client.Client(client_id=client_id, protocol=mqtt_client.MQTTv311)
    client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client

def subscribe(client: mqtt_client):
    def on_message(client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")

    client.subscribe(topic, qos=0)
    client.on_message = on_message

def unsubscribe(client: mqtt_client):
    client.on_message = None
    client.unsubscribe(topic)

def publish(client):
    msg_count = 0
    while True:
        time.sleep(2)
        msg = f"{msg_count}"
        #msg = "3"
        result = client.publish(topic, msg)
        # result: [0, 1]
        status = result.rc
        if status == 0:
           print(f"Send `{msg}` to topic `{topic}`")
        else:
           print(f"Failed to send message to topic {topic}")
        msg_count += 1
        
        if msg_count > 5:
            msg_count = 0

def disconnect(client: mqtt_client):
    client.loop_stop()
    client.disconnect()
    
def on_message(client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")

def run():
    client = connect_mqtt()
    client.loop_start()
    publish(client)
    
    #client = connect_mqtt()
    #subscribe(client)
    #client.loop_forever()


if __name__ == '__main__':
    run()