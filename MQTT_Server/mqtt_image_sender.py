import paho.mqtt.client as mqtt

client = mqtt.Client()
client.connect("localhost", 1883)

with open("sample.jpg", "rb") as f:
    img = f.read()

client.publish("image/upload", img)
print("Image sent to server.")
client.disconnect()
