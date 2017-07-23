// This #include statement was automatically added by the Particle IDE.
#include <ITEADLIB_Nextion.h>
//#include "Nextion.h"



#include "Particle.h"
#include "math.h"
#include <SparkJson.h>
#include "MAX31865.h"
//#include "MAX31865.h"
#include "OLED.h"
#include "pid2.h"

// Forward declarations
void getDataHandler(const char *topic, const char *data);
void hopperInit(void);
void SetMode(void);
void handler(const char *topic, const char *data);
void ReadTemperatures(void);
void ReadParameters(void);
void DoMode(void);
void UpdateParameters(void);
void WriteParameters(void);
void setState(int, bool);
bool getState(int);
void checkIgniter(void);
void DoAugerControl(void);
void DoControl(void);



/* Nextion Forward Declarations */
void t0PopCallback(void *ptr);
void b0PopCallback(void *ptr);
void b1PopCallback(void *ptr);
/* END of Nextion Forward Declarations*/



#define augerPin A7
#define fanPin A6
#define igniterPin D7

#define TIMENOW Time.now() + 0.1;

const char *DELETE_PARAMETERS = "ParametersDELETE";
const char *PUBLISH_PARAMETERS = "ParametersHookBody";
const char *CHECK_EVENT_NAME = "ParametersRead";

const char *DELETE_TEMPS = "TempsDELETE";
const char *PUBLISH_TEMPS = "4TempsHookBody";



/* Nextion variable **************/
USARTSerial& nexSerial = Serial1;
/*
 * Declare a text object [page id:0,component id:1, component name: "t0"]. 
 */
NexText t0 = NexText(1, 11, "t0");
/*
 * Declare a button object [page id:0,component id:2, component name: "b0"]. 
 */
NexButton b0 = NexButton(1, 2, "b0");

/*
 * Declare a button object [page id:0,component id:3, component name: "b1"]. 
 */
NexButton b1 = NexButton(1, 3, "b1");

char buffer[100] = {0};
/*
 * Register object t0, b0, b1, to the touch event list.  
 */
NexTouch *nex_listen_list[] = 
{
    &t0,
    &b0,
    &b1,
    NULL
};
/* Nextion variable end **********/



/*******************/
int debug = 0; /* set to 1 to get more debug information to the Serial port */
/******************/

char text[256];

String deviceName;

int READY = 0;
int ResetFIREBASE = 0;
int kount = 0; // used for testing

int cs = A2;
int csm1 = A1;
//int     drdy = A6;

int TempInterval = 6;            // #Frequency to record temperatures
int TempRecord = 60;             // #Period to record temperatures in memory
int ParametersInterval = 4;      //#Frequency to write parameters
int PIDCycleTime = 20;           //#Frequency to update control loop - usually 20
int ReadParametersInterval = 10; //  #Frequency to poll web for new parameters
int ReadProgramInterval = 60;    // #Freqnency to poll web for new program
double u_min = 0.15, u_max = 1.0;
int igniterTemperature = 78;
int On, Off;
int ShutdownTime = 10 * 60;

double toggleTimeAuger = 0.0;
double toggleTimeIgniter = 0.0;
double toggleTimeFan = 0.0;
double toggleTimeTemps = 0.0;

// TEMPS
float Temps[3] = {0};
float T1, T2, T3;
//double  tyme;
double TT; //Target Temp at the time the temps were taken

int modeState = 0;

//Parameter Variables
bool aug = false;
int Cycle = 20.0;
bool fan = false;
bool ign = false;
double LReadPgm;
double LReadWeb;
double LWritten;
char mode[9] = "Off"; // possible modes: Off, Start, Smoke, Ignite, Hold, Shutdown
int PB = 60.0;
int PMode = 2.0;
bool pgm = false;
double PToggle;
int target = 90; // normally initially set to 225
int Td = 45.0;
int Ti = 180;
float u = 0.15;
//double uPID = 0.15;

//New Parameter Variables
bool newaug = false;
int newCycle = 20.0;
bool newfan = false;
bool newign = false;
double newLReadPgm;
double newLReadWeb;
double newLWritten;
char newmode[9] = "Off";
int newPB = 60.0;
int newPMode = 2.0;
bool newpgm = false;
double newPToggle;
int newtarget = 90;
int newTd = 45.0;
int newTi = 180;
float newu = 0.15;

double LCalcula = 0;

