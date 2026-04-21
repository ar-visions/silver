#!/bin/env python3
import os
import json
import random
import cv2
import numpy as np
import argparse
import math
from   pathlib import Path

parser = argparse.ArgumentParser(description="Annotate images with click-based labels.")
parser.add_argument("-s", "--session",   required=True, help="session data (name of session; as named in cwd/sessions dir)")
parser.add_argument("-a", "--attribute", required=True, help="eye-center label (between eyes on nose bridge)")
parser.add_argument("-r", "--review",    action="store_true", help="review and edit annotations already made")
parser.add_argument("-f", "--filter",    default='', type=str, help="only show files with this annotation (applies to ALL camera space!)")
parser.add_argument("-n", "--n",         action="store_true", help="apply not to filter (only show without)")

args         = parser.parse_args()
a_name       = args.attribute
session      = args.session
review       = args.review
IMAGE_DIR    = "sessions/" + session
dialog       = {}
start_x      = 0
start_y      = 0
canvas_w     = 0
canvas_h     = 0
filter       = args.filter
is_not       = args.n
is_scale     = a_name == 'scale'
mouse_x      = 0
mouse_y      = 0

def normalize_click(x, y, img_w, img_h):
    return [(x - img_w / 2) / img_w, (y - img_h / 2) / img_h]

def json_path(img_path): return os.path.splitext(img_path)[0] + ".json"

def set_annot(img_path, name, object):
    path = json_path(img_path)
    if os.path.exists(path):
        with open(path, "r") as f:
            json_data = json.load(f)
    else:
        json_data = {"labels": {}}
    json_data['labels'][name] = object
    with open(path, "w") as f: json.dump(json_data, f, indent=4)

def get_annot(img_path, name, any_camera=False):
    path = json_path(img_path)
    if not os.path.exists(path):
        if any_camera:
            p = Path(path)
            s = p.stem[:3]  # Extract first 3 characters (e.g., "f1_")
            r = "f6_" if s != "f6_" else "f2_"  # Swap prefix
            path = str(p.with_name(r + p.stem[3:] + p.suffix))
            
    if os.path.exists(path):
        with open(path, "r") as f:
            json_data = json.load(f)
        return json_data['labels'][name] if name in json_data['labels'] else None
    return None

scale_save = None

def on_click(event, x, y, flags, param):
    img_path, img_w, img_h = param
    if event == cv2.EVENT_MOUSEMOVE:
        global mouse_x, mouse_y
        mouse_x, mouse_y = x, y
    if event == cv2.EVENT_LBUTTONDOWN:
        global scale_save
        if scale_save:
            set_annot(img_path, a_name, [round(scale_save, 4)])
        else:
            norm_x, norm_y = normalize_click(x - start_x, y - start_y, img_w, img_h)
            set_annot(img_path, a_name, [round(norm_x, 4), round(norm_y, 4)])
        dialog['saved'] = True

def process_images(image_dir):
    files  = sorted(os.listdir(image_dir))
    index  = 0
    images = []
    filters = []
    eyes = []
    random.shuffle(files)
    for filename in files:
        if filename.lower().endswith((".png")):
            img_path  = os.path.join(image_dir, filename)
            #if Path(img_path).stem == 'f6_center-close-10720_0.8977_0.3910':
            #    img_path = img_path
            json_path = os.path.splitext(img_path)[0] + ".json"
            #exists    = os.path.exists(json_path)
            #if not exists and (filter != is_not): continue
            eye_left  = None
            eye_right = None
            if is_scale:
                eye_left  = get_annot(img_path, 'eye-left',  False)
                eye_right = get_annot(img_path, 'eye-right', False)
                if not eye_left or not eye_right:
                    continue
            
            filter_data = get_annot(img_path, filter, True) # useful feature to check all cameras; this lets us get all annotations for the pairs
            if filter and ((not filter_data) != is_not):
                print(f'skipping, has annotation for {filter}')
                continue
            has_annot = get_annot(img_path, a_name)
            if has_annot and not review:
                continue
            images.append(img_path)
            filters.append(filter_data)
            eyes.append([eye_right, eye_left])
    
    while index < len(images):
        img_path  = images[index]
        img       = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
        img       = np.expand_dims(img, axis=-1)
        img_h, img_w = img.shape[:2]
        global dialog, start_x, start_y, canvas_w, canvas_h
        dialog    = {}
        canvas_w  = img_w * 2
        canvas_h  = img_h * 2
        canvas    = np.zeros((canvas_h, canvas_w, 1), dtype=np.uint8)
        start_x   = (canvas_w - img_w) // 2
        start_y   = (canvas_h - img_h) // 2
        canvas[start_y:start_y + img_h, start_x:start_x + img_w] = img
        print(f"showing: {img_path} - annotating {a_name}")
        title     = f'hyperspace:annotate - {a_name}'


        cp = canvas.copy()
        cv2.imshow(title, canvas)
        cv2.setMouseCallback(title, on_click, (img_path, img_w, img_h))

        while True:

            if is_scale:
                # Draw the orbiting circle
                # filter_data <-- is filled out 
                rl = eyes[index]
                assert rl, "missing eyes data (required for plotting target depth)"
                center = [(rl[0][0] + rl[1][0]) / 2, (rl[0][1] + rl[1][1]) / 2]
                x = start_x + int(img_w / 2 + center[0] * img_w)
                y = start_y + int(img_h / 2 + center[1] * img_h)
                r = int(math.sqrt((mouse_x - x) ** 2 + (mouse_y - y) ** 2)) // 2
                global scale_save
                scale_save = r / img_w * 2
                canvas = cp.copy()
                cv2.circle(canvas, (x, y), r, 255, 1)
                cv2.imshow(title, canvas)
                
            key = cv2.waitKey(20) & 0xFF
            if "saved" in dialog:
                cv2.setMouseCallback(title, lambda *args: None)
                break
            else:
                if key == 27:
                    return
                elif key == ord('d') or key == 83:  # Next
                    cv2.setMouseCallback(title, lambda *args: None)
                    break
                elif key == ord('a') or key == 81:  # Prev
                    cv2.setMouseCallback(title, lambda *args: None)
                    index -= 2
                    break
        
        cv2.destroyAllWindows()
        cv2.waitKey(10)
        index = max(index + 1, 0)

process_images(IMAGE_DIR)
cv2.destroyAllWindows()