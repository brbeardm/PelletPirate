#ifndef max31865_h
#define max31865_h

#include "Particle.h"

/* SPI TIMEOUT Value*/
#define TIMEOUT_VAL 60

/* Read Register Address */
#define REG_CONFIG 0x00
#define REG_RTD_MSB 0x01
#define REG_RTD_LSB 0x02
#define REG_HIGH_FAULT_THR_MSB 0x03
#define REG_HIGH_FAULT_THR_LSB 0x04
#define REG_LOW_FAULT_THR_MSB 0x05
#define REG_LOW_FAULT_THR_LSB 0x06
#define REG_FAULT_STATUS 0x07
#define WR(reg) ((reg) | 0x80)

class MAX31865
{

  public:
    // commonly used functions
    MAX31865(int);
    void Write(int, byte, byte);
    byte Read(int, byte);
    //        float calc_Temp(int, int);
    void Fault(byte);
    float get_Temp(int);

    // display functions go below here

  private:
    //        int currentCS;
    //        int currentDRDY;
    //        float Tempf;
};
#endif
