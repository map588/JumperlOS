"""
Node connection and routing operations.
This example shows how to connect/disconnect nodes and check connections.
"""

import jumperless as j
import time

print("Node Connections Demo")
    
# Clear all existing connections
j.nodes_clear()
print("Cleared all connections")

# Test connections
test_connections = [
    (1, 30),
    (15, 45),
    (j.DAC0, 20),
    (j.GPIO_1, 25)
]

for node1, node2 in test_connections:
    j.oled_print("\nConnecting " + str(node1) + " to " + str(node2))
    
    # Connect nodes
    j.connect(node1, node2)
    
    # Check connection
    connected = j.is_connected(node1, node2)
    j.oled_print("Is connected: " + str(connected))
    
    time.sleep(0.5)
    
    # Disconnect
    j.disconnect(node1, node2)
    
    # Verify disconnection
    connected = j.is_connected(node1, node2)
    j.oled_print("Is connected: " + str(connected))
    
    time.sleep(0.5)

# Show final status
print("\nFinal status:")
j.print_bridges()

print("Node Connections complete!")
j.nodes_clear()

