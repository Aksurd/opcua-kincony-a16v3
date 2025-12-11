# Firmware v1.0.0

**Release Date:** December 11, 2024  
**Target Hardware:** Kincony KC868-A16V3  
**ESP32 Model:** ESP32-S3  
**Flash Size:** 8MB  

## Files
- `opcua-kincony-a16v3.bin` - Main application (1.0 MB)
- `bootloader.bin` - ESP32 bootloader (21 KB)
- `partition-table.bin` - Partition table (3 KB)

## Features
- OPC UA Server v1.0
- 16 digital inputs (PCF8574)
- 16 digital outputs (PCF8574 relays)
- 4 ADC channels (12-bit)
- Ethernet support
- I/O caching system
- Performance monitoring

## Performance Metrics
- Average response time: <5ms
- Maximum polling rate: 18.9 Hz (all tags)
- Reliability: 100% (tested with 666 operations)
- All tags meet <10ms industrial requirement

## Checksums
```
SHA256(opcua-kincony-a16v3.bin): 4d76020fc621cd68459f36d6fa52f47137f6030cf39dc1db39dc5a1205a5c28b
SHA256(bootloader.bin): aef838d84acc0ce7d2141ee76ba8bd06a4e8c8b9f59ab5d817d661ee31b972e6
SHA256(partition-table.bin): 5295e8c42d6fb5f95e4e8a185932c6191d27cfc5078322475347e40e71af815a
```

## Flashing Instructions
```bash
esptool.py -p /dev/ttyUSB0 -b 460800 \
  --before=default_reset \
  --after=hard_reset \
  write_flash \
  0x1000 firmware/v1.0.0/bootloader.bin \
  0x8000 firmware/v1.0.0/partition-table.bin \
  0x10000 firmware/v1.0.0/opcua-kincony-a16v3.bin
```

## Network Defaults
- OPC UA Endpoint: `opc.tcp://[device-ip]:4840`
- Security: None (anonymous access)
- Baud Rate: 115200 for serial monitor

## Known Issues
- NTP time sync may fail without internet connection

## Support
For issues, check serial output at 115200 baud or open an issue on GitHub.
