from machine import Pin
import time, random

led = Pin("LED", Pin.OUT)
while True:
    led.toggle()
    time.sleep(random.uniform(0, .5))