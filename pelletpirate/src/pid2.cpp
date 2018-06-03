#include "pid2.h"
#include "math.h"
#include "Particle.h"

/*Constructor **************************************************************
 *    The parameters specified here are those for for which we can't set up 
 *    reliable defaults. The user needs to set them.
 ***************************************************************************/
pid::pid(double PB, double Ti, double Td, int loglevel)
{
	//# PID controller based on proportional band in standard PID form https://en.wikipedia.org/wiki/PID_controller#Ideal_versus_standard_PID_form
	//# u = Kp (e(t)+ 1/Ti INT + Td de/dt)
	//# PB = Proportional Band
	//# Ti = Goal of eliminating in Ti seconds
	//# Td = Predicts error value at Td in seconds
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
	if (loglevel == 3) {Log.info("%s PB: %f  Ti: %f  Td: %f --> Kp: %f  Ki: %f  Kd: %f\r\n", Time.timeStr().c_str(), PB, Ti, Td, Kp, Ki, Kd);}
}

void pid::setTarget(double SetPoint, int loglevel)
{
	setPoint = SetPoint;
	Error = 0.0;
	Inter = 0.0;
	Derv = 0.0;
	LastUpdate = Time.now();
	if (loglevel == 3) {Log.info("%s Pid2 setTarget has processed a New Target: %.0f at LastUpdate: %.0f\r\n", Time.timeStr().c_str(), setPoint, LastUpdate);}
}

double pid::update(double Current, double SetPoint, int loglevel)
{
	setPoint = SetPoint;
	
	//** P **
	Error = Current - setPoint;
	P = Kp * Error + 0.5; //P = 1 for PB/2 under setPoint, P = 0 for PB/2 over setPoint

	//** I **
	double dT = Time.now() - LastUpdate;
	Inter += Error * dT;
	Inter = fmax(Inter, -Inter_max);
	Inter = fmin(Inter, Inter_max);
	I = Ki * Inter;

	//** D **
	Derv = (Current - Last) / dT;
	D = Kd * Derv;

	// ** PID **
	u = P + I + D;

	//Update for next cycle
	Last = Current;
	LastUpdate = Time.now();

	if(loglevel == 3){Log.info("%s Pid2 update has processed Grill Temp: %.0f with Target Temp: %.0f at LastUpdate time: %.0f returning u: %.2f\r\n", Time.timeStr().c_str(), Current, setPoint, LastUpdate, u);}

	return u;
}

void pid::setGains(double PB, double Ti, double Td, int loglevel)
{
	CalculateGains(PB, Ti, Kd, loglevel);
	Inter_max = fabs(0.5 / Ki);
	if(loglevel == 3) {Log.info("%s New Gains (%f, %f, %f)\r\n", Time.timeStr().c_str(), Kp, Ki, Kd);}
}
