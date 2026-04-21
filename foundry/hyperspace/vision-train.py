#!/bin/env python3
import tensorflow as tf
import tensorflow.keras as keras
from   keras              import KerasTensor
from   keras              import layers
from   keras.layers       import InputLayer
from   keras.regularizers import l2
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import os
import vision as vision
from   vision import models, run_base, run_target, run_track, target_vary, mix
import math
import copy
import numpy as np
import cv2
import time
import json
import random
import argparse
from   collections import OrderedDict
from   keras.constraints import Constraint
import keras.backend as K
from tensorflow.keras.models import Model

# ------------------------------------------------------------------
# hyper-parameters and mode selection
# ------------------------------------------------------------------
parser        = argparse.ArgumentParser(description="Annotate images with click-based labels.")
parser.add_argument("-s", "--session", required=True,   help="session data (name of session; as named in cwd/sessions dir)")
parser.add_argument("-m", "--mode",    default='track', help='model-type to create; types = target (to obtain eye-center) and track (to infer screen pos, with eye center as paramter in input model)')
parser.add_argument("-r", "--repeat",  default=1)
parser.add_argument("-e", "--epochs",  default=500)
parser.add_argument("-i", "--input",   default=64)
parser.add_argument("-b", "--batch",   default=1)
parser.add_argument("-lr", "--learning-rate", default=0.0001)
args          = parser.parse_args()
session_ids   = args.session.split(',')
resize        = int(args.input)   # track: crop (128x128) or entire image (340x340) is resized to 64x64
input_size    = resize
size          = 128          # crop size for track mode
full_size     = 340          # original full image size
batch_size    = int(args.batch)
learning_rate = float(args.learning_rate)
num_epochs    = int(args.epochs)
mode          = args.mode


# test layers, this sums the result so we may check against our own implementation
image_path = 'res/images/resized.png'
image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
model = tf.keras.models.load_model('res/models/vision_simple.keras') 
image = image.astype(np.float32) / 255.0
image = np.expand_dims(image, axis=[0, -1])  # Shape: (1, 32, 32, 1)
output = model(image, training=False).numpy()

# Print output
print("vision simple:", output)

def required_fields(mode):
    if mode == 'simple':  return ['eye_left', 'eye_right', 'scale']
    if mode == 'base':    return ['eye_left', 'eye_right', 'scale']
    if mode == 'target':  return ['eye_left', 'eye_right', 'scale']
    if mode == 'eyes':    return ['eye_left', 'eye_right']
    if mode == 'track':   return ['center', 'offset'] # we need only our truth labeling from the recorder
    if mode == 'refine':  return ['iris_mid']
    return ['eye_left', 'eye_right']

# scale is tedious to plot, so we train based on a variance of images to get this number
# this scale is fed into target training and should likely be used in track as well
class base(vision.model):
    def __init__(self, data, batch_size):
        self.mode          = 'base'
        self.data          = data
        self.batch_size    = batch_size

    # make sure teh annotator can help us fill in annotations for images that have annotations on other camera ids.
    # thats a simple text replacement and check filter
    def label(self, data, i):
        # this model is 1 image at a time, but our batching is setup for the model of all camera annotations
        # so its easiest to set a repeat count of > 4 and perform a random selection
        image_path =  data.f2_image     [i]
        iris_mid   =  data.f2_iris_mid  [i].copy()
        scale      = [data.f2_scale     [i].copy()[0]]

        if random.random() < 0.5:
            image_path = data.f6_image     [i]
            iris_mid   = data.f6_iris_mid  [i].copy()
            scale      = [data.f6_scale     [i].copy()[0]]
        
        image   = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        targets = [iris_mid]
        #image2  = image.copy()
        if random.random() < 0.5:
            image = cv2.flip(image, 1)
            for t in targets:
                t[0] = -t[0]  # Flip x-coordinate
        image   = cv2.resize(image, (input_size, input_size))
        image   = image / 255.0
        image   = brightness_contrast(image, random.uniform(0.5, 1.5), random.uniform(-0.15, 0.15)) if data.train else image
        image = np.expand_dims(image, axis=-1)
        return {'image': image }, [targets[0][0], targets[0][1], scale[0]]
    
    def model(self):
        image = keras.layers.Input(shape=(input_size, input_size, 1), name='image')
        x = keras.layers.Conv2D       (name='conv0', filters=32, activation='relu', strides=(1,1), kernel_size=3, padding="valid")(image)
        x = keras.layers.MaxPooling2D (name='pool0', pool_size=(2,2))(x)
        x = keras.layers.Conv2D       (name='conv1', filters=64, activation='relu', strides=(1,1), kernel_size=3, padding="valid")(x)
        x = keras.layers.MaxPooling2D (name='pool1', pool_size=(2,2))(x)
        x = keras.layers.Flatten      (name='flatten')(x)
        x = keras.layers.Dense        (name='dense0', units=8, activation='relu')(x)
        x = keras.layers.Dense        (name='dense1', units=3, activation='linear')(x)
        return keras.Model            (name=mode, inputs=image, outputs=x)


