#ifndef PID2_h
#define PID2_h

class pid
{

  public:
    //    float P=0.0, I=0.0, D=0.0, u=0;

    //commonly used functions **************************************************************************
    pid(double, double, double); // * constructor.  links the PID to the Input, Output, and
                                 //   Setpoint.  Initial tuning parameters are also set here

    void CalculateGains(double, double, double);

    void setTarget(double);

    double update(double);

    void setGains(double, double, double);

    double LastUpdate = 0;
    //    double getK(double, double, double);

  private:
    double P, I, D, u, Ki, Kp, Kd, Derv, Inter, Inter_max, Error, Last, setPoint; //LastUpdate = 0;
    //int     Last, setPoint;
};
#endif
