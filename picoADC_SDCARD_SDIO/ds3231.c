
#include "ds3231.h"

// initialize ds3231 clock
void initDS3231 () {
  // initialize i2c hardware
  i2c_init(I2C_PORT, 100 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);
}

// read DS3231 time into struct tm
int readDS3231Time (datetime_t  * dt) {
  uint8_t reg = 0x00; // start register
  int status;
  int loop;
  uint8_t regs[7];
  regs[0]=0;
  // write address
  if(writeDS3231(regs, 1))
    return 0;
  if(readDS3231(regs, 7))
    return 0; // got error
   // ok fill datetime_t
  dt->sec = (((regs[0] & 0xf0) >> 4) * 10) + (regs[0] &0xf);
  dt->min = (((regs[1] & 0xf0) >> 4) * 10) + (regs[1] &0xf);

  if(regs[2] & 0x40)
     dt->hour = ((regs[2] & 0x20) ? 12 : 0)  + (((regs[2]>>4)&1) * 10) + regs[2]&0xf;
  else
     dt->hour = (((regs[2]>>4)&3) * 10) + (regs[2] & 0xf);
  dt->day =  ((regs[4]>>4)*10) + (regs[4]&0xf);
  dt->month = (((regs[5]>>4)&1)*10) + (regs[5]&0xf);
  dt->year = 2000+(((regs[6]>>4)& 0xf)*10) + ( regs[6]&0xf);
  dt->dotw = regs[3]-1;
  return 1;
}


// write buffer to DS3231 registers
int writeDS3231 (uint8_t * buffer, int bufferLength) {
  return !i2c_write_timeout_us(I2C_PORT, I2C_ADDR, buffer, bufferLength, true, DS3231_I2C_TIMEOUT);
}

// read registers from DS3231 to buffer
int readDS3231 (uint8_t * buffer, int bufferLength) {
  return !i2c_read_timeout_us(I2C_PORT, I2C_ADDR, buffer, bufferLength, true, DS3231_I2C_TIMEOUT);
}


