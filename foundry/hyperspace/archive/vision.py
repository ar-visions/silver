import  tensorflow as tf
from    tensorflow import keras
from    tensorflow.keras import KerasTensor
from    tensorflow.keras.layers import InputLayer
from    tensorflow.keras.regularizers import l2
import  matplotlib.pyplot as plt
import  os
import  math
import  numpy as np
import  time
from    collections import OrderedDict
import  cv2
import  json
import  random
import  requests
import  zipfile
from    typing import Tuple, List, Any, Union

# ------------------------------------------------------------------
# color formatting
# ------------------------------------------------------------------
WHITE  = '\033[97m'
GRAY   = '\033[90m'
BLUE   = '\033[94m'
YELLOW = '\033[93m'
GREEN  = '\033[92m'
CYAN   = '\033[96m'
PURPLE = '\033[95m'
RESET  = '\033[0m'

SEED = 242  # You can change this to any fixed number

# Seed Python's built-in random module
random.seed(SEED)

# Seed NumPy
np.random.seed(SEED)

# Seed TensorFlow
tf.random.set_seed(SEED)

# Ensure deterministic behavior for cuDNN (may slow down training slightly)
os.environ['TF_DETERMINISTIC_OPS'] = '1'
os.environ['CUDA_VISIBLE_DEVICES'] = '0'  # Ensures single-GPU determinism (optional)

#scales = {}
base_model   = None
target_model = None
eyes_model   = None
track_model  = None

if os.path.exists('res/models/vision_base.keras'):
    base_model = keras.models.load_model('res/models/vision_base.keras')
    base_model.summary()

if os.path.exists('res/models/vision_target.keras'):
    target_model = keras.models.load_model('res/models/vision_target.keras')
    target_model.summary()

if os.path.exists('res/models/vision_eyes.keras'):
    eyes_model = keras.models.load_model('res/models/vision_eyes.keras')
    eyes_model.summary()

if os.path.exists('res/models/vision_track.keras'):
    track_model = keras.models.load_model('res/models/vision_track.keras')
    track_model.summary()

models           = {}
models['base']   = base_model
models['target'] = target_model
models['eyes']   = eyes_model
models['track']  = track_model

full_size        = 340
target_scale     = 0.8

# Download the WIDER_train dataset if not already downloaded
def download_file(url, local_filename):
    with requests.get(url, stream=True) as r:
        r.raise_for_status()
        with open(local_filename, 'wb') as f:
            for chunk in r.iter_content(chunk_size=8192):
                f.write(chunk)

def show(image, array_v2=[], name=''):
    h, w = image.shape[:2]
    def normalize_to_pixel(coord):
          # Image height and width
        pixel_x = int((coord[0] + 0.5) * w)
        pixel_y = int((coord[1] + 0.5) * h)
        return pixel_x, pixel_y

    plt.figure(figsize=(8, 8))
    plt.imshow(image, cmap='gray')  # Ensure grayscale display
    colors = ['red', 'blue', 'green', 'yellow', 'orange', 'purple', 'pink']
    index  = 0
    for v2 in array_v2:
        x, y = normalize_to_pixel(v2)
        if len(v2) == 3:
            radius = v2[2] / 2 * w
            circle = plt.Circle((x, y), radius, color='cyan', fill=False, linewidth=2)
            plt.gca().add_patch(circle)
        else:
            plt.scatter(x, y, color=colors[index], s=100, label=name)
            plt.axhline(y=y, color='r', linestyle='--', alpha=0.5)
            plt.axvline(x=x, color='r', linestyle='--', alpha=0.5)
        index += 1

    plt.title("F2 Image with Eye Positions")
    plt.legend()
    plt.show(block=True)


def target_vary(image, center_target, targets, offset, scale_min, scale_max, can_mirror, vary_degs):
    # offset = 0
    t0 = center_target # targets[0]
    iris_mid_x = int((t0[0] + 0.5) * full_size)
    iris_mid_y = int((t0[1] + 0.5) * full_size)
    
    # randomly determine rotation degrees
    degs = random.uniform(-vary_degs, vary_degs)

    # rotate image around iris midpoint
    rotation_matrix = cv2.getRotationMatrix2D((iris_mid_x, iris_mid_y), degs, 1.0)
    image = cv2.warpAffine(image, rotation_matrix, (full_size, full_size), flags=cv2.INTER_LINEAR)

    # crop settings
    sc = random.uniform(scale_min, scale_max)
    new_h = new_w = int(full_size * sc)

    # center the crop properly
    start_x = int(iris_mid_x - new_w / 2 + round(new_w * random.uniform(-offset, offset)))
    start_y = int(iris_mid_y - new_h / 2 + round(new_h * random.uniform(-offset, offset)))

    # compute new iris-mid in cropped coordinates
    trans = []
    for target in targets:
        x = int((target[0] + 0.5) * full_size)
        y = int((target[1] + 0.5) * full_size)

        # apply the same rotation to target points
        rotated = np.dot(rotation_matrix[:, :2], np.array([x, y])) + rotation_matrix[:, 2]
        rotated_x, rotated_y = rotated

        # convert rotated target to normalized crop-space coordinates
        translated_x = (rotated_x - start_x) / new_w - 0.5
        translated_y = (rotated_y - start_y) / new_h - 0.5
        trans.append([translated_x, translated_y])

    # compute proper crop center
    crop_center = [(start_x + new_w / 2) / full_size - 0.5, (start_y + new_h / 2) / full_size - 0.5]
    image = center_crop(image, new_w, crop_center)

    # apply mirroring
    mirror = False
    if can_mirror and random.random() < 0.5:
        mirror = True
        image = cv2.flip(image, 1)
        for t in trans:
            t[0] = -t[0]  # Flip x-coordinate

    return image, trans, sc, mirror, degs

