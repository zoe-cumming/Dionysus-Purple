import os
import numpy as np
from sklearn.utils.class_weight import compute_class_weight
from sklearn.metrics import classification_report, confusion_matrix
import tensorflow as tf
from tensorflow.keras.preprocessing.image import ImageDataGenerator
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.layers import GlobalAveragePooling2D, Dense, Dropout, BatchNormalization
from tensorflow.keras.models import Model
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.callbacks import EarlyStopping, ModelCheckpoint

# Paths
BASE_DIR = os.getcwd()
TRAIN_DIR = os.path.join(BASE_DIR, 'training_images1')
TEST_DIR = os.path.join(BASE_DIR, 'testing_images1')

# Config
IMG_SIZE = (224, 224)
BATCH_SIZE = 32
EPOCHS = 10
SEED = 42

# Data generators
train_datagen = ImageDataGenerator(
    rescale=1./255,
    rotation_range=20,
    zoom_range=0.2,
    width_shift_range=0.1,
    height_shift_range=0.1,
    shear_range=0.1,
    brightness_range=(0.8, 1.2),
    horizontal_flip=True
)

test_datagen = ImageDataGenerator(rescale=1./255)

train_generator = train_datagen.flow_from_directory(
    TRAIN_DIR,
    target_size=IMG_SIZE,
    batch_size=BATCH_SIZE,
    class_mode='categorical',
    seed=SEED
)

test_generator = test_datagen.flow_from_directory(
    TEST_DIR,
    target_size=IMG_SIZE,
    batch_size=BATCH_SIZE,
    class_mode='categorical',
    shuffle=False
)

# Class weights to handle class imbalance
labels = train_generator.classes
class_weights = compute_class_weight(
    class_weight='balanced',
    classes=np.unique(labels),
    y=labels
)
class_weights = dict(enumerate(class_weights))

# Build model
base_model = MobileNetV2(include_top=False, weights='imagenet', input_shape=(224, 224, 3))
base_model.trainable = False

x = base_model.output
x = GlobalAveragePooling2D()(x)
x = Dense(256, activation='relu')(x)
x = BatchNormalization()(x)
x = Dropout(0.5)(x)
x = Dense(128, activation='relu')(x)
x = Dropout(0.3)(x)
predictions = Dense(train_generator.num_classes, activation='softmax')(x)

model = Model(inputs=base_model.input, outputs=predictions)
model.compile(optimizer=Adam(learning_rate=1e-4),
              loss='categorical_crossentropy',
              metrics=['accuracy'])

# Callbacks
early_stop = EarlyStopping(patience=3, restore_best_weights=True)
checkpoint = ModelCheckpoint("best_model1.keras", save_best_only=True)

# Train top layers
model.fit(
    train_generator,
    epochs=EPOCHS,
    validation_data=test_generator,
    class_weight=class_weights,
    callbacks=[early_stop, checkpoint]
)

# Fine-tuning some base layers
base_model.trainable = True
for layer in base_model.layers[:-40]:
    layer.trainable = False

model.compile(optimizer=Adam(learning_rate=1e-5),
              loss='categorical_crossentropy',
              metrics=['accuracy'])

model.fit(
    train_generator,
    epochs=5,
    validation_data=test_generator,
    class_weight=class_weights,
    callbacks=[early_stop, checkpoint]
)

# Evaluate
loss, acc = model.evaluate(test_generator)
print(f"Test Accuracy: {acc * 100:.2f}%")

# Print classification report
preds = model.predict(test_generator)
pred_labels = np.argmax(preds, axis=1)
true_labels = test_generator.classes
class_names = list(test_generator.class_indices.keys())

print("Confusion Matrix:")
print(confusion_matrix(true_labels, pred_labels))

print("Classification Report:")
print(classification_report(true_labels, pred_labels, target_names=class_names))

# Save final model
model.save("gesture_model1.keras")


# import os
# import numpy as np
# import tensorflow as tf
# from tensorflow.keras.preprocessing.image import ImageDataGenerator
# from tensorflow.keras.applications import MobileNetV2
# from tensorflow.keras.layers import GlobalAveragePooling2D, Dense
# from tensorflow.keras.models import Model
# from tensorflow.keras.optimizers import Adam

# # Paths
# BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# TRAIN_DIR = os.path.join(BASE_DIR, 'training_images')
# TEST_DIR = os.path.join(BASE_DIR, 'testing_images')

# # Config
# IMG_SIZE = (224, 224)
# BATCH_SIZE = 32
# EPOCHS = 10

# # Data generators
# train_datagen = ImageDataGenerator(
#     rescale=1./255,
#     rotation_range=15,
#     zoom_range=0.1,
#     width_shift_range=0.1,
#     height_shift_range=0.1,
#     horizontal_flip=True
# )

# test_datagen = ImageDataGenerator(rescale=1./255)

# train_generator = train_datagen.flow_from_directory(
#     TRAIN_DIR,
#     target_size=IMG_SIZE,
#     batch_size=BATCH_SIZE,
#     class_mode='categorical'
# )

# test_generator = test_datagen.flow_from_directory(
#     TEST_DIR,
#     target_size=IMG_SIZE,
#     batch_size=BATCH_SIZE,
#     class_mode='categorical',
#     shuffle=False
# )

# # Build model
# base_model = MobileNetV2(include_top=False, weights='imagenet', input_shape=(224, 224, 3))
# base_model.trainable = False  # Freeze base layers

# x = base_model.output
# x = GlobalAveragePooling2D()(x)
# x = Dense(128, activation='relu')(x)
# predictions = Dense(7, activation='softmax')(x)

# model = Model(inputs=base_model.input, outputs=predictions)

# # Compile model
# model.compile(optimizer=Adam(learning_rate=0.0001),
#               loss='categorical_crossentropy',
#               metrics=['accuracy'])

# # Train model
# model.fit(
#     train_generator,
#     epochs=EPOCHS,
#     validation_data=test_generator
# )

# # Evaluate model
# loss, acc = model.evaluate(test_generator)
# print(f"Test Accuracy: {acc*100:.2f}%")

# # Save model
# model.save(os.path.join(BASE_DIR, 'gesture_model.keras'))

