import os
import numpy as np
import tensorflow as tf
from tensorflow.keras.models import load_model
from tensorflow.keras.preprocessing.image import load_img, img_to_array

# Constants
IMG_SIZE = (224, 224)
IMAGE_PATH = 'actual_images/zero.jpg'  
IMAGE_PATH1 = 'actual_images/one.jpg'
IMAGE_PATH2 = 'actual_images/two.jpg'
IMAGE_PATH3 = 'actual_images/three.jpg'
IMAGE_PATH4 = 'actual_images/four.jpg'
IMAGE_PATH5 = 'actual_images/five.jpg'


# Load model
model = tf.keras.models.load_model('gesture_model2.keras')

# Define label map manually (based on training folders)
labels = ['0', '1', '2', '3', '4', '5']

def classify_image(image_path):
    img = load_img(image_path, target_size=IMG_SIZE)
    img_array = img_to_array(img) / 255.0
    img_array = np.expand_dims(img_array, axis=0)

    prediction = model.predict(img_array)
    predicted_class = np.argmax(prediction)
    confidence = np.max(prediction)

    print(f"Predicted Gesture: {labels[predicted_class]} (Confidence: {confidence:.2f})")

# Run it
classify_image(IMAGE_PATH)
classify_image(IMAGE_PATH1)
classify_image(IMAGE_PATH2)
classify_image(IMAGE_PATH3)
classify_image(IMAGE_PATH4)
classify_image(IMAGE_PATH5)