# scale is tedious to plot, so we train based on a variance of images to get this number
# this scale is fed into target training and should likely be used in track as well
class simple(vision.model):
    def __init__(self, data, batch_size):
        self.mode          = 'base'
        self.data          = data
        self.batch_size    = batch_size

    # make sure teh annotator can help us fill in annotations for images that have annotations on other camera ids.
    # thats a simple text replacement and check filter
    def label(self, data, i):
        # this model is 1 image at a time, but our batching is setup for the model of all camera annotations
        # so its easiest to set a repeat count of > 4 and perform a random selection
        image_path =  data.f2_image     [i]
        iris_mid   =  data.f2_iris_mid  [i].copy()
        scale      = [data.f2_scale     [i].copy()[0]]

        if random.random() < 0.5:
            image_path = data.f6_image     [i]
            iris_mid   = data.f6_iris_mid  [i].copy()
            scale      = [data.f6_scale     [i].copy()[0]]
        
        image   = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        targets = [iris_mid]
        #image2  = image.copy()
        if random.random() < 0.5:
            image = cv2.flip(image, 1)
            for t in targets:
                t[0] = -t[0]  # Flip x-coordinate
        image   = cv2.resize(image, (input_size, input_size))
        image   = image / 255.0
        image   = brightness_contrast(image, random.uniform(0.5, 1.5), random.uniform(-0.15, 0.15)) if data.train else image
        image = np.expand_dims(image, axis=-1)
        return {'image': image }, [targets[0][0], targets[0][1], scale[0]]
    
    def model(self):
        image = keras.layers.Input(shape=(input_size, input_size, 1), name='image')
        x = keras.layers.Conv2D       (name='conv0', filters=32, activation='relu', strides=(1,1), kernel_size=3, padding="same")(image)
        x = keras.layers.MaxPooling2D (name='pool0', pool_size=(2,2))(x)
        x = keras.layers.Flatten      (name='flatten')(x)
        x = keras.layers.Dense        (name='dense0', units=3, activation='linear')(x)
        return keras.Model            (name=mode, inputs=image, outputs=x)


# annotator could let the user use hte wsad keys and space, to move eye positions to correct spots
# this way we could pre-annotate the image and the user may adjust the plots
# this is an obvious feature to add for quicker, more exact annotations

