// This #include statement was automatically added by the Particle IDE.
#include <ITEADLIB_Nextion.h>
#include "Particle.h"
#include "math.h"
#include <SparkJson.h>
#include "MAX31865.h"
#include "pid2.h"

// Forward declarations
void getDataHandler(const char *topic, const char *data);
void hopperInit(void);
void SetMode(void);
void handler(const char *topic, const char *data);
void tempsHandler(const char *topic, const char *data);
void ReadTemperatures(void);
void ReadParameters(void);
void DoMode(void);
void UpdateParameters(void);
void WriteParameters(void);
void setState(int, int); // changed 2nd variable from bool to int on 7/29/17 -- testing
int  getState(int);
void checkIgniter(void);
void DoAugerControl(void);
void DoControl(void);
void sendToLCD(uint8_t type,String index, String cmd);

/* Nextion Forward Declarations */
void t0PopCallback(void *ptr);
void b0PopCallback(void *ptr);
void b1PopCallback(void *ptr);
void t10PopCallback(void *ptr);
void t1PopCallback(void);
void t2PopCallback(void);
void t3PopCallback(void);
/* END of Nextion Forward Declarations*/

#define TIMENOW Time.now() + 0.1;

const char *DELETE_PARAMETERS = "ParametersDELETE";
const char *PUBLISH_PARAMETERS = "ParametersHookBody";
const char *READ_PARAMETERS = "ParametersRead";

const char *DELETE_TEMPS = "TempsDELETE";
//const char *PUBLISH_TEMPS = "4TempsHookBody";
//const char *PUBLISH_TEMPS ="TempsHookBody"; 

/* Nextion variable **************/
USARTSerial& nexSerial = Serial1;       


NexButton bt0 = NexButton(1, 6, "bt0");  // Fan light
NexButton bt1 = NexButton(1, 10, "bt1"); // Igniter light
NexButton bt2 = NexButton(1, 9, "bt2"); // Auger light

NexButton bt3 = NexButton(1, 12, "bt3"); // Off - Start button
NexButton bt4 = NexButton(1, 13, "bt4"); // Mode - Smoke button
NexButton bt5 = NexButton(1, 14, "bt5"); // Mode - Ignite button
NexButton bt6 = NexButton(1, 15, "bt6"); // Mode - Hold button
NexButton bt7 = NexButton(1, 16, "bt7"); // Mode - Shutdown button

NexText t0 = NexText(1, 7, "t0");      /* Declare a text object for Target temp of the grill [page id:1, component id:8, component name: "t0"]. */
NexButton b0 = NexButton(1, 11, "b0"); /* Up ++ target temp */
NexButton b1 = NexButton(1, 8, "b1");  /* Down -- target temp */

NexText t10 = NexText(1, 6, "t10");     /* Declare a text object for Mode of the grill, default is OFF [pagid:1, component id:6, component name: "t10"]. */

NexText t1 = NexText(1, 2, "t1");       /* Grill Temp object on Nextion display */
NexText t2 = NexText(1, 3, "t2");       /* Meat 1 Temp object on Nextion display */
NexText t3 = NexText(1, 4, "t3");       /* Meat 2 Temp object on Nextion display */

char buffer[100] = {0};
char buffer1[100] = {0};

/* Register object t0, b0, b1, t10 to the Nextion touch event list. */
NexTouch *nex_listen_list[] = 
{
    &t0,
    &b0,
    &b1,
    &t10,
    NULL
};
/* Nextion variable end **********/

/********************************************************************************/
int debug = 0; /* set to 1 to get more debug information to the Serial port */
/********************************************************************************/

//char text[256];

String deviceName;

int READY = 0;
int ResetFIREBASE = 0;
int kount = 0; // used for testing

int fanPin = D4;        // if you change this, be sure to change it also in SetState()
int igniterPin = D5;    // if you change this, be sure to change it also in SetState()
int augerPin = D6;      // if you change this, be sure to change it also in SetState()

int cs = A2;    //Grill
int csm1 = A1;  //Meat1
int csm2 = A0;  //Meat2

