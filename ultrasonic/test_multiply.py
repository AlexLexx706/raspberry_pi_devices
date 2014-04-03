#!/usr/bin/python
import ultrasonic as us
import time
us.init()
id_0 = us.connect(4,5)
id_1 = us.connect(8,9)
id_2 = us.connect(0,2)

res = []
start_time = time.time()

while True:
    d0 = us.measure_multiply([id_0,], 0.1)
    d1 = us.measure_multiply([id_1,], 0.1)
    d2 = us.measure_multiply([id_2,], 0.1) 
    d1 = us.measure_multiply([id_2,], 0.1)
    res.append(d2)
    ct = time.time()

    if ct - start_time > 3:
	print "count per second:", len(res)/(ct - start_time)
        res = []
	start_time = time.time()
    
