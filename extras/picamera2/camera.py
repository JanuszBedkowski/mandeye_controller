
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


picam2.start()

print("Subscriber connected and listening for messages...")

# Receive messages in a loop and capture an image for each
while True:
    # Wait for a message
    message = socket.recv_string()
    print("Received message:", message)
    try:
        data = json.loads(message)

        mode = data['mode']
        ts = data['time']
        data_continous = data['continousScanDirectory']
        
  
        if mode=='SCANNING':
            filename = f"{data_continous}/photo_{ts}.jpg"
            picam2.capture_file(filename)
            print(f"Image saved as {filename}")
    except Exception as X:
        print (X)
