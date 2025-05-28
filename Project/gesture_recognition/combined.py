################################################
# Combine the MQTT server functionality with
# reading in the image data from the ESP32S3
################################################

# Import packages 
import socket
import numpy as np
import cv2
import os
import threading
import queue
from paho.mqtt import client as mqtt_client
import random
import time

# Import ML model
import tensorflow as tf
from tensorflow.keras.models import load_model
from tensorflow.keras.preprocessing.image import load_img, img_to_array

# Define queues
frame_queue = queue.Queue(maxsize = 5)
pred_queue = queue.Queue(maxsize = 5)

# Define constants for reading image data
HOST = '172.20.10.10'
PORT = 5000
WIDTH = 240
HEIGHT = 240
FRAME_SIZE = WIDTH * HEIGHT * 2
READY_SIZE = 5

# Define MQTT constants
broker = 'broker.emqx.io'
port = 1883
topic = 'python/pc'
client_id = 'zephyr_subscriber'
username = 'emqx'
password = 'public'

# Define ML model
model = tf.keras.models.load_model('gesture_model2.keras')
labels = ['0', '1', '2', '3', '4', '5', 'none']

##################################################
# Thread to classify the image using the ML model
##################################################
def image_recognition(frame_queue, pred_queue):
    while True:
        bgr_img = frame_queue.get()
        img_array = img_to_array(bgr_img) / 255.0
        img_array = np.expand_dims(img_array, axis=0)
        prediction = model.predict(img_array)
        predicted_class = np.argmax(prediction)
        confidence = np.max(prediction)

        print(f"Predicted Gesture: {labels[predicted_class]} (Confidence: {confidence:.2f})")
        if confidence > 0.5:
            pred_queue.put_nowait(labels[predicted_class])
        else:
            pred_queue.put_nowait('none')

##################################################
# Function to convert the RGB565 image data to 
# RGB888
##################################################
def rgb565_to_rgb888(frame):
    # Read the data using byteswap for little endian
    data = np.frombuffer(frame, dtype=np.uint16).byteswap().reshape((HEIGHT, WIDTH))

    r = ((data >> 11) & 0x1F) << 3
    g = ((data >> 5) & 0x3F) << 2
    b = (data & 0x1F) << 3

    r |= (r >> 5)
    g |= (g >> 6)
    b |= (b >> 5)

    rgb = np.stack((r, g, b), axis=-1).astype(np.uint8)
    return rgb

##################################################
# Function to receive READY message and image 
# data from ESP32S3 server
##################################################
def recv_exact(sock, size, timeout=5):
    sock.settimeout(timeout)
    data = bytearray()
    try:
        while len(data) < size:
            remaining = size - len(data)
            packet = sock.recv(remaining)
            if not packet:
                print("Socket closed/ Connection lost")
                return None
            data.extend(packet)
        return data
    except socket.timeout:
        print("Socket timed out while waiting for data.")
        return None
    except socket.error as e:
        print(f"Socket error: {e}")
        return None

##################################################
# Thread to connect to the TCP socket created by 
# the ESP32S3_EYE
# Display the images using cv2
##################################################
def tcp_receiver(frame_queue):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    print(f"Connected to {HOST}:{PORT}")

    while True:
        ready = recv_exact(s, READY_SIZE)
        if ready != b'READY':
            print("Error:", ready)
            return
        frame = recv_exact(s, FRAME_SIZE)
        if frame is None:
            print("Connection closed / recv_exact failure")
            break

        rgb_img = rgb565_to_rgb888(frame)
        bgr_img = cv2.cvtColor(rgb_img, cv2.COLOR_RGB2BGR)
        cv2.imshow("ESP32 Frame", cv2.resize(bgr_img, (480, 480), interpolation=cv2.INTER_NEAREST))
        bgr_img = cv2.resize(bgr_img, (224, 224))
        frame_queue.put_nowait(bgr_img)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    s.close()
    cv2.destroyAllWindows()

##################################################
# Function to connct to MQTT server
##################################################
def connect_mqtt():
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print("Connected to MQTT Broker!")
        else:
            print(f"Failed to connect, return code {reason_code}")

    client = mqtt_client.Client(client_id=client_id, protocol=mqtt_client.MQTTv311)
    client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client

##################################################
# Function to subscribe to MQTT topic
##################################################
def subscribe(client: mqtt_client):
    def on_message(client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")

    client.subscribe(topic, qos=0)
    client.on_message = on_message

##################################################
# Function to unsubscribe from MQTT server
##################################################
def unsubscribe(client: mqtt_client):
    client.on_message = None
    client.unsubscribe(topic)

##################################################
# Function to disconnect from MQTT server
##################################################
def disconnect(client: mqtt_client):
    client.loop_stop()
    client.disconnect()

##################################################
# Function to handle receiving messages via MQTT
##################################################    
def on_message(client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")

##################################################
# Thread to handle MQTT connection and publishing
##################################################
def mqtt_sender(pred_queue):
    client = connect_mqtt()
    client.loop_start()
    last = time.time()
    while True:
        predicted_class = pred_queue.get()
        if predicted_class == 'none':
            continue
        msg = f"{predicted_class}"
        if (time.time() - last > 2):
            result = client.publish(topic, msg)
            status = result.rc
            if status == 0:
                print(f"Send `{msg}` to topic `{topic}`")
            else:
                print(f"Failed to send message to topic {topic}")
            last = time.time()

if __name__ == "__main__":
    t1 = threading.Thread(target=tcp_receiver, args=(frame_queue,))
    t2 = threading.Thread(target=image_recognition, args=(frame_queue, pred_queue))
    t3 = threading.Thread(target=mqtt_sender, args=(pred_queue,))
    t1.start()
    t2.start()
    t3.start()