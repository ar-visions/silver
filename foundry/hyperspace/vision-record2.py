#!/usr/bin/env python3
# pip install torch torchvision opencv-python numpy requests pillow
# ----------------------------------------------------------
import  cv2
import  os
import  numpy as np
import  requests
import  random
import  string
import  time
from    io          import BytesIO
from    PIL         import Image
from    tkinter     import Tk
#from    playsound   import playsound
import  argparse
import  sounddevice as sd
import  glob
import  subprocess
import  pyfakewebcam
import  threading
import  mss


def ensure_hyperspace_loopback():
    # 1. check if /dev/video10 exists
    if os.path.exists("/dev/video10"):
        # 2. verify the name is "hyperspace"
        name_path = "/sys/class/video4linux/video10/name"
        if os.path.exists(name_path):
            with open(name_path) as f:
                if f.read().strip().lower() == "hyperspace":
                    return "/dev/video10"

    # if here → device missing OR wrong name → create it
    print("loading v4l2loopback for hyperspace...")
    subprocess.run([
        "sudo", "modprobe", "v4l2loopback",
        "devices=1",
        "video_nr=10",
        "card_label=hyperspace",
        "exclusive_caps=1"
    ])

    # give the kernel a moment
    for _ in range(20):
        time.sleep(0.1)
        if os.path.exists("/dev/video10"):
            return "/dev/video10"

    raise RuntimeError("failed to create /dev/video10 hyperspace device")





def audio_callback(indata, frames, time, status):
    if status:
        print(status, flush=True)
    rms = np.sqrt(np.mean(np.square(indata)))
    global mic_level
    mic_level = min(1.0, rms * 10)  # Scale it to 0 - 1


parser = argparse.ArgumentParser(description="Annotate images with click-based labels.")
parser.add_argument("-s", "--session",     required=True, help="session data (name of session; as named in cwd/sessions dir)")
parser.add_argument("-f", "--debug",       action="store_true", help="debug mode (cannot record)")
parser.add_argument(
    "-m", "--mode",
    choices=["annotation", "stream"],
    default="annotation",
    help="run in annotation mode (UI) or stream mode (headless camera mode)"
)

root          = Tk()
screen_width  = root.winfo_screenwidth()
screen_height = root.winfo_screenheight()
root.destroy()


def random_bg_color():
    g = 32 # random.randint(0, 64)
    global screen_width
    global screen_height
    return np.full((screen_height, screen_width), g, dtype=np.uint8)

def generate_hash_id():
    return ''.join(random.choices(string.ascii_lowercase, k=6))


class cam:
    def __init__(self, device_id):
        self.device_id = device_id
        self.cap = None
        self.images = []
        self.bright = []
        self.cap = cv2.VideoCapture(self.device_id)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH,  camera_width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, camera_height)

        # Disable all automatic adjustments
        self.cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.0)  # 0.25 is manual mode (OFF)
        self.cap.set(cv2.CAP_PROP_AUTOFOCUS, 0)        # Disable autofocus
        self.cap.set(cv2.CAP_PROP_AUTO_WB, 0)          # Disable auto white balance
        self.cap.set(cv2.CAP_PROP_BRIGHTNESS, 0)       # Set fixed brightness
        self.cap.set(cv2.CAP_PROP_CONTRAST, 0)        # Set fixed contrast
        self.cap.set(cv2.CAP_PROP_GAIN, 0)             # Set manual gain
        
        # Set exposure if provided
        #self.cap.set(cv2.CAP_PROP_EXPOSURE, 0.1)
        
        if not self.cap.isOpened():
            print(f"error: could not open (camera {self.device_id})")
            exit(2)

    def read(self):
        # capture frame from webcam, and select the best and brightest
        ret, frame = self.cap.read()
        if not ret:
            print(f"error: could not read frame (camera {self.device_id})")
            exit(2)
        gray_image = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        current_brightness = np.mean(gray_image)
        self.images.insert(0, gray_image)
        self.bright.insert(0, current_brightness)
        if len(self.images) > 2:
            self.images.pop()
            self.bright.pop()
        best_bright = current_brightness
        selected = gray_image
        index = 0
        for b_value in self.bright:
            if b_value > best_bright:
                selected = self.images[index]
                best_bright = b_value
            index += 1

        # display reduced frame; not large enough to cause too much noise
        pip_frame = cv2.resize(selected, (pip_width, pip_height))
        return selected, pip_frame



