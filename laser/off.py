#!/usr/bin/python
import RPi.GPIO as GPIO

GPIO_LASER = 4
GPIO.setmode(GPIO.BCM)
GPIO.setup(GPIO_LASER, GPIO.OUT)
GPIO.output(GPIO_LASER, False)
