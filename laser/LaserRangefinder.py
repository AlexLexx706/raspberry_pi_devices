#!/usr/bin/python
# -*- coding: utf-8 -*-
import cv2
import os
import sys
import RPi.GPIO as GPIO
from threading import Thread
import math

class LaserRangefinder:
    def __init__(self, res_queue=None):
        #1. Запустим драйвер
        os.system("sudo pkill uv4l")
        os.system("sudo uv4l --driver raspicam --framerate 25 --nopreview yes --width 1024 --height 768 --encoding h264 --auto-video_nr")
        
        #2. Запуск лазра
        self.laser_pin = 4

        #3. open video device
        self.cap = cv2.VideoCapture(0)
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.laser_pin, GPIO.OUT)

        #4. инициализация коэффициентов. 
        self.max_len = 1575.
        self.min_len = 175.
        
        self.max_pix = 24.
        self.min_pix = 153.
        self.camera_laser_offset = 60.
        
        self.max_angle = math.atan(self.camera_laser_offset / self.max_len)
        self.min_angle = math.atan(self.camera_laser_offset / self.min_len)
        self.angle_step = self.min_angle - self.max_angle
        self.radian_pixel = self.angle_step / (self.min_pix - self.max_pix)
        self.thread = None
        self.distance = 0
        self.stop_flag = False
        self.res_queue = res_queue
    
    def release(self):
        GPIO.cleanup()
    
    def start(self):
        if self.thread is None:
            self.thread = Thread(target=self.run)
            self.stop_flag = False
            self.thread.start()

    def stop(self):
        if self.thread is not None:
            self.stop_flag = True
            self.thread.join()
            self.thread = None
    
    def get_distance(self):
        return self.distance

    def calk_distance_mm(self, pix_pos):
        '''Рассчёт дистанции в мм'''
        angle = self.max_angle + (pix_pos - self.max_pix) * self.radian_pixel
        return self.camera_laser_offset / math.tan(angle)

    def run(self):
        GPIO.output(self.laser_pin, True)

        while not self.stop_flag:
            #0. получим картинку
            ret, frame = self.cap.read()

            if not ret:
                continue

            row, col, ch = frame.shape
            row = row / 2 + 20
            col = col / 2
            half_width = 20 / 2
                
            #1. выделим канал крассного цвета
            slice = frame[row - half_width: row + half_width, col:, 2]

            #2. определим точку с максимальной интенсивностью
            minVal, maxVal, minLoc, max_loc = cv2.minMaxLoc(slice)

            #3. отображение.
            self.distance = self.calk_distance_mm(max_loc[0])
            
            if self.res_queue is not None:
                self.res_queue.put(self.distance)

        self.cap.release()
        cv2.destroyAllWindows()
        GPIO.output(self.laser_pin, False)

if __name__ == "__main__":
    import time
    laser = LaserRangefinder()
    while 1:
        laser.start()
        time.sleep(4)
        print laser.get_distance()
        time.sleep(4)
        laser.stop()
        time.sleep(4)