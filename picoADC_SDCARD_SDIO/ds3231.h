

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include <time.h>
//#include "pico/util/datetime.h"

// DS3231 I2C settings
#define I2C_PORT i2c0
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
#define I2C_ADDR 0x68
#define DS3231_I2C_TIMEOUT 5000



// initialize DS3231 rtc
void initDS3231 ();

// read and write DS3231 rtc registers
int readDS3231Time (datetime_t * dt);
int readDS3231 (uint8_t * buffer, int bufferLength);
int writeDS3231 (uint8_t * buffer, int bufferLength);