# we want to target where we get inference from with base
class target(base):
    def __init__(self, data, batch_size):
        self.mode          = 'target'
        self.data          = data
        self.batch_size    = batch_size

    # make sure teh annotator can help us fill in annotations for images that have annotations on other camera ids.
    # thats a simple text replacement and check filter
    def label(self, data, i):
        # this model is 1 image at a time, but our batching is setup for the model of all camera annotations
        # so its easiest to set a repeat count of > 4 and perform a random selection
        image_path = data.f2_image     [i]
        eye_left   = data.f2_eye_left  [i]
        eye_right  = data.f2_eye_right [i]
        iris_mid   = data.f2_iris_mid  [i].copy()
        scale      = data.f2_scale     [i].copy()

        test = 2
        if image_path == 'sessions/center-offset/f2_center-offset-6003_0.6840_0.2597.png':
            test += 2

        if random.random() < 0.5:
            image_path = data.f6_image     [i]
            eye_left   = data.f6_eye_left  [i]
            eye_right  = data.f6_eye_right [i]
            iris_mid   = data.f6_iris_mid  [i].copy()
            scale      = data.f6_scale     [i].copy()
        
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)

        # Get model input shape (assumes model takes single-channel grayscale)
        input_shape = models['base'].input_shape  # (None, height, width, channels)
        input_height, input_width = input_shape[1:3]

        # Resize cropped region back to model input size
        final_input = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_AREA)

        # Normalize (convert to float32 and scale between 0 and 1)
        final_input = final_input.astype(np.float32) / 255.0
        final_input = np.expand_dims(final_input, axis=[0,-1])  # Add batch & channel dim

        # run inference on base model
        predictions = models['base'](final_input, training=False)
        predictions = tf.convert_to_tensor(predictions).numpy()
        
        # extract iris_mid (x, y) and scale from predictions
        pred_iris_x, pred_iris_y, pred_scale = predictions[0]
        #pred_scale = 0.5
        scale       = [pred_scale]
        scale_min   = (scale[0] * 0.8)
        scale_max   = (scale[0] * 1.2)

        # we may filter in some way if the iris_mid distance is too muc
        # probably better to stage this outside of batching, too.
        image, targets, sc, mirror, degs = target_vary(image, iris_mid, [iris_mid], 0.22, scale_min, scale_max, True, 22)

        #vision.show(image, targets[0], 'target-training')
        #image2 = image.copy()
        #targets = [iris_mid]
        #new_w = full_size
        image    = cv2.resize(image, (input_size, input_size))
        image    = image / 255.0
        image    = np.expand_dims(image, axis=-1)
        return {'image': image }, np.array([targets[0][0], targets[0][1], sc / scale[0]])

    def model(self):
        image = keras.layers.Input(shape=(input_size, input_size, 1), name='image')
        x = keras.layers.Conv2D       (name='conv0', filters=32, activation='relu', strides=(1,1), kernel_size=5, padding="same")(image)
        x = keras.layers.MaxPooling2D (name='pool0', pool_size=(2,2))(x)
        x = keras.layers.Flatten      (name='flatten')(x)
        x = keras.layers.Dense        (name='dense0', units=32, activation='relu')(x)
        x = keras.layers.Dense        (name='dense1', units=3, activation='tanh')(x)
        return keras.Model            (name=mode, inputs=image, outputs=x)

def brightness_contrast(f, alpha=1.0, beta=0):
    new_image = np.clip(alpha * f + beta, 0, 1.0)
    return new_image

