import time

from machine import Pin, PWM

esc = PWM(Pin(2))
esc.freq(50)


def set_speed(us):
    duty = int(us * 65535 / 20000)  # 20 ms perioda
    esc.duty_u16(duty)
    print(us)


try:
    print("Inicializace ESC...")
    set_speed(1000)  # minimum
    time.sleep(5)

    for i in range(1000, 1100, 50):
        set_speed(i)
        time.sleep(2)

    time.sleep(30)
    # print("Start motoru...")
    # set_speed(1100)
    # time.sleep(5)
    #
    # print("Speeding...")
    # set_speed(1200)
    # time.sleep(5)

    print("Stop motoru...")
    esc.deinit()

except KeyboardInterrupt:
    # Bezpečné ukončení a vypnutí PWM
    esc.deinit()
    print("PWM vypnuto")
#

#
# print("Stop")
# set_speed(1000)
