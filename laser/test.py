#!/usr/bin/python
import sys
import RPi.GPIO as GPIO
import time

if  __name__ ==  "__main__" :
    GPIO_LASER = 4
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(GPIO_LASER, GPIO.OUT)
    
    delay = float(sys.argv[1]) if len(sys.argv) > 1 else 1

    while 1:
        GPIO.output(GPIO_LASER, False)
        time.sleep(delay)
        GPIO.output(GPIO_LASER, True)
        time.sleep(delay)
