
import zmq
import time
import json
import os
from picamera2 import Picamera2


from PIL import Image
from fractions import Fraction
from PIL.ExifTags import TAGS, GPSTAGS

import time

import io

def decimal_to_dms(decimal):
    degrees = int(decimal)
    minutes = int((decimal - degrees) * 60)
    seconds = (decimal - degrees - minutes / 60) * 3600
    return (Fraction(degrees).limit_denominator(),
            Fraction(minutes).limit_denominator(),
            Fraction(seconds).limit_denominator())

def save_jpg_exif(filename, image_array, lat, lon, alt=0):

    """
    Adds GPS coordinates to the EXIF metadata of an image.
    
    Args:
        filename (str): Path to the input image file.
        image_array (BytesIO): Bytes data
        lat (float): Latitude in decimal degrees. Positive for North, negative for South.
        lon (float): Longitude in decimal degrees. Positive for East, negative for West.
    """


    start_time = time.time()
    # GPS Reference
    lat_ref = "N" if lat >= 0 else "S"
    lon_ref = "E" if lon >= 0 else "W"
    altitude_ref = 0 if alt >= 0 else 1  # 0: Above sea level, 1: Below sea level
    # Convert to EXIF GPS format
    gps_info = {
        1: lat_ref,  # Latitude reference
        2: decimal_to_dms(abs(lat)),  # Latitude
        3: lon_ref,  # Longitude reference
        4: decimal_to_dms(abs(lon)),  # Longitude
        5: altitude_ref,  # Altitude reference
        6: Fraction(abs(alt)).limit_denominator(),  # Altitude
    }

    # Open the image
    image = Image.fromarray(image_array)

    # Create EXIF data dictionary
    exif_data = image.getexif()
    exif_data[0x8825] = gps_info  # 0x8825 is the GPSInfo tag

    # Save the new image with GPS EXIF data
    image.save(filename, "JPEG", exif=exif_data)
    end_time = time.time()

    print(f"GPS coordinates added to {filename}, took {end_time - start_time:.4f}  secs")


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
    data['picamera']={}
    data['picamera_notes']={}
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


image_array = picam2.capture_array()
save_jpg_exif("/tmp/test.jpg", image_array, 52.50, 21.2222)
#with open("/tmp/test.jpg", "wb") as f:
#    f.write(data)
# add_gps_to_exif("/tmp/test.jpg", 37.7749, -122.4194)
# Receive messages in a loop and capture an image for each
while True:
    # Wait for a message
    message = socket.recv_string()
    print("Received message:", message)
    try:
        data = json.loads(message)

        mode = data['mode']
        ts = int(data['time'])
        gps_lat = 0
        gps_lon = 0
        gps_alt = 0
        if ('gnss' in data):
            gnss = data['gnss']
            print(gnss)
            if ('lat' in gnss):
                gps_lat = gnss['lat']
            if ('lon' in gnss):
                gps_lon = gnss['lon']
            if ('alt' in gnss):
                gps_alt = gnss['alt']
                        
        data_continous = data['continousScanDirectory']
        
  
        if mode=='SCANNING':
            filename = f"{data_continous}/photo_{ts}.jpg"
            image_array = picam2.capture_array()
            save_jpg_exif(filename, image_array, gps_lat, gps_lon, gps_alt)
            print(f"Image saved as {filename}")
    except Exception as X:
        print (X)
