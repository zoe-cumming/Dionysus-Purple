import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    print("Connected with code", rc)
    client.subscribe("image/stored")

def on_message(client, userdata, msg):
    with open("latest_from_server.jpg", "wb") as f:
        f.write(msg.payload)
    print("Received stored image from server.")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883)
client.loop_forever()