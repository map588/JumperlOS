import jumperless as j
import time

node_1 = 1
node_2 = 4

last_node_1 = 0
last_node_2 = 0

sleepTime = 0.01

hue = 50

j.clickwheel_reset_position()

last_pos = j.clickwheel_get_position()

switchPosition = j.get_switch_position()

bridgeSpread = 4

while True:
    

#Probe stuff
    j.force_service("ProbeSwitch")
    switchPosition = j.get_switch_position() #this is fairly slow so we need to force service it

    if switchPosition == j.SWITCH_MEASURE: #make the bridge move if the switch is in measure
        node_1 += 1
        node_2 += 1
    


    tappedNode = j.read_probe(False) #False for non-blocking
    
    if (tappedNode != j.NO_PAD):
        # print(tappedNode)
        if (tappedNode <= 60):
            node_1 = int(tappedNode)
            node_2 = int(tappedNode + bridgeSpread)

#Encoder stuff
    pos = -j.clickwheel_get_position() #invert so it moves the right way
    
    if pos != last_pos:
        # print((pos-last_pos))
        node_1 += pos-last_pos
        node_2 = node_1+bridgeSpread
        last_pos = pos
    
    encoder_button = j.clickwheel_get_button()
    
    if encoder_button == j.CLICKWHEEL_PRESSED:
        bridgeSpread += 1
        node_2 = node_1 + bridgeSpread
        
    elif encoder_button == j.CLICKWHEEL_HELD:
        bridgeSpread -=1
        node_2 = node_1 + bridgeSpread

#Bounds check
    if (node_1 > 60):
        node_1 -= 60

    if (node_2 > 60):
        node_2 -= 60

    if (node_1 < 1):
        node_1 += 60
        
    if (node_2 < 1):
        node_2 += 60

#Send connections if they've changed
    if (node_1 != last_node_1) or (node_2 != last_node_2):
        j.oled_print("node 1 = " + str(node_1) + "\n\rnode 2 = " + str(node_2))
        # print("node 2 =" + str(node_2))

        j.disconnect(last_node_1, last_node_2)
        j.connect(node_1, node_2)

    last_node_1 = node_1
    last_node_2 = node_2

#Net info stuff
    lastNet = j.get_num_nets() - 1
    j.set_net_color_hsv(lastNet,hue)
    
#Probe button stuff 
    if (sleepTime < 0.05):
        j.force_service("ProbeButton") #this is needed if there's not enough time in the time.sleep to check the button

    button = j.check_button()
    
    if (button == j.BUTTON_CONNECT):
        # print("CONNECT")
        hue += 8
        if (hue > 255):
            hue = 0
        # print(hue)
    if (button == j.BUTTON_REMOVE):
        # print("REMOVE")
        hue -= 1
        if (hue < 0):
            hue = 255
        # print (hue)

    
    time.sleep(sleepTime)