##
# we want all of this in C with a variable set of cameras that the user must bind;
# not so hard to have a thumbnail and a option for each
#
# any overwriting camera would null any users duplicating it; thats better than toggling other off to make the other visible
# no confirmation dialogs or anything silly. trinity needs in it.  theres no way im making that separate from trinity
# trinity = UX (father, quantum membrane descriptor language to outter event horizon), 
#           3D (son,    the physical realm, continuously encoding new entanglements in timescapes),
#           AR (spirit; voice, and body -- something to visualize more dimensions of data and move with us on 
#                       corrective-counter-planar projection)
# 
##




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

sct = mss.mss()

monitor = {
    "top": 0,
    "left": 0,
    "width": screen_width,
    "height": screen_height,
}

def grab_screen():
    img = sct.grab(monitor)
    frame = np.array(img)[..., :3]  # BGRA -> BGR
    frame = cv2.resize(frame, (1280, 720))
    return frame


ensure_hyperspace_loopback()

key = 'x8k3-tthb-y4e7-6mgk-0rdm'
WIDTH  = 1280
HEIGHT = 720   # 340 * 2

args         = parser.parse_args()
mode         = args.mode
debug        = args.debug
# logitech brio is a good device, /dev/video6
camera_width, camera_height = 340, 340
title        = "hyperspace:record"
frame_count  = 0
pip_scale    = 0.5  # Scale the PiP window to 10% of the background
pip_pad      = 0.5
pip_width, pip_height = int(camera_width * pip_scale), int(camera_height * pip_scale)
pip_x, pip_y = screen_width - pip_width - int(camera_width * pip_pad), screen_height - pip_height - int(camera_width * pip_pad)  # Bottom-right with padding
session      = args.session

# create virtual camera writer
fake = pyfakewebcam.FakeWebcam('/dev/video10', WIDTH, HEIGHT)

os.makedirs(f'sessions/{session}', exist_ok=True)

# monitor microphone levels
sample_rate = 44100  # standard audio sample rate (Hz)
block_size  = 2048   # number of frames per block
stream      = sd.InputStream(callback=audio_callback, 
                        channels=1,
                        samplerate=sample_rate,
                        blocksize=block_size)
stream.start()  # Start listening to the microphone

# rectangle: head here (center of rotation = your eye median); not the lower middle of your brain (who even knows where this is)
# circle: look at this

zone      = 3
zone_cm   = [ 10, 30, 50 ]
z_max     = 50
eye_x     = -1
eye_y     = -1
pcount    = 4
pindex    = 0
key       = 0
eyes_only = True

