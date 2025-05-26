# import os
# import numpy as np
# import tensorflow as tf
# from tensorflow.keras.models import load_model
# from tensorflow.keras.preprocessing.image import load_img, img_to_array
# from tensorflow.image import flip_left_right, rot90

# # Constants
# IMG_SIZE = (224, 224)
# IMAGE_PATHS = [
#     'actual_images/zero.jpg',
#     'actual_images/one.jpg',
#     'actual_images/two.jpg',
#     'actual_images/three.jpg',
#     'actual_images/four.jpg',
#     'actual_images/five.jpg'
# ]

# # Load model
# model = tf.keras.models.load_model('gesture_model2.keras')

# # Labels (should match training class folder names)
# labels = ['0', '1', '2', '3', '4', '5']

# # TTA function
# def generate_tta_images(image):
#     tta_images = [image]
#     tta_images.append(flip_left_right(image))
#     tta_images.append(rot90(image, k=1))
#     tta_images.append(rot90(image, k=2))
#     tta_images.append(rot90(image, k=3))
#     return tta_images

# def classify_image_with_tta(image_path):
#     img = load_img(image_path, target_size=IMG_SIZE)
#     img_array = img_to_array(img) / 255.0
#     img_tensor = tf.convert_to_tensor(img_array, dtype=tf.float32)

#     tta_versions = generate_tta_images(img_tensor)
#     tta_batch = tf.stack(tta_versions)

#     predictions = model.predict(tta_batch)
#     mean_prediction = tf.reduce_mean(predictions, axis=0).numpy()

#     # Print full class confidence scores
#     print(f"\n{os.path.basename(image_path)} - Class probabilities:")
#     for i, label in enumerate(labels):
#         print(f"  {label}: {mean_prediction[i]:.2f}")

#     top_idx = np.argmax(mean_prediction)
#     print(f"Predicted Gesture: {labels[top_idx]} (Confidence: {mean_prediction[top_idx]:.2f})")

# # Run on all input images
# for path in IMAGE_PATHS:
#     classify_image_with_tta(path)


import os
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"

import io
from contextlib import redirect_stderr
stderr_silencer = io.StringIO()
with redirect_stderr(stderr_silencer):
    import numpy as np
    import tensorflow as tf
    from tensorflow.keras.models import load_model
    from tensorflow.keras.preprocessing.image import load_img, img_to_array
    from tensorflow.image import flip_left_right, rot90
import sys


# Suppress TF GPU & logging messages
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

# Silence absl/TensorFlow backend stderr noise
f = io.StringIO()
with redirect_stderr(f):
    model = tf.keras.models.load_model('gesture_model2.keras')

# Constants
IMG_SIZE = (224, 224)
IMAGE_PATHS = [
    'actual_images/zero.jpg',
    'actual_images/one.jpg',
    'actual_images/two.jpg',
    'actual_images/three.jpg',
    'actual_images/four.jpg',
    'actual_images/five.jpg'
]

labels = ['0', '1', '2', '3', '4', '5']

def generate_tta_images(image):
    tta_images = [image]
    tta_images.append(flip_left_right(image))
    tta_images.append(rot90(image, k=1))
    tta_images.append(rot90(image, k=2))
    tta_images.append(rot90(image, k=3))
    return tta_images

def classify_image_with_tta(image_path):
    img = load_img(image_path, target_size=IMG_SIZE)
    img_array = img_to_array(img) / 255.0
    img_tensor = tf.convert_to_tensor(img_array, dtype=tf.float32)

    tta_versions = generate_tta_images(img_tensor)
    tta_batch = tf.stack(tta_versions)

    predictions = model.predict(tta_batch, verbose=0)
    mean_prediction = tf.reduce_mean(predictions, axis=0).numpy()

    predicted_class = np.argmax(mean_prediction)
    confidence = np.max(mean_prediction)

    print(f"{os.path.basename(image_path)} ➜ Predicted Gesture: {labels[predicted_class]} (Confidence: {confidence:.2f})")

for path in IMAGE_PATHS:
    classify_image_with_tta(path)



# import os
# import numpy as np
# import tensorflow as tf
# from tensorflow.keras.models import load_model
# from tensorflow.keras.preprocessing.image import load_img, img_to_array

# # Constants
# IMG_SIZE = (224, 224)
# IMAGE_PATH = 'actual_images/zero.jpg'  
# IMAGE_PATH1 = 'actual_images/one.jpg'
# IMAGE_PATH2 = 'actual_images/two.jpg'
# IMAGE_PATH3 = 'actual_images/three.jpg'
# IMAGE_PATH4 = 'actual_images/four.jpg'
# IMAGE_PATH5 = 'actual_images/five.jpg'


# # Load model
# model = tf.keras.models.load_model('gesture_model2.keras')

# # Define label map manually (based on training folders)
# labels = ['0', '1', '2', '3', '4', '5']

# def classify_image(image_path):
#     img = load_img(image_path, target_size=IMG_SIZE)
#     img_array = img_to_array(img) / 255.0
#     img_array = np.expand_dims(img_array, axis=0)

#     prediction = model.predict(img_array)
#     predicted_class = np.argmax(prediction)
#     confidence = np.max(prediction)

#     print(f"Predicted Gesture: {labels[predicted_class]} (Confidence: {confidence:.2f})")

# # Run it
# classify_image(IMAGE_PATH)
# classify_image(IMAGE_PATH1)
# classify_image(IMAGE_PATH2)
# classify_image(IMAGE_PATH3)
# classify_image(IMAGE_PATH4)
# classify_image(IMAGE_PATH5)