# we are defining target area as 80% scale; our scale was a radius we could make a pattern of on the face, but not exactly best rectangle size for target
# we want to target where we get inference from with base
class eyes(target):
    def __init__(self, data, batch_size):
        self.mode          = 'target'
        self.data          = data
        self.batch_size    = batch_size

    # make sure teh annotator can help us fill in annotations for images that have annotations on other camera ids.
    # thats a simple text replacement and check filter
    def label(self, data, i):
        # this model is 1 image at a time, but our batching is setup for the model of all camera annotations
        # so its easiest to set a repeat count of > 4 and perform a random selection
        image_path = data.f2_image     [i]
        eye_left   = data.f2_eye_left  [i]
        eye_right  = data.f2_eye_right [i]
        iris_mid   = data.f2_iris_mid  [i].copy()
        scale      = data.f2_scale     [i].copy() if data.f2_scale[i] is not None else None

        if random.random() < 0.5:
            image_path = data.f6_image     [i]
            eye_left   = data.f6_eye_left  [i]
            eye_right  = data.f6_eye_right [i]
            iris_mid   = data.f6_iris_mid  [i].copy()
            scale      = data.f6_scale     [i].copy() if data.f6_scale[i] is not None else None
        
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)

        # Get model input shape (assumes model takes single-channel grayscale)
        input_shape = models['base'].input_shape  # (None, height, width, channels)
        input_height, input_width = input_shape[1:3]
        final_input = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_AREA)
        final_input = final_input / 255.0
        # lets train over a huge range of IR exposure; this is essentially the same exact effect as going through that domain
        final_input = np.expand_dims(final_input, axis=[0,-1])  # Add batch & channel dim
        if scale is None:
            predictions = models['base'](final_input, training=False)
            pred_iris_x, pred_iris_y, pred_scale = predictions[0]
            scale       = [pred_scale]

        # accuracy of scale should directly impact accuracy of left and right eyes
        scale_min   = (scale[0] * 0.8 * 1.1)
        scale_max   = (scale[0] * 0.8 * 0.9) # we need to run against a target model, too: lets first test with base and see if it improves with target
        scale_normal   = (scale[0] * 0.8)
        iris_mid = [(eye_right[0] + eye_left[0]) / 2, (eye_right[1] + eye_left[1]) / 2]
        t = data.train
        image, targets, sc, mirror, degs = vision.target_vary(
            image, vision.mix(eye_right, eye_left),
            [iris_mid, eye_right, eye_left],
            0.1 if t else 0,
            scale_min if t else scale_normal,
            scale_max if t else scale_normal,
            True, 22 if t else 0)
        scale[0]    = (scale[0] * 0.8) / sc
        image_full  = image
        image       = cv2.resize(image, (input_size, input_size))
        image       = image / 255.0
        image       = brightness_contrast(image, random.uniform(0.5, 1.5), random.uniform(-0.15, 0.15)) if data.train else image
        #vision.show(image)
        image       = np.expand_dims(image, axis=-1)
        i1          = 2 if mirror else 1
        i2          = 1 if mirror else 2
        #vision.show(image_full, [targets[i1], targets[i2]], 'eye')
        return {'image': image }, np.array([
            targets[i1][0], targets[i1][1],
            targets[i2][0], targets[i2][1],
            scale[0]
        ])
    
    def model(self):
        image = keras.layers.Input(shape=(input_size, input_size, 1), name='image')
        x = keras.layers.Conv2D       (name='conv0', filters=32, activation='relu', strides=(1,1), kernel_size=5, padding="same")(image)
        x = keras.layers.MaxPooling2D (name='pool0', pool_size=(2, 2))(x)
        x = keras.layers.Flatten      (name='flatten')(x)
        x = keras.layers.Dense        (name='dense0', units=64, activation='relu')(x)
        x = keras.layers.Dense        (name='dense1', units=5, activation='linear')(x)
        return keras.Model            (name=mode, inputs=image, outputs=x)


