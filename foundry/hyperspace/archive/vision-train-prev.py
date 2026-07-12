#!/bin/env python3
import tensorflow as tf
from tensorflow.keras import KerasTensor
from tensorflow.keras.layers import InputLayer
from tensorflow.keras.regularizers import l2
import matplotlib.pyplot as plt
import os
import math
import numpy as np
import cv2
import time
import json
import random
import argparse
from collections import OrderedDict

# ------------------------------------------------------------------
# hyper-parameters and mode selection
# ------------------------------------------------------------------
parser        = argparse.ArgumentParser(description="Annotate images with click-based labels.")
parser.add_argument("-s", "--session", required=True,   help="session data (name of session; as named in cwd/sessions dir)")
parser.add_argument("-m", "--mode",    default='track', help='model-type to create; types = target (to obtain eye-center) and track (to infer screen pos, with eye center as paramter in input model)')
parser.add_argument("-q", "--quality", default=2)
parser.add_argument("-r", "--repeat",  default=1)
parser.add_argument("-e", "--epochs",  default=500)
parser.add_argument("-i", "--input",   default=64)
parser.add_argument("-b", "--batch",   default=1)
parser.add_argument("-lr", "--learning-rate", default=0.0001)
args          = parser.parse_args()
session_ids   = args.session.split(',')
quality       = int(args.quality) # top level scaling factor
resize        = int(args.input)   # track: crop (128x128) or entire image (340x340) is resized to 64x64
input_size    = resize
size          = 128          # crop size for track mode
full_size     = 340          # original full image size
batch_size    = int(args.batch)
learning_rate = float(args.learning_rate)
num_epochs    = int(args.epochs)
mode          = args.mode
is_target     = mode == 'target'

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
    
def get_track_center(session_dir, filename, id):
    image_path = os.path.join(session_dir, filename.replace('f2_', f'f{id}_'))
    json_path  = os.path.join(session_dir, filename.replace('f2_', f'f{id}_').replace(".png", ".json"))
    
    # fallback to f2 when we dont have camera data; however, this must be in ALL cases (we cannot mix)
    # debug setting here
    if not os.path.exists(json_path):
        image_path = os.path.join(session_dir, filename)
        json_path  = os.path.join(session_dir, filename.replace(".png", ".json"))
        
    if os.path.exists(json_path):
        with open(json_path, "r") as f:
            json_data = json.load(f)
            crop_pos = json_data["labels"][0]["eye-center"]
            assert os.path.exists(image_path), f'missing camera image {image_path}'
            return image_path, crop_pos
    else:
        print('annotations incomplete in dataset')
        exit(2)