int TempInterval = 3;            // #Frequency to record temperatures
int TempRecord = 60;             // #Period to record temperatures in memory
int ParametersInterval = 6;      //#Frequency to write parameters
int PIDCycleTime = 20;           //#Frequency to update control loop - usually 20
int ReadParametersInterval = 20; //  #Frequency to poll web for new parameters
int ReadProgramInterval = 60;    // #Freqnency to poll web for new program
double u_min = 0.15, u_max = 1.0;
int igniterTemperature = 100;
int On, Off;
int ShutdownTime = 10 * 60;

double toggleTimeAuger = 0.0;
double toggleTimeIgniter = 0.0;
double toggleTimeFan = 0.0;
double toggleTimeTemps = 0.0;

// TEMPS
float Temps[3] = {0};
float T1=0, T2=0, T3=0;
//double  tyme;
double TT; //Target Temp at the time the temps were taken

int modeState = 0;

//Parameter Variables
bool aug = 0;
int Cycle = 20.0;
bool fan = 0;
bool ign = 0;
double LReadPgm;
double LReadWeb;
double LWritten;
char mode[9] = "Off"; // possible modes: Off, Start, Smoke, Ignite, Hold, Shutdown
int PB = 60.0;
int PMode = 2.0;
bool pgm = false;
double PToggle;
int target = 225; // normally initially set to 225
int Td = 45.0;
int Ti = 180;
float u = 0.15;
//double uPID = 0.15;

//New Parameter Variables
bool newaug = 0;
int newCycle = 20.0;
bool newfan = 0;
bool newign = 0;
double newLReadPgm;
double newLReadWeb;
double newLWritten;
char newmode[9] = "Off";
int newPB = 60.0;
int newPMode = 2.0;
bool newpgm = false;
double newPToggle;
int newtarget = 225;
int newTd = 45.0;
int newTi = 180;
float newu = 0.15;

double LCalcula = 0;

//variables for the Grill temperature dropping more than XX degrees while in a cook... then we want to make a phone call or maybe send a text message!
bool tempMonitorOn = false;
int tempDropOnTemp = 115;      /* The grill must reach this temp before temp drop monitoring is enabled */
double tempDropOnTime;        /* The 1st timestamp when we have reached or exceeded the target temp and grill temp monitoring should start from this point. */
int tempMonitorVariance = 25; /* grill temp degrees of allowable drop in temp before we make a phone call */
int tempDropInterval = 60;    /* the interval at which we check to see if there is a grill temp drop */
int sendmessage = 0;
double sendmessageTIME;

//initialize PID
pid myPID(PB, Ti, Td);

//initialize MAX31865
MAX31865 myMAX31865(cs);

