import tensorflow as tf

# Load your TFLite model
interpreter = tf.lite.Interpreter(model_path="trash_classifier_model.tflite")

# Allocate tensors (required before getting details)
interpreter.allocate_tensors()

# Get model input details
input_details = interpreter.get_input_details()
print("Input Details:", input_details)

# Get model output details
output_details = interpreter.get_output_details()
print("Output Details:", output_details)