class TargetDataGenerator(tf.keras.utils.Sequence):
    def __init__(self, validation_split=0.1, train=True, batch_size=1, repeat=1):
        self.images     = []  # Path to image files
        self.targets    = []  # Target coordinates for each image
        self.train      = train
        self.batch_size = batch_size
        self.repeat     = repeat
        
        # Load images from each session
        for session_id in session_ids:
            session_dir = f'sessions/{session_id}'
            assert os.path.exists(session_dir), f'Directory does not exist: {session_dir}'
            
            for filename in os.listdir(session_dir):
                if filename.endswith(".png"):
                    image_path = os.path.join(session_dir, filename)
                    json_path = os.path.join(session_dir, filename.replace(".png", ".json"))
                    
                    # Get target coordinates from JSON or skip
                    if os.path.exists(json_path):
                        with open(json_path, "r") as f:
                            try:
                                json_data = json.load(f)
                                target_pos = json_data["labels"][0]["eye-center"]
                                # Store paths and targets
                                self.images.append(image_path)
                                self.targets.append(target_pos)
                            except (KeyError, json.JSONDecodeError, IndexError) as e:
                                print(f"Warning: Invalid JSON for {filename}: {e}")
                    else:
                        # Optionally parse from filename if no JSON
                        print(f"Warning: No JSON file for {filename}")
        
        # Shuffle dataset
        combined = list(zip(self.images, self.targets))
        random.shuffle(combined)
        self.images, self.targets = zip(*combined)
        
        # Convert tuples to lists for mutability
        self.images = list(self.images)
        self.targets = list(self.targets)
        
        # Split into train and validation
        split_idx = int(len(self.images) * (1 - validation_split))
        if train:
            self.images = self.images[:split_idx]
            self.targets = self.targets[:split_idx]
            
            # Apply repeat if specified
            if self.repeat > 1:
                self.images = self.images * self.repeat
                self.targets = self.targets * self.repeat
                
                combined = list(zip(self.images, self.targets))
                random.shuffle(combined)
                self.images, self.targets = zip(*combined)
                
                # Convert back to lists
                self.images = list(self.images)
                self.targets = list(self.targets)
        else:
            self.images = self.images[split_idx:]
            self.targets = self.targets[split_idx:]
    
    def __getitem__(self, idx):
        # Calculate batch indices
        start_idx = idx * self.batch_size
        end_idx = min((idx + 1) * self.batch_size, len(self.images))
        
        # Initialize batch arrays
        batch_images = []
        batch_targets = []
        
        # Process each item in the batch
        for i in range(start_idx, end_idx):
            # Load image
            image_path = self.images[i]
            image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
            
            if image is None:
                raise ValueError(f"Could not read image: {image_path}")
            
            # Get target coordinates
            target = self.targets[i].copy()
            
            # Apply augmentations for training
            if self.train:
                # Random horizontal flip
                if random.random() < 0.5:
                    image = cv2.flip(image, 1)
                    target[0] = -target[0]  # Flip x-coordinate
                
                # Random crop & scale
                if random.random() < 0.5:
                    new_h = new_w = int(full_size * random.uniform(0.85, 1.0))
                    max_trans_x = full_size - new_w
                    max_trans_y = full_size - new_h
                    start_x = random.randint(0, max_trans_x)
                    start_y = random.randint(0, max_trans_y)
                    
                    # Apply crop
                    image = image[start_y:start_y + new_h, start_x:start_x + new_w]
                    
                    # Adjust target coordinates
                    tl_x = ((target[0] + 0.5) * full_size - start_x)
                    tl_y = ((target[1] + 0.5) * full_size - start_y)
                    target = [(tl_x / new_w) - 0.5, (tl_y / new_h) - 0.5]
            
            # Resize and normalize image
            image = cv2.resize(image, (input_size, input_size))
            image = image / 255.0
            image = np.expand_dims(image, axis=-1)
            
            # Add to batch
            batch_images.append(image)
            batch_targets.append(target)
        
        # Convert to numpy arrays
        batch_images = np.array(batch_images)
        batch_targets = np.array(batch_targets)
        
        # Return in the format expected by the model
        return batch_images, batch_targets
    
    def __len__(self):
        return (len(self.images) + self.batch_size - 1) // self.batch_size

    def model(self):
        image = tf.keras.layers.Input(shape=(input_size, input_size, 1), name='image')
        x = tf.keras.layers.Conv2D       (name='conv0', filters=8 * quality, kernel_size=3, padding='same')(image)
        x = tf.keras.layers.ReLU         (name='relu0')(x)
        x = tf.keras.layers.MaxPooling2D (name='pool0', pool_size=(2, 2))(x)
        x = tf.keras.layers.Conv2D       (name='conv1', filters=16 * quality, kernel_size=3, padding='same')(x)
        x = tf.keras.layers.ReLU         (name='relu1')(x)
        x = tf.keras.layers.MaxPooling2D (name='pool1', pool_size=(2, 2))(x)
        x = tf.keras.layers.Flatten      (name='flatten')(x)
        x = tf.keras.layers.Dense        (name='dense0', units=quality * 64, activation='relu')(x)
        x = tf.keras.layers.Dense        (name='dense1', units=2)(x)
        return tf.keras.Model            (name=mode, inputs=image, outputs=x)


