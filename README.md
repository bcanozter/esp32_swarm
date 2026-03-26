# esp32_swarm

Use esp-mesh-lite to form a mesh network, espnow to broadcast state within this mesh.


## References

- [ESP-Mesh-Lite](https://github.com/espressif/esp-mesh-lite)


## Build

`idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32" -p /dev/ttyUSB0 build flash monitor`