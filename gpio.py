import RPi.GPIO as GPIO
from time import sleep
GPIO.setmode(GPIO.BCM)
GPIO.setup(26, GPIO.IN)
GPIO.setup(5, GPIO.IN)
GPIO.setup(6, GPIO.IN)
GPIO.setup(4, GPIO.IN)
print("Done setup")
try:
    while True:
        red = GPIO.input(6)
        green = GPIO.input(5)
        ultrasonic = 2*GPIO.input(26) + GPIO.input(4)
        print(f"ULTRASONIC: {ultrasonic}    GREEN: {green}    RED: {red}")
        #sleep(0.2)
except KeyboardInterrupt:
    print("Exiting...")

GPIO.cleanup()