class track(base):
    def __init__(self, data, batch_size):
        self.mode       = 'base'
        self.data       = data
        self.batch_size = batch_size

    def label(self, data, index):
        # perform all augmentation here
        f2_image_path = data.f2_image[index]
        f6_image_path = data.f6_image[index]
        
        center = data.center[index].copy()
        offset = data.offset[index].copy()

        # we need to forward pass the images f2_image_path and f6_image_path
        # to find the iris-mid [base]
        # process f2
        #print(f2_image_path)
        f2_image = cv2.imread(f2_image_path, cv2.IMREAD_GRAYSCALE)
        f2_iris_mid, f2_scale = run_base(f2_image, data.f2_iris_mid[index], data.f2_scale[index]) 

        if not data.f2_iris_mid[index] or not data.f2_scale[index]:
            f2_iris_mid,  f2_scale, f2_target_image, f2_mid_offset    = vision.run_target(f2_image, f2_iris_mid, f2_scale) 

        if not data.f2_eye_left[index] or not data.f2_eye_right[index]:
            f2_eye_right, f2_eye_left, f2_scale, f2_eyes_image, f2_eyes_right_raw = vision.run_eyes(f2_image, f2_iris_mid, f2_scale)

        else:
            f2_eye_right = data.f2_eye_right[index]
            f2_eye_left  = data.f2_eye_left[index]

        f2_iris_mid = mix(f2_eye_right, f2_eye_left)

        # process f6
        f6_image = cv2.imread(f6_image_path, cv2.IMREAD_GRAYSCALE)
        f6_iris_mid, f6_scale = run_base(f6_image, data.f6_iris_mid[index], data.f6_scale[index])

        if not data.f6_iris_mid[index] or not data.f6_scale[index]:
            f6_iris_mid,  f6_scale, f6_target_image, f6_target_mid_raw = vision.run_target(f6_image, f6_iris_mid, f6_scale)
        if not data.f6_eye_left[index] or not data.f6_eye_right[index]:
            f6_eye_right, f6_eye_left, f6_scale, f6_eyes_image, f6_eyes_right_raw  = vision.run_eyes(f6_image, f6_iris_mid, f6_scale) 
        else:
            f6_eye_right = data.f6_eye_right[index]
            f6_eye_left  = data.f6_eye_left[index]
        
        # from eyes and on, we track based on the average of the two eyes; so they vary less
        f6_iris_mid = mix(f6_eye_right, f6_eye_left)
        rx = 0 # random.uniform(-0.2, 0.2)
        ry = 0 # random.uniform(-0.2, 0.2)
        if center[0] + offset[0] + rx > 0.5 or center[0] + offset[0] + rx < -0.5: rx = 0
        if center[1] + offset[1] + ry > 0.5 or center[1] + offset[1] + ry < -0.5: ry = 0

        # we do not vary the crop location, but rather vary the iris-mid's location and resulting pixel location
        def process_image(image_path, scale, f, target):
            full_image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
            if scale is None:
                global scales
                if image_path in scales:
                    scale = scales[image_path]
                else:
                    image_copy  = (full_image.copy() / 255.0).astype(np.float32)
                    image_copy  = cv2.resize(image_copy, (32, 32), interpolation=cv2.INTER_AREA)
                    image_copy  = np.expand_dims(image_copy, axis=[0,-1])
                    predictions = models['base'](image_copy, training=False)
                    pred_iris_x, pred_iris_y, pred_scale = predictions[0]
                    scale = pred_scale
                    scales[image_path] = scale
            
            vary      = 0.00
            scale_min = (scale / f * (1.0 - vary))
            scale_max = (scale / f * (1.0 + vary))
            t         = copy.deepcopy(target)
            image, targets, sc, mirror, degs = target_vary(full_image, t[0], t, 0.0, scale_min, scale_max, False, 0)
            image     = (image / 255.0).astype(np.float32)
            image     = cv2.resize(image, (input_size, input_size), interpolation=cv2.INTER_AREA)
            image     = np.expand_dims(image, axis=-1)
            scale     = scale / sc
            return image, targets, scale, full_image
        
        # we may mirror, but we would have to do so here at the return
        f2_image, _, f2_image_scale, _ = process_image(f2_image_path, f2_scale, 1.0, [f2_iris_mid ])
        f2_left,  _, f2_left_scale,  _ = process_image(f2_image_path, f2_scale, 3.0, [f2_eye_left ])
        f2_right, _, f2_right_scale, _ = process_image(f2_image_path, f2_scale, 3.0, [f2_eye_right])

        f6_image, _, f6_image_scale, _ = process_image(f6_image_path, f6_scale, 1.0, [f6_iris_mid ])
        f6_left,  _, f6_left_scale,  _ = process_image(f6_image_path, f6_scale, 3.0, [f6_eye_left ])
        f6_right, _, f6_right_scale, _ = process_image(f6_image_path, f6_scale, 3.0, [f6_eye_right])

        alpha    = 1.0 # random.uniform( 0.8, 1.2)
        beta     = 0.0 # random.uniform(-0.2, 0.2)
        t        = data.train
        #t = True
        f2_image = brightness_contrast(f2_image, alpha, beta)                   if t else f2_image
        f6_image = brightness_contrast(f6_image, alpha, beta)                   if t else f6_image
        f2_left  = brightness_contrast(f2_left,  (alpha + 1.0) / 2, beta * 0.5) if t else f2_left
        f6_left  = brightness_contrast(f6_left,  (alpha + 1.0) / 2, beta * 0.5) if t else f6_left
        f2_right = brightness_contrast(f2_right, (alpha + 1.0) / 2, beta * 0.5) if t else f2_right
        f6_right = brightness_contrast(f6_right, (alpha + 1.0) / 2, beta * 0.5) if t else f6_right

        # we want to STORE this info so taht the next epoch, we dont have to call process_image OR run the inferences above!
        # data to store for this index id
        pixel_scale    = 1.0
        f2_image_aug_scale = f2_scale / 0.5
        f6_image_aug_scale = f6_scale / 0.5

        def aug_position(pos, x, y): return [pos[0] + x, pos[1] + y]
        
        if True or random.random() > 0.5:
            channels = vision.channels([f2_image, f6_image, f2_left, f6_left, f2_right, f6_right])
            #concatenated_image = np.hstack([channels[:, :, i] for i in range(6)])
            # Display the concatenated image
            #plt.figure(figsize=(15, 5))
            #plt.imshow(concatenated_image, cmap='gray')
            #plt.title("Stacked Image Channels")
            #plt.axis('off')
            #plt.show(block=True)
            return {
                'channels':     channels,
                'f2_eye_left':  aug_position(f2_eye_left,  rx * f2_image_aug_scale,  ry * f2_image_aug_scale), # we are looking at the user, the user looks at the screen, should the x direction be swapped when translating in image space vs gaze to screen space?
                'f6_eye_left':  aug_position(f6_eye_left,  rx * f6_image_aug_scale,  ry * f6_image_aug_scale),
                'f2_eye_right': aug_position(f2_eye_right, rx * f2_image_aug_scale,  ry * f2_image_aug_scale),
                'f6_eye_right': aug_position(f6_eye_right, rx * f6_image_aug_scale,  ry * f6_image_aug_scale)
            }, [center[0] + rx * pixel_scale, center[1] + ry * pixel_scale, offset[0], offset[1]]
        else:
            def mirror_image(image):  return np.expand_dims(cv2.flip(np.squeeze(image), 1), axis=-1)  # Flip along the vertical axis
            def mirror_position(pos): return [-pos[0], pos[1]]   # Invert X while keeping Y the same
            channels = vision.channels([
                mirror_image(f2_image),
                mirror_image(f6_image),
                mirror_image(f2_right),
                mirror_image(f6_right),
                mirror_image(f2_left),
                mirror_image(f6_left)])
            return {'channels':     channels,
                    'f2_eye_left':  mirror_position(aug_position(f2_eye_right,  rx * f2_image_aug_scale,  ry * f2_image_aug_scale)),
                    'f6_eye_left':  mirror_position(aug_position(f6_eye_right,  rx * f6_image_aug_scale,  ry * f6_image_aug_scale)),
                    'f2_eye_right': mirror_position(aug_position(f2_eye_left,   rx * f2_image_aug_scale,  ry * f2_image_aug_scale)),
                    'f6_eye_right': mirror_position(aug_position(f6_eye_left,   rx * f6_image_aug_scale,  ry * f6_image_aug_scale)),
            }, [-center[0] + rx * -pixel_scale,
                                  center[1] + ry *  pixel_scale,
                                 -offset[0],
                                  offset[1]]
        
    def model(self):
        channels     = keras.layers.Input(name='channels',     shape=(input_size, input_size, 6))
        f2_eye_left    = keras.layers.Input(name='f2_eye_left',  shape=(2,))
        f6_eye_left    = keras.layers.Input(name='f6_eye_left',  shape=(2,))
        f2_eye_right   = keras.layers.Input(name='f2_eye_right', shape=(2,))
        f6_eye_right   = keras.layers.Input(name='f6_eye_right', shape=(2,))

        #x = keras.layers.Conv2D       (name='conv0', filters=32, activation='relu', strides=(1,1), kernel_size=3, padding="same")(channels)
        x = keras.layers.DepthwiseConv2D(name='conv0', depth_multiplier=64, activation='relu', strides=(1,1), kernel_size=5, padding="same")(channels)
        x = keras.layers.MaxPooling2D (name='pool1', pool_size=(2,2))(x)
        x = keras.layers.Flatten      (name='flatten')(x)
        x = keras.layers.Concatenate  (name='concat', axis=-1)([x, f2_eye_left, f6_eye_left, f2_eye_right, f6_eye_right])
        x = keras.layers.Dense        (name='dense0', units=64, activation='relu')(x)
        x = keras.layers.Dense        (name='dense1', units=16, activation='relu')(x)
        x = keras.layers.Dense        (name='dense2', units=4, activation='linear')(x)

        return keras.Model(name="track", inputs=[
            channels, f2_eye_left, f6_eye_left, f2_eye_right, f6_eye_right
        ], outputs=x)



# ------------------------------------------------------------------
# main
# ------------------------------------------------------------------
repeat  = int(args.repeat)
gen     = globals().get(mode)
assert gen != None, "unresolved model"
data    = vision.data(mode, session_ids, required_fields(mode), validation_split=0.1, final_magnitude=2, repeat=repeat)
train   = gen(data=data.train,      batch_size=batch_size)
final   = gen(data=data.final,      batch_size=batch_size)
val     = gen(data=data.validation, batch_size=batch_size)

vision.train(train, val, final, learning_rate, num_epochs)