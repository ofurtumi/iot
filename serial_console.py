import argparse
import serial
import time

parser = argparse.ArgumentParser("Simple python serial console.")
parser.add_argument("-dst", "-D", dest="port_dest", type=str, nargs=1, help="Destination serial port")
args = parser.parse_args()

DEVICE_PROMPT = "> "

def is_ready(line):
	return len(line) > len("SYNC // FIRMWARE READY") and line[0:22] == "SYNC // FIRMWARE READY"

def is_complete(line):
	return line == ""

def query_device_v2(ser):
	in_line = ""
	listening = False
	while not listening:
		next = ser.read(size=1).decode("ascii")
		if next:
			in_line += next
			if in_line == DEVICE_PROMPT:
				time.sleep(0.05)
				if not ser.in_waiting:
					print(DEVICE_PROMPT, end="")
					listening = True
			elif next == "\n":
				print(in_line[:-1])
				in_line = ""

	cmd = input()
	if (cmd == "exit"):
		return True
	ser.write((cmd + "\n").encode("ascii"))
	return False


def sync_device(ser):
	# Little block to dump any pre-existing input on the serial port.  This may
	#  (depending on when/how console script is run) include boot dump info for
	#  device which is not of any great interest to us right now.  Keep scrapping
	#  input until 200ms go by with no further input.
	ser.reset_input_buffer()
	time.sleep(0.2)
	while ser.in_waiting > 0:
		ser.reset_input_buffer()
		time.sleep(0.2)

	print(f"Reboot device to synchronize.")
	while True:
		line = ser.readline().decode("ascii")
		if is_ready(line):
			print(f"\nSYNC // FIRMWARE READY")
			return
		else:
			print(".", end="", flush=True)




com = serial.Serial(port=args.port_dest[0], baudrate=115200, timeout=10, write_timeout=10);
if not com.is_open:
	print(f"Failed to connect to serial port {args.port_dest[0]}")
else:
	print(f"Console connected on serial port {args.port_dest[0]}")

sync_device(com)
complete = False
while not complete:
	complete = query_device_v2(com)
