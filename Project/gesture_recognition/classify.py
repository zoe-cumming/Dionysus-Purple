import os
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.utils.class_weight import compute_class_weight
from sklearn.metrics import classification_report, confusion_matrix
from tensorflow.keras.preprocessing.image import ImageDataGenerator, load_img, img_to_array
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.layers import GlobalAveragePooling2D, Dense, Dropout, BatchNormalization
from tensorflow.keras.models import Model
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.callbacks import EarlyStopping, ModelCheckpoint, ReduceLROnPlateau

# Paths
BASE_DIR = os.getcwd()
TRAIN_DIR = os.path.join(BASE_DIR, 'training_images')
TEST_DIR = os.path.join(BASE_DIR, 'testing_images')

# Config
IMG_SIZE = (224, 224)
BATCH_SIZE = 32
EPOCHS_TOP = 15
EPOCHS_FINE = 10
SEED = 42

# Augmentation
def custom_augment(image):
    image = tf.image.resize_with_crop_or_pad(image, 240, 240)
    image = tf.image.random_crop(image, size=[224, 224, 3])
    image = tf.image.random_flip_left_right(image)
    image = tf.image.random_brightness(image, max_delta=0.2)
    image = tf.image.random_contrast(image, 0.8, 1.2)
    image = tf.image.random_saturation(image, 0.8, 1.2)
    image = tf.image.random_hue(image, max_delta=0.08)
    return image

# Weighting real images
def build_dataframe_with_weights(data_dir):
    image_paths, labels, weights = [], [], []
    class_names = sorted(os.listdir(data_dir))
    for class_name in class_names:
        class_dir = os.path.join(data_dir, class_name)
        if not os.path.isdir(class_dir):
            continue
        for fname in os.listdir(class_dir):
            if not fname.lower().endswith(('.jpg', '.jpeg', '.png')):
                continue
            fpath = os.path.join(class_dir, fname)
            image_paths.append(fpath)
            labels.append(class_name)
            is_real = fname.startswith("PXL") or fname.startswith("frame")
            weights.append(2.0 if is_real else 1.0)
    return pd.DataFrame({'filename': image_paths, 'class': labels, 'weight': weights})

train_df = build_dataframe_with_weights(TRAIN_DIR)
test_df = build_dataframe_with_weights(TEST_DIR)

# Data Generators
train_datagen = ImageDataGenerator(rescale=1./255, preprocessing_function=custom_augment)
test_datagen = ImageDataGenerator(rescale=1./255)

train_generator = train_datagen.flow_from_dataframe(
    dataframe=train_df,
    x_col='filename',
    y_col='class',
    weight_col='weight',
    target_size=IMG_SIZE,
    batch_size=BATCH_SIZE,
    class_mode='categorical',
    seed=SEED
)

test_generator = test_datagen.flow_from_dataframe(
    dataframe=test_df,
    x_col='filename',
    y_col='class',
    target_size=IMG_SIZE,
    batch_size=BATCH_SIZE,
    class_mode='categorical',
    shuffle=False
)

num_classes = len(train_generator.class_indices)

# Model
base_model = MobileNetV2(include_top=False, weights='imagenet', input_shape=(224, 224, 3))
base_model.trainable = False

x = GlobalAveragePooling2D()(base_model.output)
x = Dense(256, activation='relu')(x)
x = BatchNormalization()(x)
x = Dropout(0.5)(x)
x = Dense(128, activation='relu')(x)
x = Dropout(0.3)(x)
output = Dense(num_classes, activation='softmax')(x)

model = Model(inputs=base_model.input, outputs=output)
model.compile(optimizer=Adam(1e-4), loss='categorical_crossentropy', metrics=['accuracy'])

# Callbacks
early_stop = EarlyStopping(patience=3, restore_best_weights=True)
checkpoint = ModelCheckpoint("best_model2.keras", save_best_only=True)
reduce_lr = ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=2, min_lr=1e-6)

# Train top layers
model.fit(
    train_generator,
    epochs=EPOCHS_TOP,
    validation_data=test_generator,
    callbacks=[early_stop, checkpoint, reduce_lr]
)

# Fine-tune more layers
base_model.trainable = True
for layer in base_model.layers[:60]:
    layer.trainable = False

model.compile(optimizer=Adam(1e-5), loss='categorical_crossentropy', metrics=['accuracy'])

model.fit(
    train_generator,
    epochs=EPOCHS_FINE,
    validation_data=test_generator,
    callbacks=[early_stop, checkpoint, reduce_lr]
)

# Evaluate
loss, acc = model.evaluate(test_generator)
print(f"\nFinal Test Accuracy: {acc * 100:.2f}%")

# Classification metrics
preds = model.predict(test_generator)
pred_labels = np.argmax(preds, axis=1)
true_labels = test_generator.classes
class_names = list(test_generator.class_indices.keys())

print("\nConfusion Matrix:")
print(confusion_matrix(true_labels, pred_labels))
print("\nClassification Report:")
print(classification_report(true_labels, pred_labels, target_names=class_names))

# Save final model
model.save("gesture_model2.keras")
