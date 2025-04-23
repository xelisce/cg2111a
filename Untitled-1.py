#!/usr/bin/env python3
import serial
import time

if __name__ == '__main__':
    ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
    ser.reset_input_buffer()

    while True:
        user_input = input("Command: ")
        if user_input == "q":
            break;
        ser.write(user_input.encode())
        line = ser.readline().decode('utf-8').rstrip()
        print(line)
        time.sleep(0.1)