def convert(img):
    if img.ndim == 2:
        res = img / 255.0
        res = np.expand_dims(res, axis=-1)  # Add batch & channel dim
        return res
    return img.copy()

def channels(images):
    channels = [np.asarray(convert(img)) for img in images]
    return np.concatenate(channels, axis=-1)  # Stack along the channel axis (axis=-1)

def run_base(image, iris_mid, scale):
    if iris_mid and scale:
        return iris_mid, scale[0]
    input_shape = base_model.input_shape  # (None, height, width, channels)
    input_height, input_width = input_shape[1:3]
    final_input = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_AREA)
    final_input = final_input / 255.0
    final_input = np.expand_dims(final_input, axis=[0,-1])  # Add batch & channel dim
    res = base_model(final_input, training=False)[0].numpy()
    return (np.array([res[0],res[1]]), res[2])

def run_target(image, iris_mid, scale):
    image, _, scale, mirror, degs = target_vary(image, iris_mid, [iris_mid], 0.0, scale, scale, False, 0) 
    input_shape = target_model.input_shape  # (None, height, width, channels)
    input_height, input_width = input_shape[1:3]
    final_input = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_AREA)
    final_input = final_input / 255.0
    final_input = np.expand_dims(final_input, axis=[0,-1])  # Add batch & channel dim
    res = target_model(final_input, training=False)[0].numpy()
    target_scale = res[2]
    return ([iris_mid[0] + res[0] * (target_scale * scale),
             iris_mid[1] + res[1] * (target_scale * scale)], target_scale * scale, image, [res[0], res[1]])

def run_eyes(image, iris_mid, scale):
    image, _, new_scale, mirror, degs = target_vary(image, iris_mid, [iris_mid], 0.0, scale * 0.8, scale * 0.8, False, 0) 

    input_shape = base_model.input_shape  # (None, height, width, channels)
    input_height, input_width = input_shape[1:3]
    final_input = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_AREA)
    final_input = final_input / 255.0
    final_input = np.expand_dims(final_input, axis=[0,-1])  # Add batch & channel dim
    res         = eyes_model(final_input, training=False)[0].numpy()
    
    # 0..1 = iris-mid from center (we offset iris_mid to correct this)
    rescale      = new_scale * res[4]
    result_scale = scale * res[4]
    eye_right    = [res[0], res[1]]
    eye_left     = [res[2], res[3]]
    result_eye_right = [
        iris_mid[0] + eye_right[0] * rescale,
        iris_mid[1] + eye_right[1] * rescale]
    result_eye_left = [
        iris_mid[0] + eye_left [0] * rescale,
        iris_mid[1] + eye_left [1] * rescale]
    
    show(image, [eye_right, eye_left], 'eyes')

    return (result_eye_right,
            result_eye_left,
            result_scale, image, eye_right)

def run_track(f2_image, f2_iris_mid, f2_scale, f2_eye_left, f2_eye_right, f6_image, f6_iris_mid, f6_scale, f6_eye_left, f6_eye_right):
    # we need separate images made for face, left and right eyes for both f2 and f6 cameras

    f2_face_image, _, _, _ = target_vary(f2_image, f2_iris_mid, [f2_iris_mid], 0.0, f2_scale, f2_scale, False, 0) 
    f6_face_image, _, _, _ = target_vary(f6_image, f6_iris_mid, [f6_iris_mid], 0.0, f6_scale, f6_scale, False, 0) 

    f2_left_image,  _, _, _ = target_vary(f2_image, f2_eye_left, [f2_eye_left],  0.0, f2_scale / 4.0, f2_scale / 4.0, False, 0) 
    f2_right_image, _, _, _ = target_vary(f2_image, f2_eye_right, [f2_eye_right], 0.0, f2_scale / 4.0, f2_scale / 4.0, False, 0) 
    
    f6_left_image,  _, _, _ = target_vary(f6_image, f6_eye_left, [f6_eye_left],  0.0, f6_scale / 4.0, f6_scale / 4.0, False, 0) 
    f6_right_image, _, _, _ = target_vary(f6_image, f6_eye_right, [f6_eye_right], 0.0, f6_scale / 4.0, f6_scale / 4.0, False, 0) 

    _channels = channels([f2_face_image, f6_face_image, f2_left_image, f6_left_image, f2_right_image, f6_right_image])
    input_shape = base_model.input_shape  # (None, height, width, channels)
    input_height, input_width = input_shape[1:3]
    f2_image_input = cv2.resize(f2_image, (input_width, input_height), interpolation=cv2.INTER_AREA)
    f2_image_input = f2_image_input / 255.0
    f2_image_input = np.expand_dims(f2_image_input, axis=[0,-1])  # Add batch & channel dim
    
    f6_image_input = cv2.resize(f6_image, (input_width, input_height), interpolation=cv2.INTER_AREA)
    f6_image_input = f2_image_input / 255.0
    f6_image_input = np.expand_dims(f6_image_input, axis=[0,-1])  # Add batch & channel dim
    
    # todo: make sure we are giving camera space units into this
    res = track_model([_channels, f2_scale, f6_scale, f2_iris_mid, f6_iris_mid, f2_eye_left, f6_eye_left, f2_eye_right, f6_eye_right], training=False)[0].numpy()
    return res

