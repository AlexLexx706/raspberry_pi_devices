#!/usr/bin/python
# -*- coding: utf-8 -*-

from distutils.core import setup, Extension
setup(name='ultrasonic', version='1.0',  \
      ext_modules=[Extension('ultrasonic', ['ultrasonic.c'], libraries=['wiringPi', 'stdc++', 'rt'],)])