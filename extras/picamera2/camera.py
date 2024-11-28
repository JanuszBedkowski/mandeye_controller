
import zmq
import time
import json
from picamera2 import Picamera2

# Set up ZeroMQ context and subscriber socket
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5556") 
socket.setsockopt_string(zmq.SUBSCRIBE, "")

# Initialize the Picamera2 camera
picam2 = Picamera2()
camera_config = picam2.create_still_configuration(main={"size": picam2.sensor_resolution})
picam2.configure(camera_config)

#Manual exposure time
#picam2.set_controls({"AeEnable": 0, "ExposureTime": 25000})  # Example: 5 ms exposure

# automated exposue, set to small value
picam2.set_controls({"ExposureValue": -4})

#
#picam2.set_controls({"HdrMode":4});

picam2.start()
# List the controls and their properties

controls = picam2.camera_controls
print("Available controls:")
for control, info in controls.items():
    print(f"{control}: {info}")
    
print("Subscriber connected and listening for messages...")
picam2.capture_file("/tmp/test.jpg")
# Receive messages in a loop and capture an image for each
while True:
    # Wait for a message
    message = socket.recv_string()
    print("Received message:", message)
    try:
        data = json.loads(message)

        mode = data['mode']
        ts = int(data['time'])
        data_continous = data['continousScanDirectory']
        
  
        if mode=='SCANNING':
            filename = f"{data_continous}/photo_{ts}.jpg"
            picam2.capture_file(filename)
            print(f"Image saved as {filename}")
    except Exception as X:
        print (X)
