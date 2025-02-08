
import zmq
import time
import json
import os
from picamera2 import Picamera2


def load_json_config(file_path):
    """
    Loads and parses the JSON configuration from the specified file.
    :param file_path: Path to the JSON config file.
    :return: A dictionary with the parsed configuration or an empty dictionary if invalid.
    """
    if not os.path.isfile(file_path):
        print(f"Error: File '{file_path}' does not exist.")
        return {}

    try:
        with open(file_path, "r") as file:
            data = json.load(file)

        if not isinstance(data, dict):
            print("Error: Configuration file must contain a JSON object.")
            return {}
        if 'picamera' in data:
            return data['picamera']
        return {}
    except (json.JSONDecodeError, OSError) as e:
        print(f"Error: Failed to read or parse the configuration file. {e}")
        return {}

def create_default_config(file_path):
    controls = picam2.camera_controls
    print("Available controls:")
    data = {}
    data['note'] = "auto-generated file with default config. Do not edit."
    data['picamera']={"Note" : "Here User can adjust setting of their camera!"}
    data['picamera_notes']={"Note" : "This section is more or less comment. It contains range and default value for each and every parameter."}
    supported_types={float,int,bool}
    
    for control, info in controls.items():
        assert(len(info)==3)
        if type(info[0]) in supported_types or type(info[1]) in supported_types:
            data['picamera'][control]=info[2]
            data['picamera_notes'][control]={"min":info[0],"max":info[1], "def":info[2], "type": str(type(info[0])) }
    
        print(f"{control}: {info}")
    with open(file_path, "w") as file:
            json.dump(data,file, indent=4)
        
def validate_and_apply_config(config):
    """
    Validates and applies a dynamic configuration to Picamera2.
    :param picam: An instance of Picamera2.
    :param config: A dictionary of configuration parameters.
    """
    # Validate and apply general settings dynamically
    controls = picam2.camera_controls

    for key, value in config.items():
        try:
            if value is not None:
                print (f"Set param {key} to {value}")
                picam2.set_controls({key: value})
        except Exception as e:
            print(f"Error: Failed to apply parameter '{key}'. {e}")


# Set up ZeroMQ context and subscriber socket
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5556") 
socket.setsockopt_string(zmq.SUBSCRIBE, "")
socket.setsockopt(zmq.CONFLATE,1)

# Initialize the Picamera2 camera
picam2 = Picamera2()
camera_config = picam2.create_still_configuration(main={"size": picam2.sensor_resolution})
picam2.configure(camera_config)

#Manual exposure time
#picam2.set_controls({"AeEnable": 0, "ExposureTime": 25000})  # Example: 5 ms exposure

# automated exposue, set to small value
create_default_config("/media/usb/mandeye_config.json.default")

users_config = load_json_config("/media/usb/mandeye_config.json")


print ("User's config:")
print ("===========================")
print (users_config)
print ("===========================")
validate_and_apply_config(users_config)

#
#picam2.set_controls({"HdrMode":4});

picam2.start()
# List the controls and their properties

controls = picam2.camera_controls
print("Available controls:")
for control, info in controls.items():
    print(f"{control}: {info}")
    
print("Subscriber connected and listening for messages...")
os.makedirs("/tmp/camera", exist_ok=True)
picam2.capture_file("/tmp/camera/test.jpg")
# Receive messages in a loop and capture an image for each
while True:
    # Wait for a message
    message = socket.recv_string()
    print("Received message:", message)
    try:
        data = json.loads(message)
        if 'mode' in data and 'time' in data:
            mode = data['mode']
            ts = int(data['time'])
            data_continous = data['continousScanDirectory']
            
    
            if mode=='SCANNING':
                filename = f"{data_continous}/photo_{ts}.jpg"
                picam2.capture_file(filename)
                print(f"Image saved as {filename}")
    except Exception as X:
        print (X)
