# This script is used to charge a lipo battery to full voltage using the Jumperless
# Then discharge it through a load resistor and save the data to a file with timestamped lines
# when the battery is discharges to below 3.0V, it will charge it back up to 4.2V
# and repeat the process saving to a separate file each time.

chargeCurrent = input("Enter charge current in mA (0-80) > ")

print(chargeCurrent)
oled_print(chargeCurrent) 











