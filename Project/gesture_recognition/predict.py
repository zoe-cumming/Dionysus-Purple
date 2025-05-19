import os
import numpy as np
from tensorflow.keras.models import load_model
from tensorflow.keras.preprocessing.image import load_img, img_to_array

# Constants
IMG_SIZE = (224, 224)
IMAGE_PATH = 'testing_images/4/901.jpg'  # Replace with other image filenames

# Load model
model = load_model('gesture_model.h5')

# Define your label map manually (based on training folders)
labels = ['0', '1', '2', '3', '4', '5', 'phone']

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
