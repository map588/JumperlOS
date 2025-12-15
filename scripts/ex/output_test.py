
import jumperless as j
import time


fakeGPIOtime = 0
regularConnectTime = 0
fastConnectTime = 0

j. nodes_clear()
j. set_dac(j. TOP_RAIL, 8.0)
j. set_dac(j.BOTTOM_RAIL, -8.0)

pin = j.FakeGpioPin(10, j.OUTPUT, j.TOP_RAIL, j.BOTTOM_RAIL)
pin2 = j.FakeGpioPin(19, j.OUTPUT, j.TOP_RAIL, j.BOTTOM_RAIL)

j. pause_core2 (True)
time.sleep (0.1)

startTime = time.ticks_us ()

for i in range (5000):
    
    pin.value(1) # Same thing as pin. on()
    pin2.off()
    pin. value (0)
    pin2.on()

    
j.pause_core2(False)
endTime = time.ticks_us()
fakeGPIOtime = (endTime - startTime)



j. nodes_clear()
time.sleep(0.5)

# j. pause_core2 (True)

time.sleep (0.1)
startTime = time.ticks_us ()

for i in range (50):
    j.connect(21, j.TOP_RAIL)
    j.disconnect(21, j.TOP_RAIL)

j.pause_core2(False)
endTime = time.ticks_us()
regularConnectTime = (endTime - startTime)

j. nodes_clear()

# j. pause_core2 (True)
time.sleep (0.1)
startTime = time.ticks_us ()

for i in range (50):
    j.fast_connect(21, j.TOP_RAIL)
    j.fast_disconnect(21, j.TOP_RAIL)

j.pause_core2(False)
endTime = time.ticks_us()
fastConnectTime = (endTime - startTime)



print (f"Took {fakeGPIOtime} us for 5000 toggles with fake gpio")

freq = (5000 / (fakeGPIOtime)) * 1000
print(f"Frequency = {freq} kHz")

print (f"Took {regularConnectTime} us for 50 toggles with fake gpio")

freq = (50 / (regularConnectTime)) * 1000
print(f"Frequency = {freq} kHz")

print (f"Took {fastConnectTime} us for 50 toggles with fake gpio")

freq = (50 / (fastConnectTime)) * 1000
print(f"Frequency = {freq} kHz")