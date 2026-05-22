#ifndef PID_H
#define PID_H

// PID controller based on proportional band in standard PID form
// https://en.wikipedia.org/wiki/PID_controller#Ideal_versus_standard_PID_form
// u = Kp (e(t) + 1/Ti INT + Td de/dt)
//
// Ported from PelletPirateC++_WB/src/pid2.cpp (Particle Photon)
// Changes: Time.now() -> esp_timer, Log.info() -> ESP_LOGI()

class pid
{
public:
    // PB = Proportional Band
    // Ti = Integral time (goal of eliminating error in Ti seconds)
    // Td = Derivative time (predicts error value at Td seconds)
    // loglevel: 0=off, 1=basic, 3=verbose
    pid(double PB, double Ti, double Td, int loglevel);

    void CalculateGains(double PB, double Ti, double Td, int loglevel);
    void setTarget(double SetPoint, int loglevel);
    double update(double Current, double SetPoint, int loglevel);
    void setGains(double PB, double Ti, double Td, int loglevel);

    double LastUpdate;

private:
    double P, I, D, u, Ki, Kp, Kd, Derv, Inter, Inter_max, Error, Last, setPoint;
};

#endif
