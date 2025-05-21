#include "SHT3x.h"

SHT3x::SHT3x() {
}


boolean SHT3x::begin(uint8_t i2caddr) {
  
  Wire.setSDA(i2c_sda_pin);
  Wire.setSCL(i2c_scl_pin);

  Wire.begin();
  _i2caddr = i2caddr;
  reset();
  return true;
}


void SHT3x::reset(void) {
  writeCommand(SHT3x_SOFTRESET);
  delay(10);
}

void SHT3x::heater(boolean h) {
  if (h)
    writeCommand(SHT3x_HEATEREN);
  else
    writeCommand(SHT3x_HEATERDIS);
}

uint8_t SHT3x::crc8(const uint8_t *data, int len) {
  const uint8_t POLYNOMIAL(0x31);
  uint8_t crc(0xFF);
  
  for ( int j = len; j; --j ) {
      crc ^= *data++;

      for ( int i = 8; i; --i ) {
	crc = ( crc & 0x80 )
	  ? (crc << 1) ^ POLYNOMIAL
	  : (crc << 1);
      }
  }
  return crc; 
}

void SHT3x::startMeasure(void)
  {
   writeCommand(SHT3x_MEAS_HIGHREP);
    return;
    }

boolean SHT3x::getTempHumid(void) {

//  writeCommand(SHT3x_MEAS_HIGHREP);
//  delay(500);
  uint8_t readbuffer[6];
 
  Wire.requestFrom(_i2caddr, (uint8_t)6);
  if (Wire.available() != 6) 
    return false;
  for (uint8_t i=0; i<6; i++) {
    readbuffer[i] = Wire.read();
  }
  uint16_t ST, SRH;
  ST = readbuffer[0];
  ST <<= 8;
  ST |= readbuffer[1];

  if (readbuffer[2] != crc8(readbuffer, 2)) return false;

  SRH = readbuffer[3];
  SRH <<= 8;
  SRH |= readbuffer[4];

  if (readbuffer[5] != crc8(readbuffer+3, 2)) return false;
 
  double stemp = ST;
  stemp *= 175;
  stemp /= 0xffff;
  stemp = -45 + stemp;
  temp = stemp;
  
  double shum = SRH;
  shum *= 100;
  shum /= 0xFFFF;
  
  humidity = shum;
  
  return true;
}

void SHT3x::writeCommand(uint16_t cmd) {
  Wire.beginTransmission(_i2caddr);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  Wire.endTransmission();      
}