//variables for the Grill temperature dropping more than XX degrees while in a cook... then we want to make a phone call or maybe send a text message!
bool tempMonitorOn = false;
int tempDropOnTemp = 92;      /* The grill must reach this temp before temp drop monitoring is enabled */
double tempDropOnTime;        /* The 1st timestamp when we have reached or exceeded the target temp and grill temp monitoring should start from this point. */
int tempMonitorVariance = 25; /* grill temp degrees of allowable drop in temp before we make a phone call */
int tempDropInterval = 60;    /* the interval at which we check to see if there is a grill temp drop */
int sendmessage = 0;
double sendmessageTIME;

//initialize PID
pid myPID(PB, Ti, Td);

//initialize MAX31865
MAX31865 myMAX31865(cs);

//initialze OLED
OLED myOLED;

/*#################################################################################################
S E T U P
#################################################################################################*/
void setup()
{
    Particle.subscribe("spark/", handler);
    Particle.publish("spark/device/name");

    Particle.subscribe("hook-response/ParametersRead", getDataHandler, MY_DEVICES);

    Particle.variable("msg", &sendmessage, INT);

    Serial.begin(9600);
    Particle.syncTime();
    Time.zone(-5); //set to CST

    //pinMode (drdy, INPUT);
    pinMode(cs, OUTPUT);
    pinMode(csm1, OUTPUT);

    delay(3000);
    Serial.println("\r\n\r\n\r\n******************************************* P R O G R A M    B E G I N ****************************************************");
    Serial.println(Time.timeStr());
    Serial.println(Time.format(TIME_FORMAT_ISO8601_FULL));

    //initialize hopper assembly
    hopperInit();

    //Parameters initialization
    LReadPgm = TIMENOW;
    LReadWeb = TIMENOW;
    LWritten = TIMENOW;
    LCalcula = TIMENOW;
    //TT = Time.now();

    //PiSmoker PID
    myPID.setTarget(target);

    //initialize OLED
    myOLED.OLED_init();
    myOLED.targetTemp = target;
    strcpy(myOLED.MODE, mode);

    //Set mode
    SetMode();
    //myOLED.update_OLED_mode(mode, modeState);



/* Nextion Display code ********************************************************************************************************/
    /* Set the baudrate which is for debug and communicate with Nextion screen. */
    nexInit();
    /* Register the pop event callback function of the current text component. */
    t0.attachPop(t0PopCallback);
    /* Register the pop event callback function of the current button0 component. */
    b0.attachPop(b0PopCallback);
    /* Register the pop event callback function of the current button1 component. */
    b1.attachPop(b1PopCallback);
    dbSerialPrintln("setup done");
/* END Nextion Display code ****************************************************************************************************/

    delay(500); // just 1/2 a second to chill...
    READY = 1; // now we are ready for the LOOP to start churning... all setup has run

}

/*#################################################################################################
M A I N   L O O P
#################################################################################################*/

/* executes continuously after setup() runs */
void loop()
{
    //Read buttons
    myOLED.buttonPress();
    strcpy(newmode, myOLED.MODE);
    modeState = myOLED.ModeState;



    /* Nextion Code -- When a pop or push event occured every time, the corresponding component[right page id and component id] in touch event list will be asked. */
    nexLoop(nex_listen_list);
    /* END Nextion Code */



    if (READY == 1 && modeState != 0)
    {

        //Record Temperatures
        ReadTemperatures();

        newtarget = myOLED.targetTemp; // get any new target temp changes from the OLED button menu

        //Check for new parameters that may have been written from the LCD or Web Program into Firebase
        ReadParameters();

        // code Check for new program

        // Do Mode
        DoMode();
    }
}

