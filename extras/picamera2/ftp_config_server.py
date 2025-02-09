import os
import zmq
import json
import threading
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

SHARED_DIR="/tmp/camera/"
CONFIG_PATH_TMP = "/tmp/camera/mandeye_config.json"

zmqcontext = zmq.Context()
zmqthread = None
zmqsocket = zmqcontext.socket(zmq.PUB)
zmqsocket.bind("tcp://*:5557")


class CustomFTPHandler(FTPHandler):


    def on_file_received(self, file_path):
        if file_path == CONFIG_PATH_TMP:
            print ("Config file was changed")
            data={"command" : "RELOAD_CONFIG_TEST_PHOTO"}
            zmqsocket.send_string(json.dumps(data))
        # You can replace the above line with your custom processing code.


if __name__ == "__main__":
    data={"command" : "RELOAD_CONFIG_TEST_PHOTO"}
    

    SHARED_DIR="/tmp/camera/"
    os.makedirs(SHARED_DIR, exist_ok=True)
    os.chdir(SHARED_DIR)


    # Instantiate a dummy authorizer for managing 'virtual' users
    authorizer = DummyAuthorizer()

    # Define a new user having full r/w permissions and a read-only
    # anonymous user
    authorizer.add_anonymous(os.getcwd(),perm='elradfmw')

    # Instantiate FTP handler class
    handler = CustomFTPHandler
    handler.authorizer = authorizer
    handler.permit_foreign_addresses = True

    # Define a customized banner (string returned when client connects)
    handler.banner = "Mandeye FTP camera config"


    # Instantiate FTP server class and listen on all interfaces, port 2121
    address = ('', 2121)
    server = FTPServer(address, handler)

    # set a limit for connections
    server.max_cons = 256
    server.max_cons_per_ip = 5

    # start ftp server
    server.serve_forever()
