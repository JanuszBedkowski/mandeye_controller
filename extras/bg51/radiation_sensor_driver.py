#  Copyright 2024 Jakub Delicat
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.


import struct
import serial
import binascii
import time

# from radiation_msgs.msg import Radiation


PACKET_FORMAT = "<IfI"  # Little-endian
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)


class RadiationSensorDriver():
    def __init__(self):
        # self.declare_parameter("serial_port", "/dev/ttyACM0")
        # self.declare_parameter("frame_id", "BG51")

        self.serial_port = "/dev/ttyACM0"
        self.baud_rate = 115200


        # Serial connection setup
        self.wait_for_serial_and_setup()

        print("Radiation sensor driver initialized.")

    def wait_for_serial_and_setup(self):
        while not self.setup_usb():
            self.get_logger().warn(f"Trying to connect to USB device {self.serial_port}...")
            time.sleep(1)

    def setup_usb(self):
        try:
            self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=1)
            print(f"Connected to {self.serial_port}")
            # self.timer = self.create_timer(0.1, self.read_serial_data)
            return True
        except serial.SerialException as e:
            self.get_logger().error(f"Serial error: {e}")

        return False

    def calculate_crc32(self, data):
        """CRC32 with padding to 32-bit boundary."""
        padded_data = data + b"\x00" * ((4 - (len(data) % 4)) % 4)
        return binascii.crc32(padded_data) & 0xFFFFFFFF

    def read_full_packet(self):
        """Read packet in chunks from USB."""
        data = b""
        while len(data) < PACKET_SIZE:
            chunk = self.ser.read(min(PACKET_SIZE - len(data), 64))
            if not chunk:
                raise ValueError("Timeout or incomplete packet received.")
            data += chunk
        return data

    def read_serial_data(self):
        try:
            packet_data = self.read_full_packet()
            cpm, siverts = self.process_packet(packet_data)

            print(f"CPM: {cpm}, Siverts: {siverts}")

        except serial.SerialException as e:
            print(f"Serial error: {e}")
            self.ser.close()
            self.timer.cancel()
            self.wait_for_serial_and_setup()

        except ValueError as e:
            print(f"Error: {e}")

    def process_packet(self, data):
        """Unpack and validate packet."""
        cpm, siverts, received_crc = struct.unpack(PACKET_FORMAT, data)
        calculated_crc = self.calculate_crc32(data[:-4])

        if calculated_crc != received_crc:
            raise ValueError(
                f"CRC mismatch! Received: {received_crc:08X}, Calculated: {calculated_crc:08X}"
            )

        return cpm, siverts

    def destroy_node(self):
        if self.ser.is_open:
            self.ser.close()
        super().destroy_node()


def main(args=None):

    radiation_sensor_driver = RadiationSensorDriver()
    while True:
        radiation_sensor_driver.read_serial_data()
        time.sleep(0.1)



if __name__ == "__main__":
    main()