if mode == "annotation" and not debug:
    cv2.namedWindow(title, cv2.WND_PROP_FULLSCREEN)
    cv2.setWindowProperty(title, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
    background = random_bg_color()
    cv2.imshow(title, background)
else:
    background = None

cam2      = cam(2) # top
cam6      = cam(6) # bottom (stOP!.... bottom one!)
                   # i need glue for this bottom one; or call-upon someone to help me mount it properly.
selected  = False
selected2 = None
selected6 = None

interval    = 4
recording   = False
last_space  = 0
center_x    = random.randint(0, screen_width)
center_y    = random.randint(0, screen_height)
offset_iter = 0
offset_x    = 0
offset_y    = 0
eye_x          = 0
eye_y          = 0

update_center_offset()

# Define the orbit parameters
orbit_radius = 200  # Distance from the main circle
orbit_period = 5    # Time in seconds for a full orbit

key = '7b6z-58qx-dbq5-ceaa-eyrb'
f_frame2 = None

# launch ffmpeg
YOUTUBE_URL = f"rtmp://a.rtmp.youtube.com/live2/{key}"
ffmpeg = subprocess.Popen([
    "ffmpeg",
    "-fflags", "nobuffer",
    "-flush_packets", "1",
    "-flags", "low_delay",
    "-f", "rawvideo",
    "-pix_fmt", "rgb24",
    "-s", f'{WIDTH}x{HEIGHT}',
    "-r", "30",
    "-i", "pipe:0",
    "-f", "alsa", "-i", "hw:4,0",
    #"-f", "lavfi",
    #"-i", "anullsrc=r=44100:cl=stereo",
    "-c:v", "libx264",
    "-preset", "veryfast",
    "-tune", "zerolatency",
    "-pix_fmt", "yuv420p",
    "-g", "30",
    "-b:v", "1500k",
    "-maxrate", "1500k",
    "-bufsize", "6000k",
    "-c:a", "aac",
    "-b:a", "128k",
    "-ar", "44100",
    # Map both streams explicitly
    "-map", "0:v",
    "-map", "1:a",
    "-f", "flv",
    "-rtmp_live", "live",
    YOUTUBE_URL
], stdin=subprocess.PIPE)


time.sleep(2)

while True:
    t = time.time()

    # refresh background image every 3 seconds
    if recording and frame_count % interval == 0: # we should also support audio-based capture
        #playsound('beep.wav')
        #write(2, selected2)
        #write(6, selected6)
        threading.Thread(target=write_in_thread, args=(2, selected2), daemon=True).start()
        threading.Thread(target=write_in_thread, args=(6, selected6), daemon=True).start()

    #g = 32 # random.randint(0, 64)
    #f_frame = np.zeros((720, 1280, 3), dtype=np.uint8)

    if mode == "annotation":
        blended_frame = background.copy()

        selected2, pip_frame2 = cam2.read()
        blended_frame[pip_y-pip_height:pip_y, pip_x:pip_x+pip_width] = pip_frame2

        selected6, pip_frame6 = cam6.read()
        blended_frame[pip_y:pip_height+pip_y, pip_x:pip_x+pip_width] = pip_frame6

        cv2.circle(blended_frame, (center_x, center_y), 32, 160, 1)
        cv2.circle(blended_frame, (center_x + offset_x, center_y + offset_y), 4, 255, 1)

        cv2.imshow(title, blended_frame)
    else:
        # read cameras
        selected2, pip2 = cam2.read()
        selected6, pip6 = cam6.read()

        # stack vertically (680 x 340)
        stacked = np.vstack((selected2, selected6))
        stacked_rgb = np.stack((stacked,)*3, axis=-1)

        # screen capture as background
        ##if f_frame2 is None or frame_count % 15 == 0:
        f_frame2 = grab_screen()
        f_frame2 = cv2.cvtColor(f_frame2, cv2.COLOR_BGR2RGB)

        # PiP placement (bottom-right)
        pip_h, pip_w = stacked_rgb.shape[:2]
        pos_y = 720 - pip_h - 10
        pos_x = 1280 - pip_w - 10

        f_frame2[pos_y:pos_y+pip_h, pos_x:pos_x+pip_w] = stacked_rgb

        # send to virtual webcam and ffmpeg
        fake.schedule_frame(f_frame2)
        ffmpeg.stdin.write(f_frame2.tobytes())

    frame_count += 1


    #print(f'mic: {mic_level}')
    key = cv2.waitKey(10) & 0xFF if mode == "annotation" else -1
    
    
    current_ticks = int(time.time() * 1000)  # Current time in milliseconds
    
    # Handle spacebar with cooldown
    if key == 32 and (current_ticks - last_space > 500):
        #recording = not recording
        last_space = current_ticks
        if not recording:
            write(2, selected2)
            write(6, selected6)
            update_center_offset()

    if mode == "annotation" and key == 27:
        break

cv2.destroyAllWindows()