void ReadTemperatures()
{
    double time;
    if ((Time.now() - toggleTimeTemps > TempInterval) && ResetFIREBASE == 1)
    {
        // Record Temperatures
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
                cs = A2;
            }

            Temps[i] = myMAX31865.get_Temp(cs);
            if (i == 2)
            {
                TT = target;
                //tyme = Time.now();
                time = Time.now();
                time = time * 1000; // multiply by 1000 to make sure its a unix epoch timestamp that is 13 digits (the multiplication by 1000 basically adds millis to the epoch time as 000, needed by front end web program graph)
                T1 = Temps[0], T2 = Temps[1], T3 = Temps[2];
            }
        }
        char qT[128];
        snprintf(qT, sizeof(qT), "{\"T1\":%.6f,\"T2\":%.6f,\"T3\":%.6f,\"TT\":%.0f,\"time\":%.0f,\"n\":\"%s\"}", T1, T2, T3, TT, time, deviceName.c_str());
        Particle.publish(PUBLISH_TEMPS, qT, PRIVATE);
        //Particle.publish("In IF statement on TempsWrite: ", String(sizeof(qT)) + " | " + strlen(qT), PRIVATE);
        toggleTimeTemps = Time.now();
        Serial.printf("ReadTemperatures - Grill: %.1f   Meat1: %.1f   Meat2: %.1f   Time:%.0f\r\n", T1, T2, T3, toggleTimeTemps);

        // IFTTT Logic to send text message or phone call if temp drops below a threshhold set in variables above
        if ((T1 >= tempDropOnTemp) && tempMonitorOn == false)
        {
            tempMonitorOn = true;
            tempDropOnTime = Time.now();
            Serial.printf("*** ALERT *** We are IN a Cook and Grill Temp is now being monitored for Target: %.1f and Grill: %.1f as of time:%.0f\r\n", target, T1, tempDropOnTime);
            Serial.println(Time.timeStr());
        }

        if ((tempMonitorOn == true) && (Time.now() - tempDropOnTime > tempDropInterval) && (target - T1 >= tempMonitorVariance))
        {
            Serial.printf("****** MAKING PHONE CALL and SENDING SMS ****** temp is dropping - Target: %.1f and Grill: %.1f as of time:%.0f\r\n", target, T1, tempDropOnTime);
            tempMonitorOn = false;
            sendmessage = 1; // this will TRIGGER the Phone call and SMS message through IFTTT
            sendmessageTIME = Time.now();
        }

        if ((Time.now() - sendmessageTIME) > (tempDropInterval + 15))
        {
            sendmessage = 0; // this should turn off the IFTTT trigger
            if (debug == 1)
            {
                Serial.printf("RESET sendmessage flag for IFTTT trigger to OFF\r\n");
            }
        }

        myOLED.update_OLED_temps(target, T1, T2, T3);
        myOLED.update_OLED_pid(u); // update the pid value on the OLED display .... really need to look at this and perhaps just force the update to OLED ... WHEN ... the PID value actually changes or after a timelapse calc!
        //Serial.println("DoMode -------- updating PID value in OLED display!\r\n");
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
    //Serial.printf("%f ResetFirebase - done!\n", Time.now());

    ResetFIREBASE = 1;
}

void ReadParameters()
{
    if ((Time.now() - LReadWeb) >= ReadParametersInterval)
    {
        LReadWeb = TIMENOW;
        //Particle.publish("**IN READ PARAMS**", "READ PARAMETERS - FIREBASE", PRIVATE);
        char pREAD[255];
        snprintf(pREAD, sizeof(pREAD), "{\"n\":\"%s\"}", deviceName.c_str());
        Particle.publish(CHECK_EVENT_NAME, pREAD, PRIVATE);
        //Particle.publish(CHECK_EVENT_NAME, "", PRIVATE);

        UpdateParameters();
        //Serial.printf("%f ReadParameters - done!\n", Time.now());
    }
}

void UpdateParameters()
//loop through new parameters and see what changed
{
    if (target != newtarget)
    {
        myPID.setTarget(newtarget);
        myOLED.update_OLED_target(newtarget);
        target = newtarget;
        WriteParameters();
        Serial.println("UpdateParameters - newtarget!");
    }
    else if (PB != newPB || Ti != newTi || Td != newTd)
    {
        PB = newPB;
        Ti = newTi;
        Td = newTd;
        myPID.setGains(PB, Ti, Td);
        WriteParameters();
        Serial.println("UpdateParameters - newPB or newTI or newTD!");
    }
    else if (PMode != newPMode)
    {
        PMode = newPMode;
        SetMode();
        WriteParameters();
        Serial.println("UpdateParameters - newPMode!");
    }
    else if (strcmp(mode, newmode) != 0)
    {
        strcpy(mode, newmode); // should copy newmode into the mode variable
        strcpy(myOLED.MODE, mode);
        SetMode();
        WriteParameters();
        Serial.println("UpdateParameters - newmode!");
    }
    else if (pgm != newpgm)
    {
        pgm = newpgm;
        LReadPgm = TIMENOW - 10000;
        Serial.println("UpdateParameters - newpgm!");
        //TO DO .... need to finish this when you get to the PROGRAM coding...
        //Program = GetProgram(Parameters, Program)
        //Parameters = SetProgram(Parameters, Program)
        //break # Stop processing new parameters
    }
}

