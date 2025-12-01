#ifndef DS18B20_H_  
#define DS18B20_H_

#define DS18B20_OK 0
#define DS18B20_TIMEOUT_ERROR -1
#define DS18B20_CRC_ERROR -2
#define DS18B20_DEVICE_NOT_FOUND -3

// == function prototypes =======================================

int readDS18B20();
float getDSTemperature();
float ReadDSTemperature(int gpio);

#endif
