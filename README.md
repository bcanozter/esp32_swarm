# esp32_swarm

Use esp-mesh-lite to form a mesh network, espnow to broadcast state within this mesh.


## References

- [ESP-Mesh-Lite](https://github.com/espressif/esp-mesh-lite)


## Build

> esp-idf version `5.5.1`

Child node:

`idf.py -B build/child -DSDKCONFIG=build/child/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32" -p /dev/ttyUSB0 build flash monitor`

Root node (one per mesh), set `MESH_ROOT=y` in the environment:

`MESH_ROOT=y idf.py -B build/root -DSDKCONFIG=build/root/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32" -p /dev/ttyUSB0 build flash monitor`