void WriteParameters()
{
        aug = digitalRead(augerPin);
        fan = digitalRead(fanPin);
        ign = digitalRead(igniterPin);
        LWritten = TIMENOW;

        char qP[255];
 
        snprintf(qP, sizeof(qP), "{\"Cycle\":%d,\"LReadPgm\":%.1f,\"LReadWeb\":%.1f,\"LWritten\":%.1f,\"PB\":%d,\"PMode\":%d,\"PToggle\":%.1f,\"Td\":%d,\"Ti\":%d,\"aug\":%s,\"fan\":%s,\"ign\":%s,\"mode\":\"%s\",\"pgm\":%s,\"target\":%d,\"u\":%.2f,\"n\":\"%s\"}",
                 Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, aug ? "true" : "false", fan ? "true" : "false", ign ? "true" : "false", mode, pgm ? "true" : "false", target, u, deviceName.c_str());
 
        Particle.publish(PUBLISH_PARAMETERS, qP);
        Serial.printf("WriteParameters: C:%d LRP:%.1f LRW:%.1f LW:%.1f PB:%d PM:%d PT:%.1f Td:%d Ti:%d A:%s F:%s I:%s Mode:%s Pgm:%s TT:%d u:%.2f\r\n", Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, aug ? "true" : "false", fan ? "true" : "false", ign ? "true" : "false", mode, pgm ? "true" : "false", target, u);
        //Serial.printf("LWritten %.1f\r\n", LWritten);
}

void SetMode()
{
    //    Particle.publish("top of SetMode is: ", String(Parameters.mode) + " | " + String(strcmp(Parameters.mode, "Start", 5) == TRUE), PRIVATE);
    if (strcmp(mode, "Off") == 0)
    {
        modeState = 0;
        myOLED.ModeState = 0;
        Serial.println("SetMode - Off");
        //myOLED.update_OLED_mode(mode, modeState);
        hopperInit();
    }
    else if (strcmp(mode, "Start") == 0)
    {
        modeState = 1;
        Serial.println("SetMode - Start");
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        setState(igniterPin, TRUE);
        Cycle = 15 + 45;
        u = 15.0 / (15.0 + 45.0); //P0
    }
    else if (strcmp(mode, "Smoke") == 0)
    {
        modeState = 2;
        Serial.println("SetMode - Smoke");
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        checkIgniter();
        Cycle = 15 + 45;
        u = 15.0 / (15.0 + 45.0); //P0
    }
    else if (strcmp(mode, "Ignite") == 0)
    {
        modeState = 3;
        Serial.println("SetMode - Ignite");
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        setState(igniterPin, TRUE);
        On = 15;
        Off = 45 + (PMode * 10); //http://tipsforbbq.com/Definition/Traeger-P-Setting
        Cycle = On + Off;
        u = On / (On + Off);
    }
    else if (strcmp(mode, "Hold") == 0)
    {
        modeState = 4;
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        checkIgniter();
        //myOLED.update_OLED_mode(mode, modeState);
        Cycle = PIDCycleTime;
        u = u_min; //Set to maintenance level
        Serial.printf("SetMode - Hold : u = %.2f\r\n", u);
        strcpy(myOLED.MODE, mode); // update OLED mode
        myOLED.ModeState = 4;      // update OLED ModeState
    }
    else if (strcmp(mode, "Shutdown") == 0)
    {
        modeState = 5;
        Serial.println("SetMode - Shutdown");
        hopperInit();
        setState(fanPin, TRUE);
        strcpy(myOLED.MODE, mode); // update OLED mode
        myOLED.ModeState = 5;      // update OLED ModeState
        myOLED.update_OLED_mode(mode, modeState);
    }

    WriteParameters();
    if (debug == 1)
    {
        Serial.println("SetMode end --- WriteParameters - done!");
    }
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
            strcpy(mode, "Off");
            SetMode();
        }
    }

    else if (strcmp(mode, "Start") == 0)
    {
        DoAugerControl();
        setState(igniterPin, TRUE);
        if (Temps[0] > 90)
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
    if (digitalRead(augerPin) && ((Time.now() - toggleTimeAuger) > (Cycle * u)))
    {
        int TimeSince = Time.now() - toggleTimeAuger;
        if (debug == 1)
        {
            Serial.printf("DoAugerControl - in TOP of first if - Auger ON! %f %d %f %d\r\n", toggleTimeAuger, Cycle, u, TimeSince);
        }
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
        if (debug == 1)
        {
            Serial.println("DoAugerControl - in TOP of second if - Auger OFF!");
        }
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
        //digitalWrite(igniterPin, FALSE);
        setState(igniterPin, FALSE);
        strcpy(mode, "Shutdown");
        SetMode();
    }
}

