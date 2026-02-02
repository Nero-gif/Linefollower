from machine import Pin
import time

# Většina ESP32-C3 vývojových desek má vestavěnou LED na pinu 8
led = Pin(8, Pin.OUT)

print("Startuji blikání...")

while True:
    led.value(1)  # Rozsvítit
    time.sleep(0.5)
    led.value(0)  # Zhasnout
    time.sleep(0.5)

#skill issue ze nemas "LED" pin