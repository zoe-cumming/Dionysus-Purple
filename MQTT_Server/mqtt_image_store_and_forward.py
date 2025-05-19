import os
import time
import paho.mqtt.client as mqtt

# Create a folder for storing images
IMAGE_FOLDER = "stored_images"
os.makedirs(IMAGE_FOLDER, exist_ok=True)

def on_connect(client, userdata, flags, rc):
    print("Connected with code", rc)
    client.subscribe("image/upload")

def on_message(client, userdata, msg):
    # Generate a timestamp-based filename
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    filename = f"{IMAGE_FOLDER}/image_{timestamp}.jpg"

    # Save the received image
    with open(filename, "wb") as f:
        f.write(msg.payload)
    print(f"Image saved: {filename}")

    # Re-publish the image to another topic
    client.publish("image/stored", msg.payload)
    print("Image re-published on topic 'image/stored'")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

# Replace with actual broker IP if running remotely
client.connect("localhost", 1883)
client.loop_forever()
