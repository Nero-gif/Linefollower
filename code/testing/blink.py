from machine import Pin
import rp2

led = Pin("LED", Pin.OUT)
while True:
    if rp2.bootsel_button():
        led.on()
    else:
        led.off()