#!/usr/bin/python
import ultrasonic as us
import time
us.init()
id = us.connect(4,5)

while True:
    print "Distance :", us.measure(id, 200.78)
    time.sleep(0.1)
    
