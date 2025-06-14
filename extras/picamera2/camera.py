
import zmq
import time
import json
import os
from picamera2 import Picamera2
import time
import shutil
import threading

CONFIG_PATH_USB = "/media/usb/mandeye_config.json"
CONFIG_PATH_TMP = "/tmp/camera/mandeye_config.json"

received_lock = threading.Lock()
received_timestamp = 0
received_mode = ""
received_command = None
received_data_continous = ""
Blacklisted = ['FrameDurationLimits']
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
    data['picamera']={"_Note" : "Here User can adjust setting of their camera!"}
    data['picamera_notes']={"_Note" : "This section is more or less comment. It contains range and default value for each and every parameter."}
    supported_types={float,int,bool}
    
    for control, info in controls.items():
        assert(len(info)==3)
        if type(info[0]) in supported_types or type(info[1]) in supported_types and contol not in Blacklisted:
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
            if value is not None and key not in Blacklisted:
                print (f"Set param {key} to {value}")
                if key in controls:
                    picam2.set_controls({key: value})
                else:
                    print(f"Warning: Parameter '{key}' is not supported by the camera.")
        except Exception as e:
            print(f"Error: Failed to apply parameter '{key}'. {e}")

def threadedZmqClient():
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect("tcp://localhost:5556") # mandeye controller
    socket.connect("tcp://localhost:5557") # web client
    socket.setsockopt_string(zmq.SUBSCRIBE, "")
    socket.setsockopt(zmq.CONFLATE,1)
    print("Subscriber connected and listening for messages...")
    while True:
        global received_mode
        global received_command
        global received_timestamp
        global received_data_continous
        # Wait for a message
        message = socket.recv_string()
        try:
            data = json.loads(message)
            if 'command' in data:
                with received_lock:
                    received_command = data['command']
            if 'time' in data:
                with received_lock:
                    received_timestamp = int(data['time'])
            if 'mode' in data and 'continousScanDirectory' in data:
                with received_lock:
                    received_mode = data['mode']
                    received_data_continous = data['continousScanDirectory']
       
        except Exception as X:
            print (X)
if __name__ == "__main__":
        
    # Set up ZeroMQ context and subscriber socket
   
    # Initialize the Picamera2 camera
    picam2 = Picamera2()
    camera_config = picam2.create_still_configuration(main={"size": picam2.sensor_resolution})
    picam2.configure(camera_config)

    # try to copy config file from usb to /tmp/camera
    os.makedirs("/tmp/camera", exist_ok=True)
    if os.path.isfile(CONFIG_PATH_USB):
        shutil.copyfile(CONFIG_PATH_USB, CONFIG_PATH_TMP)
    else:
        print(f"Error: File '{CONFIG_PATH_USB}' does not exist.")
        print(f"Creating default config file at '{CONFIG_PATH_TMP}'")
        create_default_config(CONFIG_PATH_TMP)
    # Write a default configuration file if it does not exist
    create_default_config(CONFIG_PATH_USB+".default")

    # Load the configuration file from the tmp directory
    users_config = load_json_config(CONFIG_PATH_TMP)

    print ("User's config:")
    print ("===========================")
    print (users_config)
    print ("===========================")
    validate_and_apply_config(users_config)

    picam2.start()
    # List the controls and their properties

    controls = picam2.camera_controls
    print("Available controls:")
    for control, info in controls.items():
        print(f"{control}: {info}")

    picam2.capture_file("/tmp/camera/test.jpg")
    # Receive messages in a loop and capture an image for each
    testPhotoCount = 0
    print("Hello")
    zmqthread = threading.Thread(target=threadedZmqClient)
    zmqthread.start()
    while True:
        filename = None
        with received_lock:
            if received_mode == 'SCANNING':
                filename = f"{received_data_continous}/photo_{received_timestamp}.jpg"
            if received_command == 'RELOAD_CONFIG_TEST_PHOTO':
                picam2.stop()
                users_config = load_json_config(CONFIG_PATH_TMP)
                print ("User's config:")
                print ("===========================")
                print (users_config)
                print ("===========================")
                validate_and_apply_config(users_config)
                picam2.start()
                picam2.capture_file("/tmp/camera/test%03d.jpg"%testPhotoCount)
                testPhotoCount += 1
                received_command = None

        if filename is not None:
            picam2.capture_file(filename)
            print(f"Image saved as {filename}")
        time.sleep(2)
    zmqthread.join()