# ------------------------------------------------------------------
# Data generator class (equivalent to PyTorch's DataLoader)
# ------------------------------------------------------------------
class TrackDataGenerator(tf.keras.utils.Sequence):
    def __init__(self, validation_split=0.1, train=True, batch_size=1, repeat=1):
        self.mode          = mode  # "track" or "target"
        self.f2_images     = [] # we want all of these to line up
        self.f2_offsets    = [] # ...
        self.f6_images     = [] # ...
        self.f6_offsets    = [] # ...
        self.train         = train
        self.batch_size    = batch_size
        self.repeat        = repeat  # Only apply repeat to training data
        self.labels        = []

        for session_id in session_ids:
            session_dir = f'sessions/{session_id}'
            assert os.path.exists(session_dir), f'dir does not exist: {session_dir}'
            for filename in os.listdir(session_dir):
                if filename.startswith("f2_") and filename.endswith(".png"):
                    parts = filename.replace(".png", "").split("_")
                    if len(parts) == 7:
                        _, _, eye_x, eye_y, head_x, head_y, head_z = parts
                        # we need json and image-path for each f2, f6
                        # this model takes in two images, however should support different ones provided, to turn the inputs on and off (omit from model)
                        f2_image_path, f2_offset = get_track_center(session_dir, filename, 2)
                        self.f2_images .append(f2_image_path)
                        self.f2_offsets.append(f2_offset)

                        f6_image_path, f6_offset = get_track_center(session_dir, filename, 6)
                        self.f6_images .append(f6_image_path)
                        self.f6_offsets.append(f6_offset)
                        self.labels.append([float(eye_x), float(eye_y)])


        # shuffle dataset, overwrite our members
        combined = list(zip(self.f2_images, self.f2_offsets, self.f6_images, self.f6_offsets, self.labels))
        random.shuffle(combined)
        self.f2_images, self.f2_offsets, self.f6_images, self.f6_offsets, self.labels = zip(*combined)

        assert len(self.labels) == len(self.f2_images), "length mismatch"

        # split into train and validation
        split_idx = int(len(self.f2_images) * (1 - validation_split))
        if train:
            self.f2_images  = self.f2_images [:split_idx]
            self.f6_images  = self.f6_images [:split_idx]
            self.f2_offsets = self.f2_offsets[:split_idx]
            self.f6_offsets = self.f6_offsets[:split_idx]
            self.labels     = self.labels    [:split_idx]

            # apply repeat to training data if specified
            if self.repeat > 1:
                # Create repeated copies of the data
                self.f2_images  = list(self.f2_images) * self.repeat
                self.f2_offsets = list(self.f2_offsets)  * self.repeat
                self.f6_images  = list(self.f6_images) * self.repeat
                self.f6_offsets = list(self.f6_offsets)  * self.repeat
                self.labels     = list(self.labels) * self.repeat
                combined        = list(zip(self.f2_images, self.f2_offsets, self.f6_images, self.f6_offsets, self.labels))
                random.shuffle(combined)
                self.f2_images, self.f2_offsets, self.f6_images, self.f6_offsets, self.labels = zip(*combined)
        else:
            self.f2_images  = self.f2_images [split_idx:]
            self.f2_offsets = self.f2_offsets[split_idx:]
            self.f6_images  = self.f6_images [split_idx:]
            self.f6_offsets = self.f6_offsets[split_idx:]
            self.labels     = self.labels    [split_idx:]

    def center_crop(self, image, offset):
        full_size = image.shape[0]  # Assuming square images

        crop_x = int((offset[0] + 0.5) * full_size)
        crop_y = int((offset[1] + 0.5) * full_size)

        start_x = crop_x - size // 2
        start_y = crop_y - size // 2
        end_x = start_x + size
        end_y = start_y + size

        # Calculate padding for cases where the crop exceeds image bounds
        pad_top = max(0, -start_y)
        pad_left = max(0, -start_x)
        pad_bottom = max(0, end_y - full_size)
        pad_right = max(0, end_x - full_size)

        # Adjust crop coordinates to valid bounds
        start_x = max(0, start_x)
        start_y = max(0, start_y)
        end_x = min(full_size, end_x)
        end_y = min(full_size, end_y)

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
            cv2.BORDER_CONSTANT, value=0  # Black padding
        )

        return padded_crop


    def __getitem__(self, idx):
        start_idx        = idx * self.batch_size
        end_idx          = min((idx + 1) * self.batch_size, len(self.f2_images))
        batch_f2         = []
        batch_f6         = []
        batch_labels     = []
        batch_f2_offsets = []
        batch_f6_offsets = []

        # Process the batch
        for i in range(start_idx, end_idx):
            # Get image paths for both cameras
            f2_image_path = self.f2_images[i]
            f6_image_path = self.f6_images[i]
            
            # Get labels and crop offsets
            l         = self.labels[i].copy()
            f2_offset = self.f2_offsets[i]
            f6_offset = self.f6_offsets[i]
            
            # Process camera 2 image
            f2_image  = cv2.imread(f2_image_path, cv2.IMREAD_GRAYSCALE)
            f6_image  = cv2.imread(f6_image_path, cv2.IMREAD_GRAYSCALE)
            
            # For track mode, crop based on provided offset
            f2_image  = self.center_crop(f2_image, f2_offset)
            f6_image  = self.center_crop(f6_image, f6_offset)

            # override offset parameters now (after we crop) with truth values; so should train to 0 loss 
            #f2_offset = [l[0], l[1]] <-- verify truth and use of label
            #f6_offset = [l[0], l[1]]
            f2_offset = np.array(f2_offset).astype(np.float32)
            f6_offset = np.array(f6_offset).astype(np.float32)
            label     = l
            
            # Resize and normalize both images
            f2_image  = cv2.resize(f2_image, (input_size, input_size), interpolation=cv2.INTER_AREA)
            f2_image  = (f2_image / 255.0).astype(np.float32)
            f2_image  = np.expand_dims(f2_image, axis=-1)

            f6_image  = cv2.resize(f6_image, (input_size, input_size), interpolation=cv2.INTER_AREA)
            f6_image  = (f6_image / 255.0).astype(np.float32)
            f6_image  = np.expand_dims(f6_image, axis=-1)
            
            #if abs(f2_offset[0]) > 0.4:
            #    plt.imshow(f2_image, cmap='gray', extent=[-0.5, 0.5, 0.5, -0.5])
            #    plt.show()
            #if abs(f6_offset[0]) > 0.4:
            #    plt.imshow(f6_image, cmap='gray', extent=[-0.5, 0.5, 0.5, -0.5])
            #    plt.show()

            # Add to batch arrays
            batch_f2        .append(f2_image)
            batch_f6        .append(f6_image)
            batch_f2_offsets.append(f2_offset)
            batch_f6_offsets.append(f6_offset)
            batch_labels    .append(label)
        
        batch_f6         = np.array(batch_f6)
        batch_f2         = np.array(batch_f2)
        batch_f6_offsets = np.array(batch_f6_offsets)
        batch_f2_offsets = np.array(batch_f2_offsets)
        batch_labels     = np.array(batch_labels)

        return {
            'input_f2':         batch_f2,
            'input_f2_offset':  batch_f2_offsets,
            'input_f6':         batch_f6,
            'input_f6_offset':  batch_f6_offsets
        }, batch_labels
            
    def __len__(self):
        if len(self.f2_images) == 0: return 0
        return max(1, (len(self.f2_images) + self.batch_size - 1) // self.batch_size)
    
    def model(self):
        input_f2        = tf.keras.layers.Input(name='input_f2',        shape=(input_size, input_size, 1))
        input_f2_offset = tf.keras.layers.Input(name='input_f2_offset', shape=(2,))
        input_f6        = tf.keras.layers.Input(name='input_f6',        shape=(input_size, input_size, 1))
        input_f6_offset = tf.keras.layers.Input(name='input_f6_offset', shape=(2,))

        def conv2D_for(id, image, offset):
            x = tf.keras.layers.Conv2D       (name=f'f{id}_conv0', activation='relu', filters=16 * quality, kernel_size=3, padding='same')(image)
            x = tf.keras.layers.MaxPooling2D (name=f'f{id}_pool0', pool_size=(2, 2))(x)
            x = tf.keras.layers.Conv2D       (name=f'f{id}_conv1', activation='relu', filters=32 * quality, kernel_size=3, padding='same')(x)
            x = tf.keras.layers.MaxPooling2D (name=f'f{id}_pool1', pool_size=(2, 2))(x)
            x = tf.keras.layers.Flatten      (name=f'f{id}_flatten')(x)
            return x

        x0 = conv2D_for(2, input_f2, input_f2_offset)
        x1 = conv2D_for(6, input_f6, input_f6_offset)
        x0 = tf.keras.layers.Concatenate (name='concat0')([x0, input_f2_offset]) 
        e0 = tf.keras.layers.Dense(quality * 32, activation='relu', name="e0")(x0)
        x1 = tf.keras.layers.Concatenate (name='concat1')([x1, input_f6_offset]) 
        e1 = tf.keras.layers.Dense(quality * 32, activation='relu', name="e1")(x1)

        x  = tf.keras.layers.Concatenate (name='concat2')([e0, e1]) # i can train with x0 or x1, but if i concat them, its worse than combining by a LOT.. makes no sense?
        x  = tf.keras.layers.Dense       (name='dense0', units=quality * 8, activation='relu')(x)
        x  = tf.keras.layers.Dense       (name='dense1', units=2)(x)
        return tf.keras.Model            (name=mode, inputs=[input_f2, input_f2_offset, input_f6, input_f6_offset], outputs=x)

# ------------------------------------------------------------------
# Training function
# ------------------------------------------------------------------
def train(model, train_generator, val_generator, epochs):
    # Set up optimizer
    optimizer = tf.keras.optimizers.Adam(learning_rate=learning_rate)
    loss_fn = tf.keras.losses.MeanSquaredError()
    bar_length = 40
    
    # Training and validation loops
    for epoch in range(epochs):
        # Training phase
        model.trainable = True
        total_loss = 0.0
        total_errors = tf.zeros(2)  # Accumulate total X, Y errors
        num_batches = len(train_generator)
        for i in range(num_batches):
            # Get the data for this batch
            try:
                images, labels = train_generator[i]
                outputs = model(images, training=True)

                # Forward pass and calculate gradients
                with tf.GradientTape() as tape:
                    outputs = model(inputs, training=True)
                    loss = loss_fn(labels, outputs)
                
                # Backward pass and update weights
                gradients = tape.gradient(loss, model.trainable_variables)
                optimizer.apply_gradients(zip(gradients, model.trainable_variables))
                
                # Calculate metrics
                total_loss += loss.numpy()
                batch_error = tf.reduce_mean(tf.abs(outputs - labels), axis=0)
                total_errors += batch_error
                progress = (i + 1) / num_batches
                avg_loss = total_loss / (i + 1)
                avg_errors = (total_errors / (i + 1)).numpy()
                fin = progress >= 0.9999
                PCOLOR = GREEN if fin else CYAN
                status = (f'\r{YELLOW}training {GRAY}{BLUE}{epoch+1:4}/{epochs:4}{GRAY} | ' +
                        PCOLOR + '━' * math.floor(progress * bar_length) + f'╸{GRAY}' +
                        '━' * math.ceil((1.0 - progress) * bar_length - 1) +
                        f'{GRAY} | {YELLOW}{avg_loss:.4f} {GRAY}|{BLUE} X={avg_errors[0]:.4f}, Y={avg_errors[1]:.4f}{RESET}')
                
                print(status, end='')
            except Exception as e:
                print(f"\nError processing batch {i}: {str(e)}")
                continue
        
        # Validation phase
        model.trainable = False
        val_loss = 0.0
        total_val_errors = tf.zeros(2)
        num_val_batches = len(val_generator)
        
        # Iterate through each validation batch
        for j in range(num_val_batches):
            try:
                # Unpack the validation data
                if mode == 'target':
                    val_images, val_labels = val_generator[j]
                    val_outputs = model(val_images, training=False)
                else:
                    val_inputs, val_labels = val_generator[j]
                    val_outputs = model(val_inputs, training=False)
                
                # Calculate validation loss and errors
                val_step_loss = loss_fn(val_labels, val_outputs).numpy()
                val_loss += val_step_loss
                val_batch_error = tf.reduce_mean(tf.abs(val_outputs - val_labels), axis=0)
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
        print(f'\n{BLUE}validate' + ' ' * 57 +
              f'{avg_val_loss:.4f} {GRAY}|{PURPLE} X={avg_val_errors[0]:.4f}, Y={avg_val_errors[1]:.4f}{RESET}\n')
    
    model.save(f'vision_{mode}.keras')
    export_json(model, f'vision_{mode}.json')


def export_part(layer, part, index):
    w = layer.get_weights()
    if   w[0].dtype == np.float32: weight_ext = '.f32'
    elif w[0].dtype == np.float16: weight_ext = '.f16'
    elif w[0].dtype == np.uint8:   weight_ext = '.u8'
    weights_filename = f"{mode}_{layer.name}_{part}{weight_ext}"
    with open(weights_filename, 'wb') as f: w[index].tofile(f)
    return weights_filename

def export_json(model, output_json_path):
    layers_dict = OrderedDict()
    output_shapes = {}
    # Function to get layer type
    def get_layer_type(layer):
        layer_class = layer.__class__.__name__
        if   layer_class == 'Conv2D':       return "conv"
        elif layer_class == 'ReLU':         return "relu"
        elif layer_class == 'MaxPooling2D': return "pool"
        elif layer_class == 'Dense':        return "dense"
        elif layer_class == 'Flatten':      return "flatten"
        elif layer_class == 'InputLayer':   return "input"
        elif layer_class == 'Concatenate':  return "concatenate"
        else:
            return layer_class
    
    # Run a sample inference to ensure all shapes are computed
    if mode == 'target':
        sample_input  = np.zeros((1, input_size, input_size, 1))
        _ = model(sample_input)
    else:
        sample_image  = np.zeros((1, input_size, input_size, 1))
        sample_offset = np.zeros((1, 2))
        _ = model([sample_image, sample_offset])
    
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
            })
        elif layer_class == 'Conv2D':
            layers_dict[layer.name].update({
                "Type":         "conv",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else [],
                "in_channels":  idim,
                "out_channels": layer.filters,
                "kernel_size":  list(layer.kernel_size),
                "padding":      layer.padding,
                "weights":      export_part(layer, 'weights', 0),  # Kernel weights
                "bias":         export_part(layer, 'bias',    1) # initial value in the accumulator in gemm
            })
        elif layer_class == 'MaxPooling2D':
            layers_dict[layer.name].update({
                "Type":         "pool",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else [],
                "type":         "max",
                "kernel_size":  list(layer.pool_size),
                "stride":       list(layer.strides)
            })
        elif layer_class == 'Dense':
            layers_dict[layer.name].update({
                "Type":         "dense",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else [],
                "input_dim":    idim,
                "output_dim":   layer.units,
                "weights":      export_part(layer, 'weights', 0),
                "bias":         export_part(layer, 'bias', 1)
            })
        elif layer_class == 'Concatenate':
            layers_dict[layer.name].update({
                "Type":         "concatenate",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else [],
                "axis":         layer.axis
            })
        elif layer_class == 'ReLU':
            layers_dict[layer.name].update({
                "Type":         "relu",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else [],
                "threshold":    layer.threshold
            })
        elif layer_class == 'Flatten':
            layers_dict[layer.name].update({
                "Type":         "flatten",
                "inputs":        [],
                "tensor":        [ishape] if ishape is not None else []
            })
        else:
            print(f'ai: implement layer_class: {layer_class}')
        
    # Build a separate dictionary to track which layers output to which other layers
    # This won't be exported but is used to find terminal nodes
    output_connections = {layer_name: [] for layer_name in layers_dict.keys()}
    model_config = model.get_config()
    if 'layers' in model_config:
        for layer_config in model_config['layers']:
            layer_name = layer_config.get('name')
            if layer_name not in layers_dict:
                continue
            if 'inbound_nodes' in layer_config and layer_config['inbound_nodes']:
                for node in layer_config['inbound_nodes']:
                    for inbound_info in node:
                        if isinstance(inbound_info, list) and len(inbound_info) > 0:
                            source_layer = inbound_info[0]
                            if source_layer not in layers_dict[layer_name]["inputs"]:
                                layers_dict[layer_name]["inputs"].append(source_layer)
                            if source_layer in output_connections:
                                output_connections[source_layer].append(layer_name)
    
    # If the above doesn't work, try direct inspection
    if all(len(layer_info["inputs"]) == 0 for layer_info in layers_dict.values() if layer_info["Type"] != "input"):
        for i, layer in enumerate(model.layers):
            if i > 0:  # Skip the first layer (usually input)
                input_tensors = layer.input
                if isinstance(input_tensors, list):
                    for input_tensor in input_tensors:
                        for prev_layer in model.layers:
                            if prev_layer.output is input_tensor:
                                layers_dict[layer.name]["inputs"].append(prev_layer.name)
                                output_connections[prev_layer.name].append(layer.name)
                else:
                    for prev_layer in model.layers:
                        if prev_layer.output is input_tensors:
                            layers_dict[layer.name]["inputs"].append(prev_layer.name)
                            output_connections[prev_layer.name].append(layer.name)
    
    # If still no connections, use a last resort approach
    if all(len(layer_info["inputs"]) == 0 for layer_info in layers_dict.values() if layer_info["Type"] != "input"):
        for i in range(1, len(model.layers)):
            current_layer = model.layers[i]
            prev_layer    = model.layers[i - 1]
            layers_dict[current_layer.name]["inputs"].append(prev_layer.name)
            output_connections[prev_layer.name].append(current_layer.name)
    
    # Find terminal nodes (layers that don't output to any other layer)
    remaining_nodes = []
    for layer_name, outputs in output_connections.items():
        if len(outputs) == 0 and layer_name in layers_dict:
            remaining_nodes.append(layer_name)
    
    assert len(remaining_nodes), "endless loop"
    output_tensor = []
    for node in remaining_nodes:
        if node in output_shapes:
            output_tensor.append(output_shapes[node])
    layers_dict["output"] = {
        "name":         "output",
        "Type":         "output",
        "inputs":       remaining_nodes,
        "tensor":       output_tensor
    }
    ops        = list(layers_dict.values())
    model_json = {
        "ident":            mode,
        "quality":          quality,
        "input":            [[input_size, input_size, 1]] if mode == 'target' else [[input_size, input_size, 1], [2]],
        "output":           [[2]],
        "ops":              ops
    }

    # Save to JSON file
    with open(output_json_path, 'w') as f:
        json.dump(model_json, f, indent=4)

# ------------------------------------------------------------------
# main
# ------------------------------------------------------------------
repeat          = int(args.repeat)
GenType         = TargetDataGenerator if is_target else TrackDataGenerator
train_generator = GenType(train=True,  batch_size=batch_size, repeat=repeat)
val_generator   = GenType(train=False, batch_size=batch_size, repeat=1)
model           = train_generator.model()
model.summary()

train(model, train_generator, val_generator, num_epochs)