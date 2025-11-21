import tensorflow as tf
import numpy as np
import os

# Path to dataset
DATA_DIR = "dataset"

# Load dataset
train_ds = tf.keras.preprocessing.image_dataset_from_directory(
    DATA_DIR,
    validation_split=0.2,
    subset="training",
    seed=123,
    image_size=(64, 64),  # small for ESP32
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

class_names = train_ds.class_names
num_classes = len(class_names)
print("Classes:", class_names)

# Prefetch for speed
train_ds = train_ds.cache().shuffle(100).prefetch(buffer_size=tf.data.AUTOTUNE)
val_ds = val_ds.cache().prefetch(buffer_size=tf.data.AUTOTUNE)

# -------------------------------------------
# 🔥 SUPER LIGHTWEIGHT MODEL FOR ESP32-CAM
# -------------------------------------------
model = tf.keras.Sequential([
    tf.keras.layers.Rescaling(1./255, input_shape=(64, 64, 3)),
    tf.keras.layers.Conv2D(4, 3, activation='relu'),
    tf.keras.layers.MaxPooling2D(),

    tf.keras.layers.Conv2D(8, 3, activation='relu'),
    tf.keras.layers.MaxPooling2D(),

    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(num_classes, activation='softmax')
])

model.compile(
    optimizer="adam",
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"]
)

model.fit(train_ds, validation_data=val_ds, epochs=10)

# Save Keras version
model.save("trash_classifier_model.h5")
print("Saved trash_classifier_model.h5")

# -------------------------------------------
# 🔥 FULL INT8 QUANTIZATION (90% size reduction)
# -------------------------------------------
def representative_dataset():
    for images, _ in train_ds.take(50):
        yield [tf.cast(images, tf.float32)]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

# Save TFLite
with open("trash_classifier_model.tflite", "wb") as f:
    f.write(tflite_model)

print("🔥 Optimized TFLite model saved! (trash_classifier_model.tflite)")
