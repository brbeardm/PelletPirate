#include "MAX31865.h"
#include "math.h"

//Registers defined in Table 1 on page 12 of the data sheet
static byte Configuration = 0b10000000;                  //0x80H
static byte read_Configuration = 0b00000000;             //0x00H
static byte Write_High_Fault_Threshold_MSB = 0b10000011; //0x83H
static byte Write_High_Fault_Threshold_LSB = 0b10000100; //0x84H
static byte Write_Low_Fault_Threshold_MSB = 0b10000101;  //0x85H
static byte Fault_Status = 0b00000111;                   //0x07H

//Callendar-Van Dusen equation is used for temperature linearization. Coeffeicant of equations are as follows:
//R(T) = R0(1 + aT + bT^2 + c(T - 100)T^3)
//Equation from : http://www.honeywell-sensor.com.cn/prodinfo/sensor_temperature/technical/c15_136.pdf
static float a = 0.00390830;
static float b = -0.0000005775;
//static float c = -0.00000000000418301;
float Reference_Resistor = 400; //Reference Resistor installed on the board.
float RTD_Resistance = 100;     //RTD Resistance at 0 Degrees. Please refer to your RTD data sheet.

double Temperature = 0;
byte Fault_Error = 0;
byte value = 0;

int unpluggedProbe = 0;

MAX31865::MAX31865(int CS)
{
    //pinMode (DRDY, INPUT);
    pinMode(CS, OUTPUT);
    SPI.begin(SPI_MODE_MASTER, CS);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE3);
    Write(CS, Configuration, 0b11010000); //was 10000000 - Enabling Vbias of max31865 now = 0xD2
    value = Read(CS, read_Configuration); //Reading contents of Configuration register to verify communication with max31865 is done properly
    Particle.publish("MAX31865 Setup Config Value: ", String(value), PRIVATE);
    if (value == 208)
    {
        Write(CS, Write_High_Fault_Threshold_MSB, 0xFF); //Writing High Fault Threshold MSB
        Write(CS, Write_High_Fault_Threshold_LSB, 0xFF); //Writing High Fault Threshold LSB
        Write(CS, Write_Low_Fault_Threshold_MSB, 0x00);  //Writing Low Fault Threshold MSB
        Write(CS, Write_Low_Fault_Threshold_MSB, 0x00);  //Writing Low Fault Threshold LSB
        Particle.publish("MAX31865 Temperature Configuration successful", PRIVATE);
    }
    else
    {
        Particle.publish("MAX31865 Config ERROR", PRIVATE);
    }
}

/* Write(byte, byte) function requires the register address and the data to be written to the register provided when called in the respective sequence. 
Write function does require that slaveSelectPin is properly defined in the setup. Changing the variable name of slaveSelectPin would require a change in Write function "slaveSelectPin" name as well
*/
void MAX31865::Write(int CS, byte w_addr, byte data)
{
    digitalWrite(CS, LOW);
    SPI.transfer(w_addr);
    SPI.transfer(data);
    digitalWrite(CS, HIGH);
}

//Read function(byte ) accepts the register address to be read and returns the contents of the register to the loop function
byte MAX31865::Read(int CS, byte r_addr)
{
    digitalWrite(CS, LOW);
    SPI.transfer(r_addr);       // read from configuration register
    value = SPI.transfer(0x00); // dummy write to provide SPI clock signals to read
    digitalWrite(CS, HIGH);
    return value;
}

float MAX31865::get_Temp(int CS)
{
    //Prior to getting started with RTD to Digital Conversion, Users can do a preliminary test to detect if their is a fault in RTD connection with max31865
    Fault_Error = Read(CS, Fault_Status);

    //If their is no fault detected, the get_Temp() is called and it initiates the conversion. The results are displayed on the serial console
    if (Fault_Error == 0)
    {
        Write(CS, Configuration, 0b11010000);
        float Tempf = 0.0;
        byte lsb_rtd = Read(CS, 0x02);
        byte fault_test = lsb_rtd & 0x01;
        //Particle.publish("Fault Test: ", String(fault_test), PRIVATE);       //Printing Temperature on console
        while (fault_test == 0)
        {
            byte msb_rtd = Read(CS, 0x01);
            float RTD = ((msb_rtd << 7) + ((lsb_rtd & 0xFE) >> 1));                                                                           //Combining RTD_MSB and RTD_LSB to protray decimal value. Removing MSB and LSB during   shifting/Anding
            float R = (RTD * Reference_Resistor) / 32768;                                                                                     //Conversion of ADC RTD code to resistance
            float Temp = -RTD_Resistance * a + sqrt(RTD_Resistance * RTD_Resistance * a * a - 4 * RTD_Resistance * b * (RTD_Resistance - R)); //Conversion of RTD resistance to Temperature
            Temp = Temp / (2 * RTD_Resistance * b);
            Tempf = Temp * 9 / 5 + 32;
            //Log.info("MAX31865_get_Temp %d (%.1f)\r\n", CS, Tempf);
            delay(TIMEOUT_VAL);
            lsb_rtd = Read(CS, 0x02);
            fault_test = lsb_rtd & 0x01;
            return Tempf;
        }
    }
    else
    {
        //If a fault is detected, Fault register is called and list of faults are displayed in the Serial console. Users are expected to troubleshoot the faults prior to proceeding
        Log.error("Fault Detected %d", Fault_Error);
        Fault(Fault_Error);
        Write(CS, Configuration, 0b10000010);
        delay(700); //Fault register isn't cleared automatically. Users are expected to clear it after every fault.
        Write(CS, Configuration, 0b11010000);
        delay(700);

    } //Setting the device in autoconfiguration again.
    return 0;
}

//Fault(byte) function requires the contents of the fault bit to be provided. It checks for the bits that are set and provides the faulty bit information on the serial console.
void MAX31865::Fault(byte fault)
{
        Particle.publish("Error MAX31865 in FAULT procedure", String(fault), PRIVATE);
        {
            //Log.error(fault, BIN);
            byte temp = 0;       //temporary variable created: Purpose is to find out which error bit is set in the fault register
            temp = fault & 0x80; //Logic Anding fault register contents with 0b10000000 to detect for D7 error bit
            if (temp > 0) {Log.error("Bit D7 is Set. It's Possible your RTD device is disconnected from RTD+ or RTD - High Fault Threshold Value"); }
            temp = fault & 0x40;
            if (temp > 0) {Log.error("Bit D6 is Set. It's Possible your RTD+ and RTD- is shorted - Low Fault Threshold Value."); }
            temp = fault & 0x20;
            if (temp > 0) {Log.error("Bit D5 is Set. Vref- is greater than 0.85 * Vbias"); }
            temp = fault & 0x10;
            if (temp > 0) {Log.error("Bit D4 is Set. Please refer to data sheet for more information"); }
            temp = fault & 0x08;
            if (temp > 0) {Log.error("Bit D3 is Set. Please refer to data sheet for more information"); }
            temp = fault & 0x04;
            if (temp > 0) {Log.error("Bit D2 is Set. Please refer to data sheet for more information"); }
        }
}
