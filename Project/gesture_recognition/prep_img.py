import numpy as np
from tensorflow.keras.preprocessing.image import load_img, img_to_array

#Loads and formats a JPEG image for model classification.
def prepare_image_for_classification(image_path, target_size=(224, 224)):
    img = load_img(image_path, target_size=target_size)  # PIL image
    img_array = img_to_array(img)                        # Convert to NumPy array
    img_array = img_array / 255.0                        # Normalize to [0, 1]
    img_array = np.expand_dims(img_array, axis=0)        # Shape: (1, height, width, 3)
    return img_array
