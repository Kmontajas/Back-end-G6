import tensorflow as tf
import numpy as np
import os

# Path to your dataset folder
DATA_DIR = "dataset"

# Load the dataset (train and validation)
train_ds = tf.keras.preprocessing.image_dataset_from_directory(
    DATA_DIR,
    validation_split=0.2,
    subset="training",
    seed=123,
    image_size=(64, 64),  # small image for ESP32 compatibility
    batch_size=4
)
val_ds = tf.keras.preprocessing.image_dataset_from_directory(
    DATA_DIR,
    validation_split=0.2,
    subset="validation",
    seed=123,
    image_size=(64, 64),
    batch_size=4
)

# Get class names before prefetching
class_names = train_ds.class_names
num_classes = len(class_names)
print("Class names:", class_names)

# Optimize dataset performance
train_ds = train_ds.cache().shuffle(100).prefetch(buffer_size=tf.data.AUTOTUNE)
val_ds = val_ds.cache().prefetch(buffer_size=tf.data.AUTOTUNE)

# Build a lightweight CNN model (ESP32-friendly)
model = tf.keras.Sequential([
    tf.keras.layers.Rescaling(1./255, input_shape=(64, 64, 3)),
    tf.keras.layers.Conv2D(8, 3, activation='relu'),
    tf.keras.layers.MaxPooling2D(),
    tf.keras.layers.Conv2D(16, 3, activation='relu'),
    tf.keras.layers.MaxPooling2D(),
    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(32, activation='relu'),
    tf.keras.layers.Dense(num_classes, activation='softmax')
])

# Compile the model
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

# Train
model.fit(train_ds, validation_data=val_ds, epochs=10)

# Save model
model.save("trash_classifier_model.h5")
print("Model saved as trash_classifier_model.h5")

# Convert to TensorFlow Lite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

# Save TFLite model
with open("trash_classifier_model.tflite", "wb") as f:
    f.write(tflite_model)

print("✅ TFLite model saved as trash_classifier_model.tflite")
