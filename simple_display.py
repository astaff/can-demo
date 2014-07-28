import curses
import curses.wrapper
import time
import serial

def display(screen):
	ser = serial.Serial('/dev/tty.usbmodem411', 115200)
	time.sleep(1)

	while True:
		s = ser.readline().rstrip()
		lines = ["", "", ""]

		i = 0

		if (s.startswith("Speed")):
			lines[0] = s
		if (s.startswith("RPM")):
			lines[1] = s
		if (s.startswith("Coolant")):
			lines[2] = s

		for i in range(3):
			screen.addstr(i, 0, lines[i])		
			screen.refresh()
		

curses.wrapper(display)

