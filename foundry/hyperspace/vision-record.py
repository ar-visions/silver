import cv2
import os
import numpy as np
import random
import string
import time
from PIL import Image, ImageTk
import argparse
import sounddevice as sd
import tkinter as tk
import subprocess
import datetime
import signal
import atexit
import threading

# simple screen casting app.
# eventually we want the app to show off higher level features using ai along with the hyperspace sensor data.
# that may be AI model queries with attached screenshot a 'parascope' crop of the screen along with dictation


# --- audio setup ---
mic_level = 0
def audio_callback(indata, frames, time, status):
    global mic_level
    rms = np.sqrt(np.mean(np.square(indata)))
    mic_level = min(1.0, rms * 10)

# --- screen size ---
def get_screen_resolution():
    root = tk.Tk()
    width, height = root.winfo_screenwidth(), root.winfo_screenheight()
    root.destroy()
    return width, height

def update_center_offset():
    global center_x, center_y
    global offset_x, offset_y
    global offset_iter
    offset_iter += 1
    if offset_iter > 8:
        center_x    = random.randint(0,   screen_width)
        center_y    = random.randint(0,   screen_height)
        offset_iter = 0
    offset_x = random.randint(-screen_width  // 4, +screen_width // 4) # distribute in a 16:9 uniform
    offset_y = random.randint(-screen_height // 4, +screen_height // 4)
    
    # lets not prefer the edges, this will bias our accuracy to the bottom of the screen
    for i in range(8):
        if center_x + offset_x < 0 or center_x + offset_x > screen_width:
            offset_x = random.randint(-screen_width  // 4, +screen_width // 4)
        if center_y + offset_y < 0 or center_y + offset_y > screen_height:
            offset_y = random.randint(-screen_height  // 4, +screen_height // 4)
    
    # constrain offset_x to not go out of bounds
    if center_x + offset_x < 0: offset_x = -center_x
    if center_y + offset_y < 0: offset_y = -center_y
    if center_x + offset_x > screen_width:  offset_x = screen_width - center_x
    if center_y + offset_y > screen_height: offset_y = screen_height - center_y


def start_recording():
    global ffmpeg_process
    
    # Check if already recording
    if ffmpeg_process is not None:
        return
    
    # Create timestamp for filename
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    output_file = f"sessions/{session}/screen_{timestamp}.mp4"
    
    # Build ffmpeg command
    ffmpeg_cmd = [
        "ffmpeg",
        "-f", "x11grab",           # Screen capture input format
        "-framerate", "15",        # Lower frame rate for better performance
        "-video_size", f"{capture_width}x{capture_height}",  # Size of the capture area
        "-i", f":0.0+{capture_x},{capture_y}",  # Display and offset
        "-f", "pulse",             # Audio input format (Linux pulse audio)
        "-i", "default",           # Default audio device
        "-c:v", "libx264",         # Video codec
        "-preset", "ultrafast",    # Fastest encoding preset
        "-pix_fmt", "yuv420p",     # Required pixel format for compatibility
        "-c:a", "aac",             # Audio codec
        "-strict", "experimental", # Allow experimental codecs
        "-b:a", "128k",            # Audio bitrate
        "-y",                      # Overwrite output files without asking
        output_file
    ]
    
    # Start ffmpeg process
    ffmpeg_process = subprocess.Popen(
        ffmpeg_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    print(f"Started recording to {output_file}")
    return output_file

def stop_recording():
    global ffmpeg_process
    
    if ffmpeg_process is None:
        return
    
    # Gracefully terminate ffmpeg with 'q' command
    try:
        ffmpeg_process.communicate(input=b'q', timeout=50)
    except subprocess.TimeoutExpired:
        # If it doesn't respond to 'q', try to terminate it
        ffmpeg_process.terminate()
        try:
            ffmpeg_process.wait(timeout=50)
        except subprocess.TimeoutExpired:
            # If it still doesn't terminate, kill it
            ffmpeg_process.kill()
    
    ffmpeg_process = None
    print("Recording stopped")


# --- camera class ---
class Cam:
    def __init__(self, device_id):
        self.images = []
        self.bright = []
        self.cap = cv2.VideoCapture(device_id)
        self.device_id = device_id  # Store device_id for error messages
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, camera_width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, camera_height)
        if not self.cap.isOpened():
            raise Exception(f"could not open camera {device_id}")
    
    def read(self):
        ret, frame = self.cap.read()
        if not ret:
            print(f"error: could not read frame (camera {self.device_id})")
            exit(2)
        
        gray_image = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        current_brightness = np.mean(gray_image)

        # keep last 2 images
        self.images.insert(0, gray_image)
        self.bright.insert(0, current_brightness)
        if len(self.images) > 2:
            self.images.pop()
            self.bright.pop()

        # select brightest image
        best_index = np.argmax(self.bright)
        selected = self.images[best_index]

        # scale selected to pip size
        pip_frame = cv2.resize(selected, (pip_width, pip_height))
        return selected, pip_frame





def write(id, image):
    cen_x = center_x / screen_width
    cen_y = center_y / screen_height
    off_x = offset_x / screen_width
    off_y = offset_y / screen_height
    filename = f"sessions/{session}/f{id}_{session}-{frame_count:04d}_{cen_x + off_x:.4f}_{cen_y + off_y:.4f}.png"
    cv2.imwrite(filename, image, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    json = filename.replace(".png", ".json")
    with open(json, 'w') as f:
        f.write(f'{{"labels":{{"center":[{cen_x:.4f}, {cen_y:.4f}], "offset": [{off_x:.4f}, {off_y:.4f}]}}}}')
    print(f"saved: {filename}")


def write_in_thread(id, data):
    write(id, data)


# --- helper functions ---
def save_frame(id, img, cx, cy, ox, oy):
    fname = f"sessions/{session}/f{id}_{session}-{frame_count:04d}_{cx+ox:.4f}_{cy+oy:.4f}.png"
    cv2.imwrite(fname, img, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    json = fname.replace(".png", ".json")
    with open(json, 'w') as f:
        f.write(f'{{"labels":{{"center":[{cx:.4f},{cy:.4f}],"offset":[{ox:.4f},{oy:.4f}]}}}}')






# --- args ---
parser = argparse.ArgumentParser(description="Hyperspace Vision Recorder")
parser.add_argument("-s", "--session", required=True, help="session name")
parser.add_argument("-f", "--movie", action="store_true", help="movie preview mode")
args = parser.parse_args()

movie_mode = args.movie

session = args.session
screen_width, screen_height = get_screen_resolution()

# --- prepare session folder ---
os.makedirs(f'sessions/{session}', exist_ok=True)

# --- FFmpeg Screen Recording ---
ffmpeg_process = None

# --- config ---
camera_width, camera_height = 340, 340
pip_scale = 0.5
pip_width, pip_height = int(camera_width * pip_scale), int(camera_height * pip_scale)
pip_pad = 10

# Screen capture config for 720p in lower right
capture_width, capture_height = 1280, 720
# Position in lower right
capture_x = screen_width - capture_width
capture_y = screen_height - capture_height

window_width = pip_width
window_height = pip_height * 2 if movie_mode else screen_width
window_title = "hyperspace:record"

interval    = 4
recording   = False
last_space  = 0
center_x    = random.randint(0, screen_width)
center_y    = random.randint(0, screen_height)
offset_iter = 0
offset_x    = 0
offset_y    = 0
eye_x       = 0
eye_y       = 0
orbit_radius = 200  # Distance from the main circle
orbit_period = 5    # Time in seconds for a full orbit

# Ensure ffmpeg is stopped when program exits
atexit.register(stop_recording)

zone      = 3
zone_cm   = [ 10, 30, 50 ]
z_max     = 50
eye_x     = -1
eye_y     = -1
pcount    = 4
pindex    = 0
key       = 0
eyes_only = True


# --- UI ---
if movie_mode:
    root = tk.Tk()
    
    # Remove title bar but keep window
    root.overrideredirect(True)
    
    # Window position variables
    x_pos = pip_pad
    y_pos = pip_pad
    
    root.geometry(f"{pip_width}x{pip_height*2}+{x_pos}+{y_pos}")
    root.title(window_title)
    root.attributes("-topmost", True)
    
    # Add a frame for the whole window (will be used for dragging)
    frame = tk.Frame(root, bg="black")
    frame.pack(fill=tk.BOTH, expand=True)
    
    # Add label for displaying images
    label = tk.Label(frame)
    label.pack(fill=tk.BOTH, expand=True)
    
    # Add a record button
    def toggle_record():
        global recording
        recording = not recording
        if recording:
            start_recording()
            record_button.config(bg="red", text="Stop")
            print("Recording started")
        else:
            stop_recording()
            record_button.config(bg="green", text="Record")
            print("Recording stopped")
    
    if movie_mode:
        record_button = tk.Button(root, text="Record", bg="green", fg="white", 
                                command=toggle_record, 
                                font=("Arial", 10, "bold"))
        record_button.place(x=10, y=10, width=60, height=25)
    
    # Variables to track drag state
    drag_data = {"x": 0, "y": 0, "dragging": False}
    
    # Function to handle window dragging
    def on_drag_start(event):
        drag_data["x"] = event.x
        drag_data["y"] = event.y
        drag_data["dragging"] = True
    
    def on_drag_motion(event):
        if drag_data["dragging"]:
            # Calculate the distance moved
            dx = event.x - drag_data["x"]
            dy = event.y - drag_data["y"]
            
            # Get the current position of the window
            x = root.winfo_x() + dx
            y = root.winfo_y() + dy
            
            # Move the window
            root.geometry(f"+{x}+{y}")
    
    def on_drag_release(event):
        drag_data["dragging"] = False
    
    # Bind the frame to mouse events for dragging
    frame.bind("<ButtonPress-1>", on_drag_start)
    frame.bind("<B1-Motion>", on_drag_motion)
    frame.bind("<ButtonRelease-1>", on_drag_release)
    
    # Bind the label to mouse events for dragging
    label.bind("<ButtonPress-1>", on_drag_start)
    label.bind("<B1-Motion>", on_drag_motion)
    label.bind("<ButtonRelease-1>", on_drag_release)
    
else:
    cv2.namedWindow(window_title, cv2.WND_PROP_FULLSCREEN)
    cv2.setWindowProperty(window_title, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)






# --- audio stream (just for meter, not for recording) ---
sample_rate = 44100
#stream = sd.InputStream(callback=audio_callback, channels=1, samplerate=sample_rate, blocksize=2048)
#stream.start()

# --- cameras ---
cam2 = Cam(2)
cam6 = Cam(6)

# --- main loop ---
frame_count = 0
recording = False
center_x, center_y = random.randint(0,screen_width), random.randint(0,screen_height)
offset_x, offset_y = 0, 0

update_center_offset()

while True:
    # capture frames
    img2, img2_pip = cam2.read()
    img6, img6_pip = cam6.read()

    if movie_mode:
        img2 = cv2.resize(img2, (pip_width, pip_height))
        img6 = cv2.resize(img6, (pip_width, pip_height))

        merged = np.zeros((pip_height*2, pip_width), dtype=np.uint8)
        merged[0:pip_height, :] = img2_pip
        merged[pip_height:pip_height*2, :] = img6_pip

        imgtk = ImageTk.PhotoImage(image=Image.fromarray(merged))
        label.imgtk = imgtk12
        label.configure(image=imgtk)
        root.update()
        
        # For tkinter window, check for escape key
        root.bind('<Escape>', lambda e: root.destroy())
        
        # Key binding removed - using button instead
        
    else:
        bg = np.full((screen_height, screen_width), 32, dtype=np.uint8)
        bg[0:pip_height, 0:pip_width] = img2_pip
        bg[pip_height:pip_height*2, 0:pip_width] = img6_pip

        cv2.circle(bg, (center_x, center_y), 32, 160, 1, lineType=cv2.LINE_AA)  # -1 thickness fills the circle
        cv2.circle(bg, (center_x + offset_x, center_y + offset_y), 4, 255, 1, lineType=cv2.LINE_AA)  # -1 thickness fills the circle

        cv2.imshow(window_title, bg)

    frame_count += 1
    key          = cv2.waitKey(10) & 0xFF
    current_ticks = int(time.time() * 1000)  # Current time in milliseconds
    
    if not movie_mode:
        key = cv2.waitKey(10) & 0xFF

        # Handle spacebar with cooldown
        if key == 32 and (current_ticks - last_space > 200):
            #recording = not recording
            last_space = current_ticks
            if not recording:
                print('writing image')
                write(2, img2)
                write(6, img6)
                update_center_offset()


        if key == 27:  # Escape key
            break
        elif key == ord('r'):  # Press 'r' to toggle recording
            recording = not recording
            if recording:
                start_recording()
                print("Recording started")
            else:
                stop_recording()
                print("Recording stopped")
    else:
        # Short delay to prevent CPU hogging
        time.sleep(0.01)

        if key == 27: break # esc

        # Check if window was closed
        try:
            root.update()
        except tk.TclError:
            break


# Ensure recording is stopped before exit
if recording:
    stop_recording()

if not movie_mode:
    cv2.destroyAllWindows()
#stream.stop()