void DoControl()
{
    //Serial.printf("Time.now: %f  myPID.LastUpdate: %f  difference %d  Cycle: %d\r\n", Time.now(), myPID.LastUpdate, Time.now() - myPID.LastUpdate, Cycle);
    if ((Time.now() - myPID.LastUpdate) > Cycle)
    {
        u = myPID.update(Temps[0]);
        u = max(u, u_min);
        u = min(u, u_max);

        if (debug == 1)
        {
            Serial.printf("DoControl - setting PID u value: %f\r\n", u);
        }
        //float z = 0.15;
        //char* uPID = floatToString(z);
        //Serial.printf("DoControl - u PID is now a string: %s\r\n", uPID);
        myOLED.update_OLED_pid(u);
        myOLED.update_OLED_mode(mode, modeState);
        //Serial.println("");

        //Particle.publish("New PID from DoControl...", "PID: " + String(u), PRIVATE);
        // To Do ... write code here to STAMP out Program Control data to Firebase

        WriteParameters();
    }
}

void hopperInit()
{
    //initialize hopper assembly
    pinResetFast(fanPin);
    pinMode(fanPin, OUTPUT);
    fan = digitalRead(fanPin);
    toggleTimeFan = TIMENOW;

    pinResetFast(igniterPin);
    pinMode(igniterPin, OUTPUT);
    ign = digitalRead(igniterPin);
    toggleTimeIgniter = TIMENOW;

    pinResetFast(augerPin); // initialize to LOW
    pinMode(augerPin, OUTPUT);
    aug = digitalRead(augerPin);
    toggleTimeAuger = TIMENOW;

    myOLED.update_FIA(fan, ign, aug); // update FIA on OLED
}

bool getState(int pin)
{
    return pinReadFast(pin);
}

void setState(int pin, bool newState)
{
    bool currentState = getState(pin);

    if (currentState != newState)
    {
        digitalWrite(pin, newState);
        switch (pin)
        {
        case igniterPin:
            toggleTimeIgniter = TIMENOW;
            Serial.printf("setState: toggling Igniter: %d\r\n", newState);
            ign = newState;
            myOLED.update_FIA(fan, ign, aug); // update FIA on OLED
            break;
        case fanPin:
            toggleTimeFan = TIMENOW;
            Serial.printf("setState: toggling Fan: %d\r\n", newState);
            fan = newState;
            myOLED.update_FIA(fan, ign, aug); // update FIA on OLED
            break;
        case augerPin:
            toggleTimeAuger = TIMENOW;
            Serial.printf("setState: toggling Auger: %d\r\n", newState);
            aug = newState;
            myOLED.update_FIA(fan, ign, aug); // update FIA on OLED
            break;
        }
    }
}

void handler(const char *topic, const char *data)
{
    deviceName = String(data);
    //Particle.publish("Device Name: " + String(deviceName), String(deviceName));
    ResetFirebase();
}

void getDataHandler(const char *topic, const char *data)
{
    StaticJsonBuffer<768> jsonBuffer;
    char *mutableCopy = strdup(data);
    JsonObject &root = jsonBuffer.parseObject(mutableCopy);
    Serial.printf("data: %s\r\n", data);
    //free(mutableCopy);

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

        {
            Serial.printf("Read Parameters from Firebase: %d %.1f %.1f %.1f %d %d %.1f %d %d %s %s %s %s %s %d %.2f\r\n", newCycle, newLReadPgm, newLReadWeb, newLWritten, newPB, newPMode, newPToggle, newTd, newTi, newaug ? "true" : "false", newfan ? "true" : "false", newign ? "true" : "false", newmode, newpgm ? "true" : "false", newtarget, newu);
        }
    }
}



/* Nextion Code *********************************************************************************************/
/*
 * Text component pop callback function. 
 */
void t0PopCallback(void *ptr)
{
    dbSerialPrintln("t0PopCallback");
    t0.setText("225");
}

/*
 * Button0 component pop callback function.
 * In this example,the value of the text component will plus one every time when button0 is released.
 */
void b0PopCallback(void *ptr)
{
    uint16_t len;
    uint16_t number;
    
    dbSerialPrintln("b0PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t0.getText(buffer, sizeof(buffer));
    
    number = atoi(buffer);
    number += 5;

    memset(buffer, 0, sizeof(buffer));
    itoa(number, buffer, 10);
    
    t0.setText(buffer);
}

/*
 * Button1 component pop callback function.
 * In this example,the value of the text component will minus one every time when button1 is released.
 */
void b1PopCallback(void *ptr)
{
    uint16_t len;
    uint16_t number;
    
    dbSerialPrintln("b1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t0.getText(buffer, sizeof(buffer));
    
    number = atoi(buffer);
    number -= 5;

    memset(buffer, 0, sizeof(buffer));
    itoa(number, buffer, 10);
    
    t0.setText(buffer);
}
/* END Nextion Code *******************************************************************************************/