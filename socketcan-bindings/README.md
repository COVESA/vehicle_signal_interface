# SocketCAN bindings to VSI
**NOTE:** To succesfully build you need to add/update can-sginals submodule i.e.
```
git submodule update --init
```

## CAN frame layout for GENIVI AMM studio:
* frame 0x111, length 8 bytes
    * bit 0-7 vehicle.engine.oilpressure
    * bit 8-15 vehicle.engine.rpm
    * bit 16-23 vehicle.engine.temperature
    * bit 24 - vehicle.turnsignal.right
    * bit 25 - vehicle.turnsignal.left
    * bit 32-39 - vehicle.battery
    * bit 40-43 - vehicle.transmission.gear
    * bit 44-47 - vehicle.ignition
    * bit 48-55 - vehicle.fuel
    * bit 56-63 - vehicle.speed
    
## To start vsi-socketcand as daemon
```
vsi-socketcand ../vehicle_signal_specification/genivi-demo_1.0.vsi can0
```

## To start vsi-socketcand in foreground
```
vsi-socketcand -F ../vehicle_signal_specification/genivi-demo_1.0.vsi can0
```

## To stop vsi-socketcand (daemon mode)
```
killall vsi-socketcand
```

## To test with Virtual CAN
```
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
ip link set can0 up
```

## Exemplary CAN frame
```
cansend can0 111#010203030a481122
```
Will be translated to:
```
Processing signal [1/10], vehicle.engine.oilpressure(9), s: 0, e: 7, min: 0, max: 160, type: 1
Signal received - name: vehicle.engine.oilpressure, id: 9, val(u8): 1
Processing signal [2/10], vehicle.engine.rpm(7), s: 8, e: 15, min: 0, max: 10, type: 1
Signal received - name: vehicle.engine.rpm, id: 7, val(u8): 2
Processing signal [3/10], vehicle.engine.temperature(8), s: 16, e: 23, min: 0, max: 120, type: 1
Signal received - name: vehicle.engine.temperature, id: 8, val(u8): 3
Processing signal [4/10], vehicle.turnsignal.right(3), s: 24, e: 24, min: 0, max: 1, type: 0
Signal received - name: vehicle.turnsignal.right, id: 3, val(bool): 1
Processing signal [5/10], vehicle.turnsignal.left(2), s: 25, e: 25, min: 0, max: 1, type: 0
Signal received - name: vehicle.turnsignal.left, id: 2, val(bool): 1
Processing signal [6/10], vehicle.battery(6), s: 32, e: 39, min: 10, max: 14, type: 1
Signal received - name: vehicle.battery, id: 6, val(u8): 10
Processing signal [7/10], vehicle.transmission.gear(10), s: 40, e: 43, min: 0, max: 8, type: 1
Signal received - name: vehicle.transmission.gear, id: 10, val(char): drive
Processing signal [8/10], vehicle.ignition(1), s: 44, e: 47, min: 0, max: 8, type: 1
Signal received - name: vehicle.ignition, id: 1, val(char): on
Processing signal [9/10], vehicle.fuel(5), s: 48, e: 55, min: 0, max: 100, type: 1
Signal received - name: vehicle.fuel, id: 5, val(u8): 17
Processing signal [10/10], vehicle.speed(4), s: 56, e: 63, min: 0, max: 220, type: 1
Signal received - name: vehicle.speed, id: 4, val(u8): 34
```