# keras-interfacing model, with batch using our data
class model(keras.utils.Sequence):
    def __init__(self, mode, data, batch_size):
        self.mode         = mode
        self.data         = data
        self.batch_size   = batch_size
        self.inputs       = []
        self.cache        = None

    def loss(self, y_true, y_pred):
        return keras.losses.MeanSquaredError()(y_true, y_pred)

    # process batch
    def __getitem__(self, batch_index):
        start         = batch_index * self.batch_size
        end           = min((batch_index + 1) * self.batch_size, len(self.data))
        if not hasattr(self, 'cache'):
            self.cache = {}

        batch_data    = {}
        batch_label   = []
        for i in range(start, end):
            if i in self.cache:
                data, label = self.cache[i]['data'], self.cache[i]['label']
            else:
                data, label = self.label(self.data, i)
                self.cache[i] = {}
                self.cache[i]['data']  = data
                self.cache[i]['label'] = label

            for f in self.inputs:
                if f not in batch_data:
                    batch_data[f] = []
                batch_data[f].append(np.array(data[f], dtype=np.float32))
            batch_label.append(np.array(label, dtype=np.float32))
        for f in batch_data:  batch_data [f] = np.array(batch_data [f], dtype=np.float32)
        return batch_data, np.array(batch_label)
    
    # length of batched data
    def __len__(self):
        if len(self.data) == 0: return 0
        return max(1, (len(self.data) + self.batch_size - 1) // self.batch_size)
    
    # user implements
    def model(self):
        assert False, 'user-implement'

# export based on quantized state
def export_part(mode, layer, part, index):
    w = layer.get_weights()
    weights_id = f"{mode}_{layer.name}_{part}"
    weight_array = w[index]
    shape = weight_array.shape
    n_dims = len(shape)

    # Prepare binary shape information
    shape_header = (np.array([n_dims], dtype=np.int32).tobytes() + 
                    np.array(shape, dtype=np.int64).tobytes())

    # check for quantized weights (int8)
    if hasattr(layer, 'quantize_config'):
        # Extract scale and zero-point from quantized layers
        if hasattr(layer, 'get_quantize_params'):
            scale = layer.get_quantize_params().get("scale", 1.0)
            zero_point = layer.get_quantize_params().get("zero_point", 0)
        else:
            scale = 1.0
            zero_point = 0
        with open('res/models/%s.i8' % weights_id, 'wb') as f:
            f.write(shape_header)
            np.array(scale, dtype=np.float32).tofile(f)      # Store scale
            np.array(zero_point, dtype=np.float32).tofile(f) # Store zero-point
            weight_array.astype(np.int8).tofile(f)               # Store quantized weights
    else:
        # default float weight export
        with open('res/models/%s.f32' % weights_id, 'wb') as f:
            f.write(shape_header)
            weight_array.tofile(f)

    return weights_id

def export_json(model, mode, output_json_path, input_shapes):
    #return
    layers_dict = OrderedDict()
    output_shapes = {}
    
    # Function to get layer type
    def get_layer_type(layer):
        layer_class = layer.__class__.__name__
        if   layer_class == 'Conv2D':       return "conv"
        elif layer_class == 'DepthwiseConv2D': return "depthwise_conv"
        elif layer_class == 'ReLU':         return "relu"
        elif layer_class == 'MaxPooling2D': return "pool"
        elif layer_class == 'Dense':        return "dense"
        elif layer_class == 'Flatten':      return "flatten"
        elif layer_class == 'InputLayer':   return "input"
        elif layer_class == 'Concatenate':  return "concatenate"
        else:
            return layer_class
    
    # Run a sample inference to ensure all shapes are computed
    #if mode == 'target':
    #    sample_input  = np.zeros((1, input_size, input_size, 1))
    #    _ = model(sample_input)
    #else:
    #    sample_image  = np.zeros((1, input_size, input_size, 1))
    #    sample_iris_mid = np.zeros((1, 2))
    #    _ = model([sample_image, sample_iris_mid])
    
    def input_shape(layer):
        if hasattr(layer.input, 'shape'):
            if isinstance(layer.input, list):
                return [tensor.shape[1:] for tensor in layer.input]
            return layer.input.shape[1:]
        return None
    
    def get_output_shape(layer):
        if hasattr(layer.output, 'shape'):
            if isinstance(layer.output, list):
                return [list(tensor.shape[1:]) for tensor in layer.output]
            return list(layer.output.shape[1:])
        return None
    
    # Now the shapes should be populated
    for i, layer in enumerate(model.layers):
        layer_class = layer.__class__.__name__
        ishape      = input_shape(layer)
        oshape      = get_output_shape(layer)
        idim        = layer.input.shape[-1] if hasattr(layer.input, 'shape') and not isinstance(layer.input, list) else None
        
        if oshape is not None:
            output_shapes[layer.name] = oshape
        if hasattr(layer, '_inbound_nodes') and layer._inbound_nodes:
            for node in layer._inbound_nodes:
                if hasattr(node, 'inbound_layers'):
                    print(f"  Inbound layers: {[l.name for l in node.inbound_layers]}")
        
        layers_dict[layer.name] = { "name": layer.name }
        if   layer_class == 'InputLayer':
            layers_dict[layer.name].update({
                "Type":         "input",
                "tensor":       input_shapes
            })
        elif layer_class == 'Conv2D':
            layers_dict[layer.name].update({
                "Type":         "conv",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else [],
                "in_channels":  idim,
                "out_channels": layer.filters,
                "kernel_size":  list(layer.kernel_size),
                "strides":      list(layer.strides),
                "padding":      layer.padding,
                "activation":   layer.activation.__name__,
                "weights":      export_part(mode, layer, 'weights', 0),  # Kernel weights
                "bias":         export_part(mode, layer, 'bias',    1) # initial value in the accumulator in gemm
            })
        elif layer_class == 'DepthwiseConv2D':
            layers_dict[layer.name].update({
                "Type":         "depthwise_conv",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else [],
                "in_channels":   idim,
                "depth_multiplier": layer.depth_multiplier,  # How many filters per input channel
                "out_channels":  idim * layer.depth_multiplier,  # Depthwise convolutions keep input channels
                "kernel_size":   list(layer.kernel_size),
                "strides":       list(layer.strides),
                "padding":       layer.padding,
                "activation":    layer.activation.__name__,
                "weights":       export_part(mode, layer, 'depthwise_kernel', 0),  # Depthwise kernel weights
                "bias":          export_part(mode, layer, 'bias', 1)  # Initial value in the accumulator
            })
        elif layer_class == 'MaxPooling2D':
            layers_dict[layer.name].update({
                "Type":         "pool",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else [],
                "type":         "max",
                "pool_size":    list(layer.pool_size),
                "strides":      list(layer.strides)
            })
        elif layer_class == 'Dense':
            layers_dict[layer.name].update({
                "Type":         "dense",
                "inputs":        [],
                "tensor":        [1, ishape[0]],
                "input_dim":    idim,
                "output_dim":   layer.units,
                "activation":   layer.activation.__name__,
                "weights":      export_part(mode, layer, 'weights', 0),
                "bias":         export_part(mode, layer, 'bias', 1)
            })
        elif layer_class == 'Concatenate':
            layers_dict[layer.name].update({
                "Type":         "concatenate",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else [],
                "axis":         layer.axis
            })
        elif layer_class == 'ReLU':
            layers_dict[layer.name].update({
                "Type":         "relu",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else [],
                "threshold":    layer.threshold
            })
        elif layer_class == 'Flatten':
            layers_dict[layer.name].update({
                "Type":         "flatten",
                "inputs":        [],
                "tensor":        ishape if ishape is not None else []
            })
        else:
            print(f'ai: implement layer_class: {layer_class}')
        

        # Dictionary to track which layers output to which others
        output_connections = {layer.name: set() for layer in model.layers}

    # Process layers once to extract inputs
    for layer in model.layers:

        # Ensure we handle multiple inputs correctly and avoid Keras tensor names
        input_layers = []
        if isinstance(layer.input, list):
            for inp in layer.input:
                if hasattr(inp, '_keras_history'):  # This maps tensors back to layers
                    input_layers.append(inp._keras_history[0].name)
        else:
            if hasattr(layer.input, '_keras_history'):
                input_layers.append(layer.input._keras_history[0].name)

        # Store input connections correctly
        layers_dict[layer.name]["inputs"] = input_layers

        # Use model's internal config to extract connections
        for node in layer._inbound_nodes:
            for inbound_layer in getattr(node, 'inbound_layers', []):
                output_connections[inbound_layer.name].add(layer.name)

    # Identify terminal nodes (layers that do not output to any others)
    remaining_nodes = [layer for layer in layers_dict if not output_connections[layer]]

    # Ensure we only use the last computational layer
    valid_output_nodes = [node for node in remaining_nodes if "dense" in node or "conv" in node]
    assert len(valid_output_nodes), "Error: No valid terminal output nodes detected!"
    output_tensor = output_shapes[valid_output_nodes[-1]]  # ✅ Only take the last dense or conv layer
    assert len(remaining_nodes), "Error: No terminal output nodes detected!"

    if len(output_tensor) == 1:
        output_tensor = [1, output_tensor[0]]
    layers_dict["output"] = {
        "name":         "output",
        "Type":         "output",
        "inputs":       [valid_output_nodes[-1]],
        "tensor":       output_tensor
    }
    ops        = list(layers_dict.values())
    model_json = {
        "ident":            mode,
        "output":           output_tensor,
        "ops":              ops
    }

    # Save to JSON file
    with open(output_json_path, 'w') as f:
        json.dump(model_json, f, indent=4)


def train(train, val, final, lr, epochs):
    model = train.model()
    model.summary()

    # prevent boilerplate with this round-about input binding
    train.inputs = []
    for layer in model.layers:
        if isinstance(layer, InputLayer):
            train.inputs.append(layer.name)
    val.inputs   = train.inputs
    final.inputs = train.inputs
    # Set up optimizer
    optimizer = keras.optimizers.Adam(learning_rate=lr)
    bar_length = 40
    input_shape = None
    output_dim = model.output_shape[-1]

    # training and validation loops
    for epoch in range(epochs):
        final_epochs = epoch >= (epochs // 2)
        f_train = final if final_epochs else train
        model.trainable = True
        total_loss = 0.0
        total_errors = tf.zeros(output_dim)  # Accumulate total X, Y errors
        num_batches = len(f_train)
        mode = train.__class__.__name__
        
        #if final_epochs: print('final epochs mode')
        for i in range(num_batches):
            try:
                data, labels = f_train[i]
                inputs   = {}
                for name in f_train.inputs:
                    inputs[name] = data[name]
                if not input_shape:
                    input_shape = [inputs[name].shape[1:] for name in f_train.inputs]
                    if len(f_train.inputs) == 1:
                        input_shape = input_shape[0]
                
                outputs = model(inputs, training=True)

                # forward pass and calculate gradients
                with tf.GradientTape() as tape:
                    outputs = model(inputs, training=True)
                    loss    = f_train.loss(labels, outputs)
                
                # Backward pass and update weights
                gradients = tape.gradient(loss, model.trainable_variables)
                optimizer.apply_gradients(zip(gradients, model.trainable_variables))
                
                # Calculate metrics
                total_loss   += loss.numpy()
                batch_error   = tf.reduce_mean(tf.abs(outputs - labels), axis=0)
                total_errors += batch_error
                progress      = (i + 1) / num_batches
                avg_loss      = total_loss / (i + 1)
                avg_errors    = (total_errors / (i + 1)).numpy()
                fin           = progress >= 0.9999
                PCOLOR        = GREEN if fin else CYAN
                final_training = 'finalize' if final_epochs else 'training'
                labels = ['X', 'Y', 'Z', 'W', 'AA', 'BB', 'CC'][:len(avg_errors)]  # Only take as many labels as needed
                error_str = ", ".join(f"{label}={error:.4f}" for label, error in zip(labels, avg_errors))
                status        = (f'\r{YELLOW}{final_training} {GRAY}{BLUE}{epoch+1:4}/{epochs:4}{GRAY} | ' +
                        PCOLOR + '━' * math.floor(progress * bar_length) + f'╸{GRAY}' +
                        '━' * math.ceil((1.0 - progress) * bar_length - 1) +
                        f'{GRAY} | {YELLOW}{avg_loss:.4f} {GRAY}|{BLUE} {error_str}{RESET}')
                
                print(status, end='')
            except Exception as e:
                print(f"\nerror processing batch {i}: {str(e)}")
                continue
        
        # Validation phase
        model.trainable  = False
        val_loss         = 0.0
        total_val_errors = tf.zeros(output_dim)
        num_val_batches  = len(val)
        
        # Iterate through each validation batch
        for j in range(num_val_batches):
            try:
                # Unpack the validation data
                if mode == 'target':
                    val_images, val_labels = val[j]
                    #val_images = val_images[model_inputs[0]] if len(model_inputs) == 1 else [val_images[name] for name in model_inputs]

                    val_outputs   = model(val_images, training=False)
                else:
                    val_inputs, val_labels = val[j]
                    val_outputs   = model(val_inputs, training=False)
                
                # Calculate validation loss and errors
                val_step_loss     = train.loss(val_labels, val_outputs).numpy()
                val_loss         += val_step_loss
                val_batch_error   = tf.reduce_mean(tf.abs(val_outputs - val_labels), axis=0)
                total_val_errors += val_batch_error
            except Exception as e:
                print(f"\nError processing validation batch {j}: {str(e)}")
                continue
        
        # Calculate final validation metrics
        if num_val_batches > 0:
            avg_val_loss = val_loss / num_val_batches
            avg_val_errors = (total_val_errors / num_val_batches).numpy()
        else:
            avg_val_loss = 0
            avg_val_errors = [0, 0]
        
        # Print validation results on a new line
        labels = ['X', 'Y', 'Z', 'W', 'AA', 'BB', 'CC'][:len(avg_val_errors)]  # Only take as many labels as needed
        error_str = ", ".join(f"{label}={error:.4f}" for label, error in zip(labels, avg_val_errors))
    
        print(f'\n{BLUE}validate' + ' ' * 57 +
              f'{avg_val_loss:.4f} {GRAY}|{PURPLE} {error_str}{RESET}\n')
    
    model.save(f'res/models/vision_{mode}.keras')
    export_json(model, mode, f'res/models/vision_{mode}.json', input_shape)

def mix(a, b, f = 0.5):
    return [(a[0] + b[0]) * (1 - f), (a[1] + b[1]) * f]

# should be a model, i think.  an object has these fields so we may describe it as such.  just an object!
# this bring annotations and object inference into object lifecycle and thats where we design?
# otherwise hey its just unshaped json.. although, hubris apparently because versioning is just more of an issue after changing to a model
# the best idea is to not over implement now, and where the hell was i
def get_annots(session_dir, filename, id):
    image_name = filename.replace('f2_', f'f{id}_')
    image_path = os.path.join(session_dir, image_name)
    json_path  = os.path.join(session_dir, image_name.replace(".png", ".json"))
    iris_mid   = None
    eye_left   = None
    eye_right  = None
    scale      = None
    center     = None
    offset     = None
    if os.path.exists(json_path):
        with open(json_path, "r") as f:
            json_data = json.load(f)
            labels    = json_data['labels']
            iris_mid  = labels.get('iris-mid')
            eye_left  = labels.get("eye-left")
            eye_right = labels.get("eye-right")
            scale     = labels.get("scale")
            center    = labels.get("center")
            if center:
                center[0] = center[0] - 0.5
                center[1] = center[1] - 0.5
            offset    = labels.get("offset")
            if offset:
                offset[0] = offset[0] - 0.5
                offset[1] = offset[1] - 0.5

    if eye_left is not None and eye_right is not None:
        iris_mid = mix(eye_left, eye_right)
    return image_path, iris_mid, eye_left, eye_right, scale, center, offset


def shuffle(
    dataset_obj: Any,
    validation_split: float = 0.1,
    repeat: int = 1
) -> Tuple[Any, Any]:
    # get all fields that are lists, check for same length
    fields = [field for field in dir(dataset_obj) if (not field.startswith('__') and field != 'train') and isinstance(getattr(dataset_obj, field), list)]
    field_lengths = [len(getattr(dataset_obj, field)) for field in fields]
    if len(set(field_lengths)) > 1:
        raise ValueError(f"All data fields must have the same length. Got lengths: {field_lengths}")
    
    # dataset instances
    train_dataset      = dataset_obj.__class__(True)
    validation_dataset = dataset_obj.__class__(False)
    if not fields or not field_lengths[0]:
        return train_dataset, validation_dataset
    
    # Extract data from each field
    lists = [getattr(dataset_obj, field) for field in fields]
    
    # Combine all fields together to maintain alignment
    combined = list(zip(*lists))

    # Shuffle dataset to randomize order
    random.shuffle(combined)

    # Calculate split point
    base_count = len(combined)
    validation_count = round(base_count * validation_split)
    index = base_count - validation_count
    val_combined = combined[index:]

    # **Repeat Entire Combined Dataset Instead of Individual Fields**
    train_combined = combined[:index] * repeat  # Repeat whole entries to maintain alignment
    random.shuffle(train_combined)  # Shuffle again to mix repeated data properly
    
    # Unpack aligned data back into their respective fields
    for i, field in enumerate(fields):
        setattr(train_dataset, field, [row[i] for row in train_combined])
        setattr(validation_dataset, field, [row[i] for row in val_combined])

    return train_dataset, validation_dataset


def center_crop(image, size, iris_mid):
    full_size  = image.shape[0]  # square images
    crop_x     = int((iris_mid[0] + 0.5) * full_size)
    crop_y     = int((iris_mid[1] + 0.5) * full_size)
    start_x    = crop_x - size // 2
    start_y    = crop_y - size // 2
    end_x      = start_x + size
    end_y      = start_y + size
    pad_top    = max(0, -start_y)
    pad_left   = max(0, -start_x)
    pad_bottom = max(0, end_y - full_size)
    pad_right  = max(0, end_x - full_size)
    start_x    = max(0, start_x)
    start_y    = max(0, start_y)
    end_x      = min(full_size, end_x)
    end_y      = min(full_size, end_y)

    # If the entire crop is out of bounds, return a blank image
    if start_x >= full_size or start_y >= full_size or end_x <= 0 or end_y <= 0:
        return np.zeros((size, size), dtype=image.dtype)

    # Extract the valid crop from the image
    crop = image[start_y:end_y, start_x:end_x]

    # If the extracted crop is empty, return a black image
    if crop.size == 0:
        return np.zeros((size, size), dtype=image.dtype)

    # Apply padding if needed
    padded_crop = cv2.copyMakeBorder(
        crop, pad_top, pad_bottom, pad_left, pad_right,
        cv2.BORDER_CONSTANT, value=0)
    return padded_crop

import copy

class dataset:
    def __init__(self, train):
        self.train         = train
        self.f2_image      = []
        self.f2_iris_mid   = []
        self.f2_eye_left   = []
        self.f2_eye_right  = []
        self.f2_scale      = []
        self.f6_image      = []
        self.f6_iris_mid   = []
        self.f6_eye_left   = []
        self.f6_eye_right  = []
        self.f6_scale      = []
        self.pixel         = []
        self.center        = []
        self.offset        = []
        self.f2_cache      = []
        self.f6_cache      = []
        self.f2_iris_mid_i = []
        self.f6_iris_mid_i = []
    def __len__(self):
        return len(self.f2_image)
    def copy(self): return copy.deepcopy(self)

    def augment_final(self, magnitude=0):
        """Appends data from train to final when iris_mid[0] > 0.5."""
        independent = self.f2_iris_mid.copy()
        for m in range(magnitude):
            for index, iris_mid in enumerate(independent):  
                if iris_mid and (abs(iris_mid[0]) > 0.33 or abs(iris_mid[1]) > 0.33):  # set on model, the caller has access
                    # Append data from self.train to all lists in self.final
                    self.f2_image.append(self.f2_image[index])
                    self.f2_iris_mid.append(self.f2_iris_mid[index])
                    self.f2_eye_left.append(self.f2_eye_left[index])
                    self.f2_eye_right.append(self.f2_eye_right[index])
                    self.f2_scale.append(self.f2_scale[index])
                    self.f6_image.append(self.f6_image[index])
                    self.f6_iris_mid.append(self.f6_iris_mid[index])
                    self.f6_eye_left.append(self.f6_eye_left[index])
                    self.f6_eye_right.append(self.f6_eye_right[index])
                    self.f6_scale.append(self.f6_scale[index])
                    self.pixel.append(self.pixel[index])
                    self.center.append(self.center[index])
                    self.offset.append(self.offset[index])
                    self.f2_cache.append(self.f2_cache[index])
                    self.f6_cache.append(self.f6_cache[index])
                    self.f2_iris_mid_i.append(self.f2_iris_mid_i[index])
                    self.f6_iris_mid_i.append(self.f6_iris_mid_i[index])

    def shuffle_data(self):
        """Shuffles all lists together to maintain data integrity."""
        combined = list(zip(
            self.f2_image, self.f2_iris_mid, self.f2_eye_left, self.f2_eye_right, self.f2_scale,
            self.f6_image, self.f6_iris_mid, self.f6_eye_left, self.f6_eye_right, self.f6_scale,
            self.pixel, self.center, self.offset, self.f2_cache, self.f6_cache, self.f2_iris_mid_i, self.f6_iris_mid_i
        ))
        np.random.shuffle(combined)  # Shuffle while keeping lists aligned
        (
            self.f2_image, self.f2_iris_mid, self.f2_eye_left, self.f2_eye_right, self.f2_scale,
            self.f6_image, self.f6_iris_mid, self.f6_eye_left, self.f6_eye_right, self.f6_scale,
            self.pixel, self.center, self.offset, self.f2_cache, self.f6_cache, self.f2_iris_mid_i, self.f6_iris_mid_i
        ) = zip(*combined)  # Unpack back to separate lists

        # Convert back to lists after shuffling
        self.f2_image       = list(self.f2_image)
        self.f2_iris_mid    = list(self.f2_iris_mid)
        self.f2_eye_left    = list(self.f2_eye_left)
        self.f2_eye_right   = list(self.f2_eye_right)
        self.f2_scale       = list(self.f2_scale)
        self.f6_image       = list(self.f6_image)
        self.f6_iris_mid    = list(self.f6_iris_mid)
        self.f6_eye_left    = list(self.f6_eye_left)
        self.f6_eye_right   = list(self.f6_eye_right)
        self.f6_scale       = list(self.f6_scale)
        self.pixel          = list(self.pixel)
        self.center         = list(self.center)
        self.offset         = list(self.offset)
        self.f2_cache       = list(self.f2_cache)
        self.f6_cache       = list(self.f6_cache)
        self.f2_iris_mid_i  = list(self.f2_iris_mid_i)
        self.f6_iris_mid_i  = list(self.f6_iris_mid_i)

class data:
    def __init__(self, mode, session_ids, required_fields, validation_split=0.1, final_magnitude=1, repeat=1):
        # do not expose all data at once, that would allow train to leak with validation
        _dataset = dataset(False)
        for session_id in session_ids:
            session_dir = f'sessions/{session_id}'
            assert os.path.exists(session_dir), f'dir does not exist: {session_dir}'
            for filename in os.listdir(session_dir):
                if filename.startswith("f2_") and filename.endswith(".png"):
                    parts = filename.replace(".png", "").split("_")
                    pixel_x = pixel_y = 0
                    center_x = center_y = 0
                    offset_x = offset_y = 0
                    if   len(parts) == 4: _, _, pixel_x, pixel_y          = parts
                    elif len(parts) >= 7: _, _, pixel_x, pixel_y, _, _, _ = parts
                    else:
                        continue

                    pixel_x = float(pixel_x) - 0.5
                    pixel_y = float(pixel_y) - 0.5

                    # we need json and image-path for each f2, f6
                    # this model takes in two images, however should support different ones provided, to turn the inputs on and off (omit from model)
                    f2_image_path, f2_iris_mid, f2_eye_left, f2_eye_right, f2_scale, f2_center, f2_offset = get_annots(session_dir, filename, 2)
                    f6_image_path, f6_iris_mid, f6_eye_left, f6_eye_right, f6_scale, f6_center, f6_offset = get_annots(session_dir, filename, 6)
                    
                    #f2_eye_left  = None
                    #f6_eye_left  = None
                    #f2_iris_mid  = None
                    #f6_iris_mid  = None
                    #f2_eye_right = None
                    #f6_eye_right = None
                    #f2_scale     = 0.3
                    #f6_scale     = 0.3

                    # based on mode, this will need to be a filter (define on model and give to data?)
                    # then data->model calls the model?
                    # it is more direct
                    #if not f2_iris_mid or not f6_iris_mid or not f2_scale or not f6_scale:
                    #    continue
                    require_both = mode == 'track'
                    use_f2  = (not 'iris_mid'  in required_fields or f2_iris_mid  is not None) and \
                              (not 'eye_left'  in required_fields or f2_eye_left  is not None) and \
                              (not 'eye_right' in required_fields or f2_eye_right is not None) and \
                              (not 'center'    in required_fields or f2_center    is not None) and \
                              (not 'offset'    in required_fields or f2_offset    is not None) and \
                              (not 'scale'     in required_fields or f2_scale     is not None)
                    use_f6  = (not 'iris_mid'  in required_fields or f6_iris_mid  is not None) and \
                              (not 'eye_left'  in required_fields or f6_eye_left  is not None) and \
                              (not 'eye_right' in required_fields or f6_eye_right is not None) and \
                              (not 'center'    in required_fields or f6_center    is not None) and \
                              (not 'offset'    in required_fields or f6_offset    is not None) and \
                              (not 'scale'     in required_fields or f6_scale     is not None)

                    if f6_center or f2_center:
                        assert f6_center and f2_center, "expected centers"
                        assert f6_center[0] == f2_center[0] and f6_center[1] == f2_center[1], 'expected centers to be equal'
                    if f6_offset or f2_offset:
                        assert f6_offset and f2_offset, "expected offsets"
                        assert f6_offset[0] == f2_offset[0] and f6_offset[1] == f2_offset[1], 'expected offsets to be equal'
                    
                    # some models require both inupts, so we must also support those modes
                    if (require_both and (not use_f2 or not use_f6)) or (not use_f2 and not use_f6):
                        continue
                    
                    # we always make 2 sets of dataset entries, f2 and f6's so they are always lined up. when we dont have the data we must do this
                    if not use_f2:
                        f2_image_path = f6_image_path
                        f2_iris_mid   = f6_iris_mid
                        f2_eye_left   = f6_eye_left
                        f2_eye_right  = f6_eye_right
                        f2_scale      = f6_scale
                        use_f2        = True
                    
                    if use_f2:
                        _dataset.f2_image     .append(f2_image_path)
                        _dataset.f2_iris_mid  .append(f2_iris_mid)
                        _dataset.f2_eye_left  .append(f2_eye_left)
                        _dataset.f2_eye_right .append(f2_eye_right)
                        _dataset.f2_scale     .append(f2_scale)
                        _dataset.f2_iris_mid_i.append(None) # this is so the labeler can perform an inference on this image, then store the result for next use (epoch)
                        if not use_f6:
                            f6_image_path = f2_image_path
                            f6_iris_mid   = f2_iris_mid
                            f6_eye_left   = f2_eye_left
                            f6_eye_right  = f2_eye_right
                            f6_scale      = f2_scale

                    _dataset.f6_image     .append(f6_image_path)
                    _dataset.f6_iris_mid  .append(f6_iris_mid)
                    _dataset.f6_eye_left  .append(f6_eye_left)
                    _dataset.f6_eye_right .append(f6_eye_right)
                    _dataset.f6_scale     .append(f6_scale)
                    _dataset.f6_iris_mid_i.append(None)

                    _dataset.pixel        .append([pixel_x, pixel_y])
                    _dataset.center       .append([f2_center[0], f2_center[1]] if f2_center is not None else None)
                    _dataset.offset       .append([f2_offset[0], f2_offset[1]] if f2_offset is not None else None)
                    _dataset.f2_cache     .append(None)
                    _dataset.f6_cache     .append(None)

        # we want self.train to contain the same type as _dataset, with its arrays occupied.  shuffle will go through the object fields the user put in (making sure we dont also look at python base fields??)
        self.train, self.validation = shuffle(_dataset, repeat=repeat, validation_split=validation_split)
        self.final = self.train.copy()
        self.final.augment_final(final_magnitude)
        self.final.shuffle_data()