void setup()
{
    Particle.subscribe("spark/", handler);
    Particle.publish("spark/device/name");

    Particle.subscribe("hook-response/ParametersRead", getDataHandler);
    
    //Particle.subscribe("SSE-TempsHookBody", tempsHandler);

    Particle.variable("msg", &sendmessage, INT);

    Serial.begin(9600);
    Particle.syncTime();
    Time.zone(-6); //set to CST

    pinMode(fanPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(igniterPin, OUTPUT);

    pinMode(cs, OUTPUT);
    pinMode(csm1, OUTPUT);
    pinMode(csm2, OUTPUT);

    delay(1000);
    Serial.println("\r\n\r\n\r\n*************************************** P R O G R A M    B E G I N ************************************************");
    Serial.println(Time.timeStr());
    Serial.println(Time.format(TIME_FORMAT_ISO8601_FULL));

    /* Nextion Display code ********************************************************************************************************/
    /* Set the baudrate which is for debug and communicate with Nextion screen. */
    nexInit();
    /* Register the pop event callback function of the current text component. */
    t0.attachPop(t0PopCallback);
    /* Register the pop event callback function of the current button0 component. */
    b0.attachPop(b0PopCallback);
    /* Register the pop event callback function of the current button1 component. */
    b1.attachPop(b1PopCallback);
    /* Register the pop event callback function of the current Mode component. */
    t10.attachPop(t10PopCallback);
    //    /* Register the pop event callback function of the current Grill Temp component. */
    //   t1.attachPop(t1PopCallback);

    dbSerialPrintln("setup done");
    /* END Nextion Display code ****************************************************************************************************/

    LReadPgm = Time.now();
    LReadWeb = Time.now();


    delay(1000); // just 1 second to chill...
    Serial.println("Finishing SETUP in 1 second, awaiting command from the Pellet Pirate!");
    READY = 1; // now we are ready for the LOOP to start churning... all setup has run

}

void loop()
{
    /* Nextion Code -- When a pop or push event occured every time, the corresponding component[right page id and component id] in touch event list will be asked. */
    nexLoop(nex_listen_list);
     /* END Nextion Code */

    if (READY == 1 && modeState != 0)
    {

        //Record Temperatures
        ReadTemperatures();
        nexLoop(nex_listen_list);

        //Display Grill Temperature
        t1PopCallback();
        t2PopCallback();
        t3PopCallback();
        nexLoop(nex_listen_list); //added since others had this after them

        //Check for new parameters that may have been written from the Nextion Touch Display or Web Program into Firebase
        ReadParameters();
        nexLoop(nex_listen_list);

        // Do Mode
        DoMode();
    }
}

void ReadTemperatures()
{
    double time;

    for (int i = 0; i < 3; i++)
    {
        if (i == 0)
        {
            cs = A2;
        } //Grill
        if (i == 1)
        {
            cs = csm1;
        } //Meat1
        if (i == 2)
        {
            cs = csm2;
        } //Meat2

        Temps[i] = myMAX31865.get_Temp(cs);
        if (i == 2)
        {
            TT = target;
            time = Time.now();
            time = time * 1000; // multiply by 1000 to make sure its a unix epoch timestamp that is 13 digits (the multiplication by 1000 basically adds millis to the epoch time as 000, needed by front end web program graph)
    
           if (Temps[0] > 0 && Temps[0] < 500) {T1 = Temps[0];} else {T1 = 0.0;} //force bad temps from MAX31865 to be 0.0 degrees - recode this when MAX31865 is fixed
           if (Temps[1] > 0 && Temps[1] < 500) {T2 = Temps[1];} else {T2 = 0.0;} //force bad temps from MAX31865 to be 0.0 degrees - recode this when MAX31865 is fixed
           if (Temps[2] > 0 && Temps[2] < 500) {T3 = Temps[2];} else {T3 = 0.0;} //force bad temps from MAX31865 to be 0.0 degrees - recode this when MAX31865 is fixed
           //T1 = Temps[0], T2 = Temps[1], T3 = Temps[2];
        }
    }
    
    // Record Temperatures in "Firebase"
    if ((Time.now() - toggleTimeTemps > TempInterval) && ResetFIREBASE == 1)
    {
             char qT[128];
             snprintf(qT, sizeof(qT), "{\"T1\":%.6f,\"T2\":%.6f,\"T3\":%.6f,\"TT\":%.0f,\"time\":%.0f,\"n\":\"%s\"}", T1, T2, T3, TT, time, deviceName.c_str());
             Particle.publish("sse-Temps", qT, PRIVATE);
    
        toggleTimeTemps = Time.now();
        Serial.printf("%s ReadTemperatures - Grill: %.1f   Meat1: %.1f   Meat2: %.1f   Time:%.0f\r\n", Time.timeStr().c_str(), T1, T2, T3, toggleTimeTemps);

        // IFTTT Logic to send text message or phone call if temp drops below a threshhold set in variables above
        if ((T1 >= tempDropOnTemp) && tempMonitorOn == false)
        {
            tempMonitorOn = true;
            tempDropOnTime = Time.now();
            Serial.printf("%s *** ALERT *** We are IN a Cook and Grill Temp is now being monitored for Target: %.1f and Grill: %.1f as of time:%.0f\r\n", Time.timeStr().c_str(), target, T1, tempDropOnTime);
            //Serial.println(Time.timeStr());
        }

        if ((tempMonitorOn == true) && (Time.now() - tempDropOnTime > tempDropInterval) && (target - T1 >= tempMonitorVariance))
        {
            Serial.printf("****** MAKING PHONE CALL and SENDING SMS ****** temp is dropping - Target: %.1f and Grill: %.1f as of time:%.0f\r\n", target, T1, tempDropOnTime);
            tempMonitorOn = false;
            sendmessage = 1; // this will TRIGGER the Phone call and SMS message through IFTTT
            sendmessageTIME = Time.now();
        }

        if (((Time.now() - sendmessageTIME) > (tempDropInterval + 15)) && sendmessage == 1)
        {
            sendmessage = 0; // this should turn off the IFTTT trigger
            Serial.printf("%s RESET sendmessage flag for IFTTT trigger to OFF\r\n", Time.timeStr().c_str());
        }
    }
}

void ResetFirebase()
{
    char qDELETE[64];
    snprintf(qDELETE, sizeof(qDELETE), "{\"n\":\"%s\"}", deviceName.c_str());
    Particle.publish(DELETE_TEMPS, qDELETE, PRIVATE);
    kount = 0;
    delay(300);

    char pDELETE[64];
    snprintf(pDELETE, sizeof(pDELETE), "{\"n\":\"%s\"}", deviceName.c_str());
    Particle.publish(DELETE_PARAMETERS, pDELETE, PRIVATE);
    delay(300);
    Serial.printf("%s ResetFirebase is done!\r\n", Time.timeStr().c_str());

    ResetFIREBASE = 1;
}

void ReadParameters()
{
    if ((Time.now() - LReadWeb) >= ReadParametersInterval)
    {
        LReadWeb = TIMENOW;
        char pREAD[255];
        snprintf(pREAD, sizeof(pREAD), "{\"n\":\"%s\"}", deviceName.c_str());
        Particle.publish(READ_PARAMETERS, pREAD, PRIVATE);
        UpdateParameters();
    }
}

void UpdateParameters()
{
    bool DoINeedtoWriteParameters = false;
    //loop through new parameters and see what changed
    if (target != newtarget)
    {
    
        myPID.setTarget(newtarget);
        target = newtarget;
        Serial.println("UpdateParameters - newtarget!");
        DoINeedtoWriteParameters= true;
    }
    if (PB != newPB || Ti != newTi || Td != newTd)
    {
        PB = newPB;
        Ti = newTi;
        Td = newTd;
        myPID.setGains(PB, Ti, Td);
        Serial.println("UpdateParameters - newPB or newTI or newTD!");
        DoINeedtoWriteParameters= true;
    }
    if (PMode != newPMode)
    {
        PMode = newPMode;
        SetMode();
        Serial.println("UpdateParameters - newPMode!");
        DoINeedtoWriteParameters= true;
    }
    if (strcmp(mode, newmode) != 0)
    {
        Serial.printf("%s UpdateParameters - mode(%s) and newmode:(%s) are different\r\n", Time.timeStr().c_str(), mode, newmode);
        strcpy(mode, newmode); // should copy newmode into the mode variable
        SetMode();
        Serial.println("UpdateParameters - newmode!");
        DoINeedtoWriteParameters= true;
    }
    if (pgm != newpgm)
    {
        float timeholder;
        timeholder = Time.now();
        Serial.printf("check on this...not sure timeholder is working in UpdateParameters, time is: %f", timeholder);
        pgm = newpgm;
        LReadPgm = (timeholder - 10000 + .01);
        Serial.println("UpdateParameters - newpgm!");
            DoINeedtoWriteParameters= true;
        //TO DO .... need to finish this when you get to the PROGRAM coding...
        //Program = GetProgram(Parameters, Program)
        //Parameters = SetProgram(Parameters, Program)
        //break # Stop processing new parameters
    }
    if(DoINeedtoWriteParameters)
    {
        WriteParameters();
        Serial.printf("%s ***** DoINeedToWriteParameters just fired...*****\r\n", Time.timeStr().c_str());
    }
    else { Serial.printf("%s Nothing in Parameters changed this time...\r\n", Time.timeStr().c_str());}
}

void WriteParameters()
{
        aug = digitalRead(augerPin);
        fan = digitalRead(fanPin);
        ign = digitalRead(igniterPin);
        //Serial.printf("WriteParameters: FIA status: Fan: %d  Igniter: %d  Auger: %d\r\n", fan, ign, aug);
        LWritten = TIMENOW;

        char qP[255];
 
        snprintf(qP, sizeof(qP), "{\"Cycle\":%d,\"LReadPgm\":%.1f,\"LReadWeb\":%.1f,\"LWritten\":%.1f,\"PB\":%d,\"PMode\":%d,\"PToggle\":%.1f,\"Td\":%d,\"Ti\":%d,\"aug\":%s,\"fan\":%s,\"ign\":%s,\"mode\":\"%s\",\"pgm\":%s,\"target\":%d,\"u\":%.2f,\"n\":\"%s\"}",
                 Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, aug ? "true" : "false", fan ? "true" : "false", ign ? "true" : "false", mode, pgm ? "true" : "false", target, u, deviceName.c_str());

        Particle.publish(PUBLISH_PARAMETERS, qP);
        Serial.printf("%s WriteParameters: C:%d LRP:%.1f LRW:%.1f LW:%.1f PB:%d PM:%d PT:%.1f Td:%d Ti:%d A:%s F:%s I:%s Mode:%s Pgm:%s TT:%d u:%.2f\r\n", Time.timeStr().c_str(), Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, aug ? "true" : "false", fan ? "true" : "false", ign ? "true" : "false", mode, pgm ? "true" : "false", target, u);
}

void SetMode()
{
    if (strcmp(mode, "Off") == 0)
    {
        modeState = 0;
        Serial.println("SetMode - Off");
        hopperInit();
        Serial.printf("%s SetMode, just finished hopperInit\r\n", Time.timeStr().c_str());
    }
    else if (strcmp(mode, "Start") == 0)
    {
        ResetFirebase();
        modeState = 1;
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        setState(igniterPin, TRUE);
        Serial.printf("%s SetMode in START after all pins set TRUE: Fan: %d  Igniter: %d  Auger: %d\r\n", Time.timeStr().c_str(),digitalRead(fanPin), digitalRead(igniterPin), digitalRead(augerPin));
        Cycle = 15 + 45;
        u = 15.0 / (15.0 + 45.0); //P0
        Serial.printf("%s SetMode - Start : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Smoke") == 0)
    {
        modeState = 2;
        //sendToLCD(1, "t10", mode);
        sendCommand("click bt4,1"); //activate press event of component bt4 - the SMOKE button on the display
        sendCommand("click bt4,0"); //activate press release event of component bt4 - the SMOKE button on the display
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        checkIgniter();
        On = 15;
        Off = 45 + PMode * 10;
        Cycle = On + Off;
        u = On / (On + Off);
        Serial.printf("%s SetMode - Smoke : u = %.2f\r\n", Time.timeStr().c_str(), u);

    }
    else if (strcmp(mode, "Ignite") == 0)
    {
        modeState = 3;
        sendCommand("click bt5,1"); //activate press event of component bt5 - the Ignite button on the display
        sendCommand("click bt5,0"); //activate press release event of component bt5 - the Ignite button on the display
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        setState(igniterPin, TRUE);
        On = 15;
        Off = 45 + (PMode * 10); //http://tipsforbbq.com/Definition/Traeger-P-Setting
        Cycle = On + Off;
        u = On / (On + Off);
        Serial.printf("%s SetMode - Ignite : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Hold") == 0)
    {
        modeState = 4;
        Serial.println("SetMode - Hold");
        sendCommand("click bt6,1"); //activate press event of component bt6 - the Hold button on the display
        sendCommand("click bt6,0"); //activate press release event of component bt6 - the Hold button on the display       
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        checkIgniter();
        Cycle = PIDCycleTime;
        u = u_min; //Set to maintenance level
        Serial.printf("%s SetMode - Hold : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Shutdown") == 0)
    {
        modeState = 5;
        Serial.println("SetMode - Shutdown");
        sendCommand("click bt7,1"); //activate press event of component bt7 - the Shutdown button on the display
        sendCommand("click bt7,0"); //activate press release event of component bt7 - the Shutdown button on the display               
        hopperInit();
        setState(fanPin, TRUE);
    }

    //Serial.print("SetMode: I think I found it... fixing to WriteParameters");
    WriteParameters();
    //Serial.printf("I found it... WriteParameters done because SetMode is in Setup procedure above !!!!");

    if (debug == 1){Serial.println("SetMode end --- WriteParameters - done!");}
}

void DoMode()
{
    if (strcmp(mode, "Off") == 0)
    {
        return;
    }

    else if (strcmp(mode, "Shutdown") == 0)
    {
        if ((Time.now() - toggleTimeFan) > ShutdownTime)
        {
            sendCommand("click bt3,1"); //activate press release event of component bt3 - the Off/On button of on the display
            sendCommand("click bt3,0"); //activate press release event of component bt3 - the Off/On button on the display
            sendToLCD(2, "bt0", "0"); // turn off Fan button
            sendToLCD(2, "bt7", "0"); // turn off Shutdown mode button
            strcpy(mode, "Off");
            SetMode();
        }
    }

    else if (strcmp(mode, "Start") == 0)
    {
        DoAugerControl();
        setState(igniterPin, TRUE);
        if (Temps[0] > 120)
        {
            strcpy(mode, "Hold");
            SetMode();
        }
    }

    else if (strcmp(mode, "Smoke") == 0)
    {
        DoAugerControl();
    }

    else if (strcmp(mode, "Ignite") == 0)
    {
        DoAugerControl();
        setState(igniterPin, TRUE);
    }

    else if (strcmp(mode, "Hold") == 0)
    {
        DoControl();
        DoAugerControl();
    }
}

void DoAugerControl()
{
    //Auger currently on AND TimeSinceToggle > Auger On Time
    if (digitalRead(augerPin) && ((Time.now() - toggleTimeAuger) > (Cycle * u * 2))) // 12162017 added the *2 to cycle*u to increase the ON time for Auger to get more pellets in kettle!
    {
        int TimeSince1 = Time.now() - toggleTimeAuger;
        //if (debug == 1)
        //{
        Serial.printf("%s DoAugerControl - in TOP of first if - Auger ON! timenow:%f  toggletimeauger:%d  Cycle:%f u:%d\r\n", Time.timeStr().c_str(), toggleTimeAuger, Cycle, u, TimeSince1);
        //}
        if (u <= 1.0) // added the = statement 02272017 to stop the violent looping of this function when PID is == 1.0
        {
            setState(augerPin, FALSE);
            WriteParameters();
        }

        checkIgniter();
    }

    //Auger currently off AND TimeSinceToggle > Auger Off Time
    if (!digitalRead(augerPin) && ((Time.now() - toggleTimeAuger) > ((Cycle * (1 - u)))))
    {
        int TimeSince2 = Time.now() - toggleTimeAuger;
        //if (debug == 1)
        //{
        Serial.printf("%s DoAugerControl - in TOP of second if - Auger OFF! timenow:%f  toggletimeauger:%d  Cycle:%f u:%d\r\n", Time.timeStr().c_str(), toggleTimeAuger, Cycle, u, TimeSince2);
        //}
        setState(augerPin, TRUE);
        checkIgniter();
        WriteParameters();
    }
}

void checkIgniter()
{
    //Check if igniter needed
    if (Temps[0] < igniterTemperature)
    {
        setState(igniterPin, TRUE);
    }
    else
    {
        setState(igniterPin, FALSE);
    }

    //Check if the igniter has been running too long
    if ((Time.now() - toggleTimeIgniter) > 1200 && digitalRead(igniterPin))
    {
        Serial.println("**SAFETY FIRST** - Disabling igniter due to timeout");
        setState(igniterPin, FALSE);
        strcpy(mode, "Shutdown");
        SetMode();
    }
}

void DoControl()
{
    double old_u;

    //Serial.printf("DoControl: Time.now: %f  myPID.LastUpdate: %f  difference %d  Cycle: %d  u now:%.2f\r\n", Time.now(), myPID.LastUpdate, Time.now() - myPID.LastUpdate, Cycle, u);
    if ((Time.now() - myPID.LastUpdate) > Cycle)
    {
        u = myPID.update(Temps[0], target);
        old_u = u;
        u = max(u, u_min);
        u = min(u, u_max);

        Serial.printf("%s DoControl IF Statement - through u max min calculation old_u:%.2f and u:%.2f\r\n", Time.timeStr().c_str(), old_u, u);

        if (debug == 1)
        {
            Serial.printf("%s DoControl - setting PID u value: %f\r\n", Time.timeStr().c_str(), u);
        }

        // To Do ... write code here to STAMP out Program Control data to Firebase

        WriteParameters();
    }
}

void hopperInit()
{
    //initialize hopper assembly
    pinResetFast(fanPin); // initialize to LOW
    pinMode(fanPin, OUTPUT);
    fan = digitalRead(fanPin);
    toggleTimeFan = TIMENOW;

    pinResetFast(igniterPin); // initialize to LOW
    pinMode(igniterPin, OUTPUT);
    ign = digitalRead(igniterPin);
    toggleTimeIgniter = TIMENOW;

    pinResetFast(augerPin); // initialize to LOW
    pinMode(augerPin, OUTPUT);
    aug = digitalRead(augerPin);
    toggleTimeAuger = TIMENOW;

}

int getState(int pin)
{
    return pinReadFast(pin);
}

void setState(int pin, int newState)  // changed newState from bool to int
{
    int currentState = getState(pin);
    char pinState[10];
    
     if (currentState != newState)
    {
        digitalWrite(pin, newState);
        switch (pin)
        {
        case (D4):
            toggleTimeFan = TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt0", pinState);
            //Serial.printf("setState: toggling Fan: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));           
            Serial.printf("%s setState: toggling Fan: %d\r\n", Time.timeStr().c_str(), newState);
            fan = newState;
            break;
        case (D5):
            toggleTimeIgniter = TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt1", pinState);
            //Serial.printf("setState: toggling Igniter: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));
            Serial.printf("%s setState: toggling Igniter: %d\r\n", Time.timeStr().c_str(), newState);
            ign = newState;
            break;
        case (D6):
            toggleTimeAuger = TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt2", pinState);
            //Serial.printf("setState: toggling Auger: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));            
            Serial.printf("%s setState: toggling Auger: %d\r\n", Time.timeStr().c_str(), newState);
            aug = newState;
            break;
        }
    }
}

void handler(const char *topic, const char *data)
{
    deviceName = String(data);
    //ResetFirebase();
}

void tempsHandler(const char *event, const char *data)
{
    //Serial.println(data);
}

void getDataHandler(const char *topic, const char *data)
{
    StaticJsonBuffer<1024> jsonBuffer;
    char *mutableCopy = strdup(data);
    JsonObject &root = jsonBuffer.parseObject(mutableCopy);

    Serial.printf("%s %s\r\n", Time.timeStr().c_str(), data);
    free(mutableCopy);

    if (!root.success())
    {
        Serial.println("parse failed");
    }
    else
    {
        newCycle = root["Cycle"];
        newLReadPgm = root["LReadPgm"];
        newLReadWeb = root["LReadWeb"];
        newLWritten = root["LWritten"];
        newPB = root["PB"];
        newPMode = root["PMode"];
        newPToggle = root["PToggle"];
        newTd = root["Td"];
        newTi = root["Ti"];
        newaug = root["aug"];
        newfan = root["fan"];
        newign = root["ign"];
        strcpy(newmode, (const char *)root["mode"]);
        newpgm = root["pgm"];
        newtarget = root["target"];
        newu = root["u"];

        //Serial.printf("%s Params Read: %d %.1f %.1f %.1f %d %d %.1f %d %d %s %s %s %s %s %d %.2f\r\n", Time.timeStr().c_str(), newCycle, newLReadPgm, newLReadWeb, newLWritten, newPB, newPMode, newPToggle, newTd, newTi, newaug, newfan, newign, newmode, newpgm, newtarget, newu);
    }

    UpdateParameters(); // process what we just got from Firebase read!

}

/* Nextion Code *********************************************************************************************/
void t0PopCallback(void *ptr)   /* Text component pop callback function. */
{
    dbSerialPrintln("t0PopCallback");
    t0.setText("225");
}

void b0PopCallback(void *ptr)   /* Taget temp +5 degrees every time the Up+ button is released. */
{
    uint16_t number;
    
    dbSerialPrintln("b0PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t0.getText(buffer, sizeof(buffer));
    
    number = atoi(buffer);
    number += 5;
    newtarget = number;

    memset(buffer, 0, sizeof(buffer));
    itoa(number, buffer, 10);
    
    t0.setText(buffer);

    UpdateParameters();
}

void b1PopCallback(void *ptr)   /* In this example,the value of the text component will minus 5 degress every time when button1 is released. */
{
    uint16_t number;
    
    dbSerialPrintln("b1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t0.getText(buffer, sizeof(buffer));
    
    number = atoi(buffer);
    number -= 5;
    newtarget = number;

    //Serial.printf("********************** in - 5 temps ***********************************\r\n");

    memset(buffer, 0, sizeof(buffer));
    itoa(number, buffer, 10);
    
    t0.setText(buffer);

    UpdateParameters();
}

void t10PopCallback(void *ptr)   /* Text component pop callback function for Mode. */
{
    dbSerialPrintln("t10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t10.getText(buffer, sizeof(buffer));

    if(strcmp("Off", buffer) == 0) // on-off button pressed ... if set to "Off" then i need to force Shutdown if grill is currently HOT... so shutdown process runs to cool grill
    { 
        if(T1 > 115) 
        {
          Serial.printf("%s T10PopCallback - On/Off button pressed while in cook, setting mode to Shutdown for proper cool down procedure ", Time.timeStr().c_str());
          strcpy(buffer, "Shutdown");
          t10.setText(buffer); 
        }
    }
    strcpy(newmode, buffer);
    Serial.printf("%s T10PopCallback - newmode is: %s and buffer length is: %d\r\n", Time.timeStr().c_str(), newmode, strlen(buffer));
        
    UpdateParameters();
}

void t1PopCallback(void)
{
    uint16_t integer_temp, decimal_temp;

    integer_temp = T1;
    decimal_temp = ((T1 - integer_temp)*10);
    //Serial.printf("T1PopCallback - integer is: %d and the decimal is: %d\r\n", integer_temp, decimal_temp);
 
    memset(buffer, 0, sizeof(buffer));
    memset(buffer1, 0, sizeof(buffer));
    itoa(integer_temp, buffer, 10);
    itoa(decimal_temp, buffer1, 10);

    strcat(buffer, ".");
    strcat(buffer, buffer1);
     
    t1.setText(buffer);
}

void t2PopCallback(void)
{
    uint16_t integer_temp, decimal_temp;

    integer_temp = T2;
    decimal_temp = ((T2 - integer_temp)*10);
     
    memset(buffer, 0, sizeof(buffer));
    memset(buffer1, 0, sizeof(buffer));
    itoa(integer_temp, buffer, 10);
    itoa(decimal_temp, buffer1, 10);

    strcat(buffer, ".");
    strcat(buffer, buffer1);
     
    t2.setText(buffer);
}

void t3PopCallback(void)
{
    uint16_t integer_temp, decimal_temp;

    integer_temp = T3;
    decimal_temp = ((T3 - integer_temp)*10);
     
    memset(buffer, 0, sizeof(buffer));
    memset(buffer1, 0, sizeof(buffer));
    itoa(integer_temp, buffer, 10);
    itoa(decimal_temp, buffer1, 10);

    strcat(buffer, ".");
    strcat(buffer, buffer1);
     
    t3.setText(buffer);
}

void sendToLCD(uint8_t type,String index, String cmd)
{
	if (type == 1 ){
		Serial1.print(index);
		Serial1.print(".txt=");
		Serial1.print("\"");
		Serial1.print(cmd);
		Serial1.print("\"");
	}
	else if (type == 2){
		Serial1.print(index);
		Serial1.print(".val=");
		Serial1.print(cmd);
	}
	else if (type == 3){
		Serial1.print(index);
		Serial1.print(".picc="); 
		Serial1.print(cmd);
	}
	else if (type ==4 ){
		Serial1.print("page ");
		Serial1.print(cmd);
	}

	Serial1.write(0xff);
	Serial1.write(0xff);
	Serial1.write(0xff);
	
	delay(25); // changed from 50
}

/* END Nextion Code *******************************************************************************************/
