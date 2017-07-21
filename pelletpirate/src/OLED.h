#ifndef OLED_h
#define OLED_h

#include "clickButton.h"
//#include "Particle.h"

class OLED
{

  public:
    //commonly used functions **************************************************************************
    OLED(); // * constructor.  links the OLED to the functions

    void command(byte);
    void data(byte);
    void send_packet(byte);
    void output(const byte Stringch[4][21]);
    void OLED_init(void);
    void OLED_menu1(void);
    void OLED_cook(void);
    void buttonPress(void);
    void menu1(void);
    //void update_OLED_temps(float*[3], int*);
    void update_OLED_temps(float, float, float, float);
    void outputTTemp(const byte Stringch[3][4]);

    void update_OLED_mode(char mode[9], int modestate);
    void outputMode(const byte Stringch[5][9]);

    void update_OLED_pid(double uPID);
    void update_FIA(int, int, int);
    void outputPID(const byte Stringch[1][5]);

    void buttonChangeTarget(bool);
    void update_OLED_target(float tt);

    //float Grill, Meat1, Meat2, PID;
    float targetTemp;
    char MODE[9];
    char MODEbutton[9];
    char MODEarray[6][9] = {"Off", "Start", "Smoke", "Ignite", "Hold", "Shutdown"};
    int ModeState;
    int RunCookFlag;

  private:
    //double   P, I, D, u, Ki, Kp, Kd, Derv, Inter, Inter_max, Error, LastUpdate = 0;
    //int     Last, setPoint;
};
#endif
