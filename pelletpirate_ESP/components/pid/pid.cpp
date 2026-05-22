// PID controller - ported from PelletPirateC++_WB/src/pid2.cpp
// Original: Particle Photon platform
// Changes: Time.now() -> time_now_sec(), Log.info() -> ESP_LOGI()

#include "pid.h"
#include <cmath>
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "pid";

// Returns current time in seconds (fractional), replacing Particle's Time.now()
static double time_now_sec(void)
{
    return (double)esp_timer_get_time() / 1000000.0;
}

pid::pid(double PB, double Ti, double Td, int loglevel)
{
    // PID controller based on proportional band in standard PID form
    // u = Kp (e(t) + 1/Ti INT + Td de/dt)
    CalculateGains(PB, Ti, Td, loglevel);

    P = 0.0;
    I = 0.0;
    D = 0.0;
    u = 0.0;

    Derv = 0.0;
    Inter = 0.0;
    Inter_max = fabs(0.5 / Ki);

    Last = 150;

    setTarget(0.0, loglevel);
}

void pid::CalculateGains(double PB, double Ti, double Td, int loglevel)
{
    Kp = -1 / PB;
    Ki = Kp / Ti;
    Kd = Kp * Td;
    if (loglevel == 3) {
        ESP_LOGI(TAG, "PB: %f  Ti: %f  Td: %f --> Kp: %f  Ki: %f  Kd: %f", PB, Ti, Td, Kp, Ki, Kd);
    }
}

void pid::setTarget(double SetPoint, int loglevel)
{
    setPoint = SetPoint;
    Error = 0.0;
    Inter = 0.0;
    Derv = 0.0;
    LastUpdate = time_now_sec();
    if (loglevel == 3) {
        ESP_LOGI(TAG, "setTarget: new target: %.0f at LastUpdate: %.0f", setPoint, LastUpdate);
    }
}

double pid::update(double Current, double SetPoint, int loglevel)
{
    setPoint = SetPoint;

    // ** P **
    Error = Current - setPoint;
    P = Kp * Error + 0.5;  // P = 1 for PB/2 under setPoint, P = 0 for PB/2 over setPoint

    // ** I **
    double dT = time_now_sec() - LastUpdate;
    Inter += Error * dT;
    Inter = fmax(Inter, -Inter_max);
    Inter = fmin(Inter, Inter_max);
    I = Ki * Inter;

    // ** D **
    Derv = (Current - Last) / dT;
    D = Kd * Derv;

    // ** PID **
    u = P + I + D;

    if (loglevel >= 1) {
        ESP_LOGI(TAG, "update: P: %.2f I: %.2f D: %.2f u: %.2f", P, I, D, u);
    }

    // Update for next cycle
    Last = Current;
    LastUpdate = time_now_sec();

    if (loglevel >= 1) {
        ESP_LOGI(TAG, "update: Grill: %.0f  Target: %.0f  LastUpdate: %.0f  u: %.2f", Current, setPoint, LastUpdate, u);
    }

    return u;
}

void pid::setGains(double PB, double Ti, double Td, int loglevel)
{
    CalculateGains(PB, Ti, Td, loglevel);
    Inter_max = fabs(0.5 / Ki);
    if (loglevel == 3) {
        ESP_LOGI(TAG, "New Gains (%f, %f, %f)", Kp, Ki, Kd);
    }
}
