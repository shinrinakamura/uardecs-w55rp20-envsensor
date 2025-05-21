#ifndef _SHT3x_H_
#define _SHT3x_H_

#include "Arduino.h"
#include "Wire.h"

extern int i2c_sda_pin;
extern int i2c_scl_pin;

#define SHT3x_ADDR    0x45  // センサアドレス
#define SHT3x_MEAS_HIGHREP_STRETCH 0x2C06
#define SHT3x_MEAS_MEDREP_STRETCH  0x2C0D
#define SHT3x_MEAS_LOWREP_STRETCH  0x2C10
#define SHT3x_MEAS_HIGHREP         0x2400
#define SHT3x_MEAS_MEDREP          0x240B
#define SHT3x_MEAS_LOWREP          0x2416
#define SHT3x_READSTATUS           0xF32D
#define SHT3x_CLEARSTATUS          0x3041
#define SHT3x_SOFTRESET            0x30A2
#define SHT3x_HEATEREN             0x306D
#define SHT3x_HEATERDIS            0x3066

class SHT3x {
  public:
    SHT3x();    
    boolean begin(uint8_t i2caddr = SHT3x_ADDR);
    void startMeasure(void);
    boolean getTempHumid(void);
    void reset(void);
    void heater(boolean);
    double humidity, temp;

  private:
    void writeCommand(uint16_t cmd);
    uint8_t crc8(const uint8_t *data, int len);
    uint8_t _i2caddr;
    
};

#endif
