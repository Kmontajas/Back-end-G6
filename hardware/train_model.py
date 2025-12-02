# convert_to_int8_64.py
import os
import numpy as np

# === CONFIG ===
INPUT_WIDTH = 32
INPUT_HEIGHT = 32
CHANNELS = 3

KERAS_MODEL_PATH = "trash_classifier_model.h5"
ORIG_TFLITE_PATH = "trash_classifier_model.tflite"
OUTPUT_TFLITE = "trash_classifier_model_int8_32.tflite"

REP_IMAGES_DIR = "dataset"  # put representative images here

try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras.layers import Resizing # type: ignore
    print("TensorFlow version:", tf.__version__)
except Exception as e:
    print("TensorFlow is not installed.")
    raise SystemExit(1)

def load_keras_model(path):
    print("Loading Keras model:", path)
    return keras.models.load_model(path, compile=False)

def make_wrapper_model(original_model, target_h=INPUT_HEIGHT, target_w=INPUT_WIDTH):
    orig_input_shape = original_model.input_shape[1:3]
    print("Original model input shape:", original_model.input_shape)

    # Wrapper input: 64x64
    inp = keras.Input(shape=(target_h, target_w, 3), name="wrapped_input")

    # Use Keras Resizing layer instead of tf.image.resize
    if orig_input_shape[0] is None or orig_input_shape[1] is None:
        x = inp  # skip if variable input size
    else:
        resize_layer = Resizing(
            orig_input_shape[0],
            orig_input_shape[1],
            interpolation="bilinear"
        )
        x = resize_layer(inp)

    out = original_model(x)
    return keras.Model(inputs=inp, outputs=out, name="wrapped_model")

def representative_dataset_gen_from_dir(n_samples=100):
    """
    Representative dataset using float32 normalized images (0..1),
    required because model input dtype is float32.
    """
    from PIL import Image

    image_paths = []
    for root, dirs, files in os.walk(REP_IMAGES_DIR):
        for f in files:
            if f.lower().endswith((".jpg", ".jpeg", ".png")):
                image_paths.append(os.path.join(root, f))

    if not image_paths:
        print("No images found. Using random fallback.")
        for _ in range(n_samples):
            arr = np.random.rand(1, INPUT_HEIGHT, INPUT_WIDTH, CHANNELS).astype(np.float32)
            yield [arr]
        return

    print(f"Found {len(image_paths)} representative images in database")

    count = 0
    for path in image_paths:
        try:
            img = Image.open(path).convert("RGB").resize((INPUT_WIDTH, INPUT_HEIGHT))
            arr = np.asarray(img).astype(np.float32) / 255.0   # ⚠ normalize to 0–1 float32
            arr = np.expand_dims(arr, 0)
            yield [arr]

            count += 1
            if count >= n_samples:
                break

        except Exception as e:
            print("Skipping file:", path, "Error:", e)
            continue



def convert_from_keras(keras_path, out_tflite):
    model = load_keras_model(keras_path)
    wrapper = make_wrapper_model(model, INPUT_HEIGHT, INPUT_WIDTH)

    converter = tf.lite.TFLiteConverter.from_keras_model(wrapper)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    # IMPORTANT: pass the function object (no parentheses)
    converter.representative_dataset = representative_dataset_gen_from_dir

    # Force INTEGER only ops
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]

    # If you choose uint8 here, your representative dataset should yield uint8 arrays (0..255).
    # Alternatively set inference_input_type = tf.float32 if rep data are float32 normalized 0..1
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8

    print("Converting to INT8 TFLite...")
    tflite_model = converter.convert()

    with open(out_tflite, "wb") as f:
        f.write(tflite_model)

    print("Saved INT8 model to:", out_tflite)

if __name__ == "__main__":
    if os.path.exists(KERAS_MODEL_PATH):
        convert_from_keras(KERAS_MODEL_PATH, OUTPUT_TFLITE)
    else:
        print("Keras model not found.")
