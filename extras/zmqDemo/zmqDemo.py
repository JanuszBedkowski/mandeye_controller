import zmq

# Set up ZeroMQ context and subscriber socket
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5556")
socket.setsockopt_string(zmq.SUBSCRIBE, "")
socket.setsockopt(zmq.CONFLATE,1)


while True:
    # Wait for a message
    message = socket.recv_string()
    print("Received message:", message)
