#include <ITEADLIB_Nextion.h>
#include "Particle.h"
#include "math.h"
#include "JsonParserGeneratorRK.h"
#include "papertrail.h"
#include "main.h"
#include "MAX31865.h"
#include "pid2.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // selects the u.FL antenna

const char *DELETE_PARAMETERS = "ParametersDELETE";
const char *PUBLISH_PARAMETERS = "ParametersHookBody";
const char *READ_PARAMETERS = "ParametersRead";
const char *DELETE_TEMPS = "TempsDELETE";

// Papertrail Logger at papertrail.com
PapertrailLogHandler papertailHandler("logs2.papertrailapp.com", 20753, "PapertrailSimpleDemo");

/* Nextion variable **************/
USARTSerial& nexSerial = Serial1;       

NexButton bt0 = NexButton(5, 6, "bt0");  // Fan light
NexButton bt1 = NexButton(5, 10, "bt1"); // Igniter light
NexButton bt2 = NexButton(5, 9, "bt2"); // Auger light

NexButton bt3 = NexButton(5, 12, "bt3"); // Off - Start button
NexButton bt4 = NexButton(5, 13, "bt4"); // Mode - Smoke button
NexButton bt5 = NexButton(5, 14, "bt5"); // Mode - Ignite button
NexButton bt6 = NexButton(5, 15, "bt6"); // Mode - Hold button
NexButton bt7 = NexButton(5, 16, "bt7"); // Mode - Shutdown button
NexText t10 =   NexText(5, 5, "t10");     /* Declare a text object for Mode of the grill, default is OFF [pagid:5, component id:5, component name: "t10"]. */
NexText t1 =    NexText(5, 2, "t1");       /* Grill Temp object on Nextion display */
NexText t2 =    NexText(5, 3, "t2");       /* Meat 1 Temp object on Nextion display */
NexText t3 =    NexText(5, 4, "t3");       /* Meat 2 Temp object on Nextion display */
NexText t0 =    NexText(5, 7, "t0");      /* Declare a text object for Target temp of the grill [page id:5, component id:7, component name: "t0"]. */
NexButton b1 =  NexButton(5, 8, "b1"); /* Up ++ target temp */
NexButton b0 =  NexButton(5, 11, "b0");  /* Down -- target temp */
NexText l2 =    NexText(6, 2, "l2");      /* Declare a text object for current logging level [page id:6, component id:2, component name: "l2"]. */
NexButton l4 =  NexButton(6, 4, "l4"); /* Up ++ logging level */
NexButton l3 =  NexButton(6, 3, "l3");  /* Down -- logging level */

char buffer[100] = {0};
char buffer1[100] = {0};

/* Register object b0, b1, t10 to the Nextion touch event list. */
NexTouch *nex_listen_list[] = 
{
    //&t0,
    &b0,
    &b1,
    &l4,
    &l3,
    &t10,
    NULL
};
/* Nextion variable end **********/

/********************************************************************************/
int debug = 0; /* set to 1 to get more debug information to the Serial port */
/********************************************************************************/

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
int ReadParametersInterval = 6; //  #Frequency to poll web for new parameters
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
int target = 90; // normally initially set to 225
int Td = 45.0;
int Ti = 180;
float u = 0.15;
//double uPID = 0.15;

//New Parameter Variables
int     newCycle = 20.0;
double  newLReadPgm;
double  newLReadWeb;
double  newLWritten;
int     newPB = 60.0;
int     newPMode = 2.0;
double  newPToggle;
int     newTd = 45.0;
int     newTi = 180;
bool    newaug = false;
bool    newfan = false;
bool    newign = false;
char    newmode[9] = "Off";
bool    newpgm = false;  
int     newtarget = 225;  
double  newu = 0.15;

//variables for WiFi management
int wifi_strength; // -128 is weak to -1 is strong signal strength. A 1 means Wi-Fi chip error and 2 means a time-out error.
int wifi_bars; // a calculation of the signal strength -128 to -97 = 1bar; <-97 && >-64 = 2bar; <-64 & >-32 = 3bar; <-32 && > 0 = 4bar;

bool ONLINE = false; // true is Online and false is Offline

//variables for the Grill temperature dropping more than XX degrees while in a cook... then we want to make a phone call or maybe send a text message!
bool tempMonitorOn = false;
int tempDropOnTemp = 90;      /* 115 - The grill must reach this temp before temp drop monitoring is enabled */
double tempDropOnTime;        /* The 1st timestamp when we have reached or exceeded the target temp and grill temp monitoring should start from this point. */
int tempMonitorVariance = 15; /* 25 - grill temp degrees of allowable drop in temp before we make a phone call */
int tempDropInterval = 60;    /* the interval at which we check to see if there is a grill temp drop */
int sendmessage = 0;
int loglevel = 1;
double sendmessageTIME;
int loopcounter =0;

uint32_t freemem;

//initialize PID
pid myPID(PB, Ti, Td, loglevel);

//initialize MAX31865
MAX31865 myMAX31865(cs);

void setup() {

    cloudConnect();

    // if the cloud is not available, then change SYSTEM_MODE from 2(manual) to 1(automatic) -- this theory needs some work...
    if(waitFor(Particle.connected, 10000)) {
        ONLINE = true; // We are onlinein Semi-Automatic mode, connected to the Cloud and WiFi
        Particle.syncTime();
        Time.beginDST();
        Time.zone(-6); //CST
        Log.info("Pellet Pirate successfully connected to the cloud\r\n");
        Log.info("Online?: %s\r\n", ONLINE ? "true" : "false");
    }

    Serial.begin(9600);
    waitFor(Serial.isConnected, 5000);

    freemem = System.freeMemory();

    Log.info("**************************************************************************************************************************\r\n");
    Log.info("*************************************** P R O G R A M    B E G I N *******************************************************\r\n");
    Log.info("Firmware version: %s  at: %s  and free memory is:%d\r\n", System.version().c_str(), Time.timeStr().c_str(), freemem);

    // write to log what network credentials we have setup on the photon
    // security is one of WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA, WLAN_SEC_WPA2, WLAN_SEC_WPA_ENTERPRISE, WLAN_SEC_WPA2_ENTERPRISE
    // cipher is one of WLAN_CIPHER_AES, WLAN_CIPHER_TKIP or WLAN_CIPHER_AES_TKIP
    WiFiAccessPoint ap[5];
    int found = WiFi.getCredentials(ap, 5);
    for (int i = 0; i < found; i++) {Log.info("Network credential in PelletPirate: ssid: %s  security: %d  cipher: %d\r\n", ap[i].ssid, ap[i].security, ap[i].cipher);}
    // END log network credentials
    wifi_strength = WiFi.RSSI();
    Log.info("Connected to: %s  strength: %d\r\n", WiFi.SSID(), wifi_strength);

    Particle.subscribe("spark/", handler, MY_DEVICES);
    Particle.publish("spark/device/name", NULL, 60, PRIVATE);
    Particle.subscribe("hook-response/ParametersRead", getDataHandler, MY_DEVICES);
    Particle.variable("msg", &sendmessage, INT);
 
    pinMode(fanPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(igniterPin, OUTPUT);

    pinMode(cs, OUTPUT);
    pinMode(csm1, OUTPUT);
    pinMode(csm2, OUTPUT);

    delay(5000);

    /* Nextion Display code ********************************************************************************************************/
    /* Set the baudrate which is for debug and communicate with Nextion screen. */
    nexInit();
    
    t0.attachPop(t0PopCallback); /* Register the pop(Release) event callback function of the current text component. */
    b0.attachPop(b0PopCallback); /* Register the pop(Release) event callback function of the current button0 component. */
    b1.attachPop(b1PopCallback); /* Register the pop(Release) event callback function of the current button1 component. */
    t10.attachPop(t10PopCallback); /* Register the pop(Release) event callback function of the current Mode component. */
    l4.attachPop(l4PopCallback); /* Register the pop(Release) event callback function of the logging button L4 component. */
    l3.attachPop(l3PopCallback);  /* Register the pop(Release) event callback function of the logging button L3 component. */       

    dbSerialPrintln("setup done");
    /* END Nextion Display code ****************************************************************************************************/

    LReadPgm = TIMENOW;
    LReadWeb = TIMENOW;

    delay(1000); // just 1 second to chill...
    READY = 1; // now we are ready for the LOOP to start churning... all setup has run
    freemem = System.freeMemory();
    Log.info("Finishing SETUP in 1 second, free memory is: %d  Now awaiting command from the Pellet Pirate!\r\n", freemem);
}

void loop() {

    nexLoop(nex_listen_list);
    cloudConnect();
    nexLoop(nex_listen_list);    
    if (READY == 1 && modeState !=0 && ONLINE == true) { // Online mode... let's cook and talk to the whole world online!
        ReadTemperatures();
        nexLoop(nex_listen_list);
        t1PopCallback(); //Update the display with current temperatures
        t2PopCallback(); //Update the display with current temperatures
        t3PopCallback(); //Update the display with current temperatures
        nexLoop(nex_listen_list); //added since others had this after them
        ReadParameters(); //Check for new parameters that may have been written from the Nextion Touch Display or Web Program into Firebase
        nexLoop(nex_listen_list);
        DoMode();
    } else if (READY == 1 && modeState !=0 && ONLINE == false) { // Offline mode... let's just COOK baby
        //Log.info("in LOOP elseif statement - OFFLINE\r\n");
        ReadTemperatures();
        nexLoop(nex_listen_list);
        t1PopCallback();
        t2PopCallback();
        t3PopCallback();
        nexLoop(nex_listen_list); //added since others had this after them
        DoMode();
    }
    nexLoop(nex_listen_list);    
}

void cloudConnect() {
    if (Particle.connected() == false) {
        Particle.connect();
        ONLINE = Particle.connected();
    } 
}

// EXAMPLE using a callback
void wifi_scan_callback(WiFiAccessPoint* wap, void* data) {
    WiFiAccessPoint& ap = *wap;
    Log.info("SSID: %s  Security: %d  Channel: %d  RSSI: %d\r\n", ap.ssid, ap.security, ap.channel, ap.rssi);
}

void ReadTemperatures() {
    double time;

    for (int i = 0; i < 3; i++) {
        if (i == 0) {cs = A2;} //Grill
        if (i == 1) {cs = csm1;} //Meat1
        if (i == 2) {cs = csm2;} //Meat2

        Temps[i] = myMAX31865.get_Temp(cs);
        if (i == 2) {
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
    if (ONLINE == true && (Time.now() - toggleTimeTemps > TempInterval) && ResetFIREBASE == 1) {
             char qT[128];
             snprintf(qT, sizeof(qT), "{\"T1\":%.6f,\"T2\":%.6f,\"T3\":%.6f,\"TT\":%.0f,\"time\":%.0f,\"n\":\"%s\"}", T1, T2, T3, TT, time, deviceName.c_str());
             Particle.publish("sse-Temps", qT, PRIVATE);
    
        toggleTimeTemps = Time.now();
        freemem = System.freeMemory();

        if (loglevel >= 1) {Log.info("%s ReadTemperatures - Grill: %.1f   Meat1: %.1f   Meat2: %.1f   Time:%.0f free memory:%d\r\n", Time.timeStr().c_str(), T1, T2, T3, toggleTimeTemps, freemem);}
        
        // IFTTT Logic to send text message or phone call if temp drops below a threshhold set in variables above
        if (modeState not_eq 0 && modeState not_eq 5) {   // as long as we are not OFF or in Shutdown mode, then you can blast IFTTT messages!
            if ((T1 >= tempDropOnTemp) && tempMonitorOn == false) { // testing to SEE if we should flip to say we are IN a cook !!!
                tempMonitorOn = true;
                tempDropOnTime = Time.now();
                Log.info("%s *** ALERT *** We are IN a Cook and Grill Temp is now being monitored for Target: %d and Grill: %.1f as of time:%.0f\r\n", Time.timeStr().c_str(), target, T1, tempDropOnTime);
            }

            if ((tempMonitorOn == true) && (Time.now() - tempDropOnTime > tempDropInterval) && (target - T1 >= tempMonitorVariance)) {
                sendmessage = 1; // this will TRIGGER the Phone call and SMS message through IFTTT
                Log.warn("****** MAKING PHONE CALL and SENDING SMS ****** temp is dropping - Target: %d and Grill: %.1f as of time:%.0f\r\n", target, T1, tempDropOnTime);
                tempMonitorOn = false;
                sendmessageTIME = Time.now();
            }

            if (((Time.now() - sendmessageTIME) > (tempDropInterval + 15)) && sendmessage == 1) {
                sendmessage = 0; // this should turn off the IFTTT trigger
                Log.info("%s RESET sendmessage flag for IFTTT trigger to OFF\r\n", Time.timeStr().c_str());
            }
        }
    }
}

void ResetFirebase() {

    if(ONLINE == true) {
        char qDELETE[64];
        snprintf(qDELETE, sizeof(qDELETE), "{\"n\":\"%s\"}", deviceName.c_str());
        Particle.publish(DELETE_TEMPS, qDELETE, PRIVATE);
        kount = 0;
        delay(300);

        char pDELETE[64];
        snprintf(pDELETE, sizeof(pDELETE), "{\"n\":\"%s\"}", deviceName.c_str());
        Particle.publish(DELETE_PARAMETERS, pDELETE, PRIVATE);
        delay(300);
        Log.info("%s ResetFirebase is done!\r\n", Time.timeStr().c_str());

        ResetFIREBASE = 1;
    } else {
        ResetFIREBASE = 1; // in manual mode, got to set this anyway to get TEMPS to work
    }
}

void ReadParameters() {
    if (ONLINE == true && (Time.now() - LReadWeb) >= ReadParametersInterval) {
        //Log.info("*****************************  IN ReadParameters  ************************************************\r\n");
        LReadWeb = TIMENOW;
        char pREAD[255];
        snprintf(pREAD, sizeof(pREAD), "{\"n\":\"%s\"}", deviceName.c_str());
        Particle.publish(READ_PARAMETERS, pREAD, PRIVATE);
        UpdateParameters();
    }
}

void UpdateParameters() {
    bool DoINeedtoWriteParameters = false;
    char str_target[4];

    //loop through new parameters and see what changed
    if (target != newtarget) {
        myPID.setTarget(newtarget, loglevel);
        target = newtarget;
        sprintf(str_target,"%d",target); // get target into a string so we can send it to the nextion display
        t0.setText(str_target);
        Log.info("%s UpdateParameters - new target temp is: %s\r\n", Time.timeStr().c_str(), str_target);
        DoINeedtoWriteParameters= true;
    }
    if (PB != newPB || Ti != newTi || Td != newTd) {
        Log.info("%s UpdateParameters - New PB Ti or Td -- (PB:%d - newPB:%d) or (Ti:%d - newTi:%d) or (Td:%d - newTd:%d)\r\n", Time.timeStr().c_str(), PB, newPB, Ti, newTi, Td, newTd);
        PB = newPB;
        Ti = newTi;
        Td = newTd;
        myPID.setGains(PB, Ti, Td, loglevel);
        DoINeedtoWriteParameters= true;
    }
    if (PMode != newPMode) {
        Log.info("%s UpdateParameters - New PMode -- (PMode:%d - newPMode:%d)\r\n", Time.timeStr().c_str(), PMode, newPMode );
        PMode = newPMode;
        SetMode();
        DoINeedtoWriteParameters= true;
    }
    if (strcmp(mode, newmode) != 0) {
        Log.info("%s UpdateParameters - Processing New Mode: (%s) changing from Previous Mode: (%s)\r\n", Time.timeStr().c_str(), newmode, mode);
        strcpy(mode, newmode); // should copy newmode into the mode variable
        SetMode();
        DoINeedtoWriteParameters= true;
    }
    if (pgm != newpgm) {
        float timeholder;
        timeholder = Time.now();
        Log.info("check on this...not sure timeholder is working in UpdateParameters, time is: %f", timeholder);
        pgm = newpgm;
        LReadPgm = (timeholder - 10000 + .01);
        //Log.info("UpdateParameters - newpgm!");
        DoINeedtoWriteParameters= true;
        //TO DO .... need to finish this when you get to the PROGRAM coding...
        //Program = GetProgram(Parameters, Program)
        //Parameters = SetProgram(Parameters, Program)
        //break # Stop processing new parameters
    }
    if(ONLINE == true && DoINeedtoWriteParameters) {
        WriteParameters();
        //Log.info("%s ***** DoINeedToWriteParameters just fired...*****\r\n", Time.timeStr().c_str());
    }
    //else { Log.info("%s Nothing in Parameters changed this time...\r\n", Time.timeStr().c_str());}
}

void WriteParameters() {
        aug = digitalRead(augerPin);
        fan = digitalRead(fanPin);
        ign = digitalRead(igniterPin);
        //Log.info("WriteParameters: FIA status: Fan: %d  Igniter: %d  Auger: %d\r\n", fan, ign, aug);
        LWritten = TIMENOW;

        char qP[255];
 
        snprintf(qP, sizeof(qP), "{\"Cycle\":%d,\"LReadPgm\":%.1f,\"LReadWeb\":%.1f,\"LWritten\":%.1f,\"PB\":%d,\"PMode\":%d,\"PToggle\":%.1f,\"Td\":%d,\"Ti\":%d,\"fan\":%s,\"ign\":%s,\"aug\":%s,\"mode\":\"%s\",\"pgm\":%s,\"target\":%d,\"u\":%.2f,\"n\":\"%s\"}",
                 Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, fan ? "true" : "false", ign ? "true" : "false", aug ? "true" : "false", mode, pgm ? "true" : "false", target, u, deviceName.c_str());

        Particle.publish(PUBLISH_PARAMETERS, qP, 60, PRIVATE);
        if ( loglevel >= 2 ) {Log.info("%s C%d LP%.0f LW%.0f LWr%.0f PB%d PM%d PT%.0f Td%d Ti%d F:%s I:%s A:%s Mode:%s Pg:%s TT:%d u:%.2f\r\n", Time.timeStr().c_str(), Cycle, LReadPgm, LReadWeb, LWritten, PB, PMode, PToggle, Td, Ti, fan ? "true" : "false", ign ? "true" : "false", aug ? "true" : "false", mode, pgm ? "true" : "false", target, u);}
}

void SetMode() {
    if (strcmp(mode, "Off") == 0) {
        modeState = 0;
        Log.info("SetMode - Off");
        hopperInit();
        Log.info("%s SetMode, just finished hopperInit\r\n", Time.timeStr().c_str());
    }
    else if (strcmp(mode, "Start") == 0) {
        if (ONLINE == true) {ResetFirebase();}
        modeState = 1;
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        setState(igniterPin, TRUE);
        Log.info("%s SetMode in START after all pins set TRUE: Fan: %d  Igniter: %d  Auger: %d\r\n", Time.timeStr().c_str(),digitalRead(fanPin), digitalRead(igniterPin), digitalRead(augerPin));
        Cycle = 15 + 45;
        u = 15.0 / (15.0 + 45.0); //P0
        Log.info("%s SetMode - Start : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Smoke") == 0) {
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
        Log.info("%s SetMode - Smoke : u = %.2f\r\n", Time.timeStr().c_str(), u);

    }
    else if (strcmp(mode, "Ignite") == 0) {
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
        Log.info("%s SetMode - Ignite : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Hold") == 0) {
        modeState = 4;
        Log.info("SetMode - Hold\r\n");
        sendCommand("click bt6,1"); //activate press event of component bt6 - the Hold button on the display
        sendCommand("click bt6,0"); //activate press release event of component bt6 - the Hold button on the display       
        setState(augerPin, TRUE);
        setState(fanPin, TRUE);
        checkIgniter();
        Cycle = PIDCycleTime;
        u = u_min; //Set to maintenance level
        Log.info("%s SetMode - Hold : u = %.2f\r\n", Time.timeStr().c_str(), u);
    }
    else if (strcmp(mode, "Shutdown") == 0) {
        modeState = 5;
        Log.info("SetMode - Shutdown\r\n");
        sendCommand("click bt7,1"); //activate press event of component bt7 - the Shutdown button on the display
        sendCommand("click bt7,0"); //activate press release event of component bt7 - the Shutdown button on the display               
        hopperInit();
        setState(fanPin, TRUE);
        setState(augerPin, false); // added 4/1/2018 after this button stayed on while in Shutdown mode, it should be OFF to burn off pellets.
    }

    //Log.info("SetMode: I think I found it... fixing to WriteParameters");
    WriteParameters();
    //Log.info("I found it... WriteParameters done because SetMode is in Setup procedure above !!!!");

    if (loglevel == 3){Log.info("SetMode end --- WriteParameters - done!\r\n");}
}

void DoMode() {
    if (strcmp(mode, "Off") == 0) {
        return;
    }

    else if (strcmp(mode, "Shutdown") == 0) {
        if ((Time.now() - toggleTimeFan) > ShutdownTime) {
            sendCommand("click bt3,1"); //activate press event of component bt3 - the Off/On button of on the display
            sendCommand("click bt3,0"); //activate release event of component bt3 - the Off/On button on the display
            sendToLCD(2, "bt0", "0"); // turn off Fan button
            sendToLCD(2, "bt7", "0"); // turn off Shutdown mode button
            strcpy(mode, "Off");
            SetMode();
        }
    }

    else if (strcmp(mode, "Start") == 0) {
        DoAugerControl();
        setState(igniterPin, TRUE);
        if (Temps[0] > 120) {
            strcpy(mode, "Hold");
            SetMode();
        }
    }

    else if (strcmp(mode, "Smoke") == 0) {
        DoAugerControl();
    }

    else if (strcmp(mode, "Ignite") == 0) {
        DoAugerControl();
        setState(igniterPin, TRUE);
    }

    else if (strcmp(mode, "Hold") == 0) {
        DoControl();
        DoAugerControl();
    }
}

void DoAugerControl() {
    //Auger currently on AND TimeSinceToggle > Auger On Time
    if (digitalRead(augerPin) && ((Time.now() - toggleTimeAuger) > (Cycle * u * 2))) { // 12162017 added the *2 to cycle*u to increase the ON time for Auger to get more pellets in kettle!
        //int TimeSince1 = Time.now() - toggleTimeAuger;
        //if (debug == 1) {
        //Log.info("%s DoAugerControl - in TOP of first if - Auger is on! turning it OFF! timenow:%f  toggletimeauger:%d  Cycle:%0.2f u:%d\r\n", Time.timeStr().c_str(), toggleTimeAuger, Cycle, u, TimeSince1);
        //}
        if (u <= 1.0) { // added the = statement 02272017 to stop the violent looping of this function when PID is == 1.0
            setState(augerPin, FALSE);
            WriteParameters();
        }

        checkIgniter();
    }

    //Auger currently off AND TimeSinceToggle > Auger Off Time
    if (!digitalRead(augerPin) && ((Time.now() - toggleTimeAuger) > ((Cycle * (1 - u))))) {
        int TimeSince2 = Time.now() - toggleTimeAuger;
        if (loglevel == 1) {Log.info("%s DoAugerControl - in TOP of second if - Auger is off, turning it ON! timenow:%f  toggletimeauger:%d  Cycle:%f u:%d\r\n", Time.timeStr().c_str(), toggleTimeAuger, Cycle, u, TimeSince2);}
        setState(augerPin, TRUE);
        checkIgniter();
        WriteParameters();
    }
}

void checkIgniter() {
    //Check if igniter needed
    if (Temps[0] < igniterTemperature) {
        setState(igniterPin, TRUE);
    }
    else {
        setState(igniterPin, FALSE);
    }

    //Check if the igniter has been running too long
    if ((Time.now() - toggleTimeIgniter) > 1200 && digitalRead(igniterPin)) {
        Log.warn("**SAFETY FIRST** - Disabling igniter due to timeout\r\n");
        setState(igniterPin, FALSE);
        strcpy(mode, "Shutdown");
        SetMode();
    }
}

void DoControl() {
    double old_u;

    //Log.info("DoControl: Time.now: %f  myPID.LastUpdate: %f  difference %d  Cycle: %d  u now:%.2f\r\n", Time.now(), myPID.LastUpdate, Time.now() - myPID.LastUpdate, Cycle, u);
    if ((Time.now() - myPID.LastUpdate) > Cycle) {
        u = myPID.update(Temps[0], target, loglevel);
        old_u = u;
        u = max(u, u_min);
        u = min(u, u_max);

        if (loglevel == 3) {Log.info("%s DoControl IF Statement - through u max min calculation old_u:%.2f and u:%.2f\r\n", Time.timeStr().c_str(), old_u, u);}

        // To Do ... write code here to STAMP out Program Control data to Firebase

        WriteParameters();
    }
}

void hopperInit() {
    //initialize hopper assembly
    pinResetFast(fanPin); // initialize to LOW
    //pinMode(fanPin, OUTPUT); // this is done in STARTUP.... I don't think I need to do it again 05142018
    fan = digitalRead(fanPin);
    toggleTimeFan = Time.now(); //TIMENOW

    pinResetFast(igniterPin); // initialize to LOW
    //pinMode(igniterPin, OUTPUT); // this is done in STARTUP.... I don't think I need to do it again 05142018
    ign = digitalRead(igniterPin);
    toggleTimeIgniter = Time.now(); // TIMENOW;

    pinResetFast(augerPin); // initialize to LOW
    //pinMode(augerPin, OUTPUT); // this is done in STARTUP.... I don't think I need to do it again 05142018
    aug = digitalRead(augerPin);
    toggleTimeAuger = Time.now(); // TIMENOW;

}

int getState(int pin) {
    return pinReadFast(pin);
}

void setState(int pin, int newState) {  // changed newState from bool to int
    int currentState = getState(pin);
    char pinState[10];
    
     if (currentState != newState) {
        digitalWrite(pin, newState);
        switch (pin) {
        case (D4):
            toggleTimeFan = Time.now(); //TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt0", pinState);
            //Log.info("setState: toggling Fan: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));           
            Log.info("%s setState: toggling Fan: %d\r\n", Time.timeStr().c_str(), newState);
            fan = newState;
            break;
        case (D5):
            toggleTimeIgniter = Time.now(); //TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt1", pinState);
            //Log.info("setState: toggling Igniter: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));
            Log.info("%s setState: toggling Igniter: %d\r\n", Time.timeStr().c_str(), newState);
            ign = newState;
            break;
        case (D6):
            toggleTimeAuger = Time.now(); //TIMENOW;
            snprintf(pinState, sizeof(pinState), "%d", newState);
            sendToLCD(2, "bt2", pinState);
            //Log.info("setState: toggling Auger: %d and text pinState: %s and length %d\r\n", newState, pinState, strlen(pinState));            
            Log.info("%s setState: toggling Auger: %d\r\n", Time.timeStr().c_str(), newState);
            aug = newState;
            break;
        }
    }
}

void handler(const char *topic, const char *data) {
    deviceName = String(data);
    //ResetFirebase();
}

void getDataHandler(const char *event, const char *data) {

    JsonParserStatic<768, 60> parser1;
    parser1.clear();
    parser1.addString(data);
	if (!parser1.parse()) {Log.warn("parsing failed to parse params\r\n"); return;}
	if (!parser1.getOuterValueByKey("Cycle", newCycle)) {Log.warn("failed to get newCycle from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("LReadPgm", newLReadPgm)) {Log.warn("failed to get newLReadPgm from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("LReadWeb", newLReadWeb)) {Log.warn("failed to get newLReadWeb from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("LWritten", newLWritten)) {Log.warn("failed to get newLWritten from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("PB", newPB)) {Log.warn("failed to get newPB from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("PMode", newPMode)) {Log.warn("failed to get newPMode from json params\r\n");	return;}
	if (!parser1.getOuterValueByKey("PToggle", newPToggle)) {Log.warn("failed to get newPToggle from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("Td", newTd)) {Log.warn("failed to get newTd from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("Ti", newTi)) {Log.warn("failed to get newTi from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("aug", newaug)) {Log.warn("failed to get newaug from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("fan", newfan)) {Log.warn("failed to get newfan from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("ign", newign)) {Log.warn("failed to get newign from json params\r\n"); return; }
    String strValue;	
	if (!parser1.getOuterValueByKey("mode", strValue)) {Log.warn("failed to get mode from json params\r\n"); return;}
	strcpy(newmode, strValue);	
	if (!parser1.getOuterValueByKey("pgm", newpgm)) {Log.warn("failed to get newpgm from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("target", newtarget)) {Log.warn("failed to get newtarget from json params\r\n"); return;}
	if (!parser1.getOuterValueByKey("u", newu)) {Log.warn("failed to get newu from json params\r\n"); return;}

    Log.info("%s Params Read: %d %.1f %.1f %.1f %d %d %.1f %d %d %s %s %s %s %s t:%d u:%.2f\r\n", Time.timeStr().c_str(), newCycle, newLReadPgm, newLReadWeb, newLWritten, newPB, newPMode, newPToggle, newTd, newTi, newaug ? "true" : "false", newfan ? "true" : "false", newign ? "true" : "false", newmode, newpgm ? "true" : "false", newtarget, newu);
 
    UpdateParameters(); // process what we just got from Firebase read!
}

/* Nextion Code *********************************************************************************************/
void t0PopCallback(void *ptr) {  /* Text component pop callback function. */
    dbSerialPrintln("t0PopCallback");
    
    memset(buffer, 0, sizeof(buffer));
    t0.setText(itoa(target, buffer, 4));
}

void b1PopCallback(void *ptr) {  /* Target temp +5 degrees every time the Up+ button is released. */
    uint16_t number;
    
    dbSerialPrintln("b1PopCallback");

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

void b0PopCallback(void *ptr) {  /* In this example,the value of the text component will minus 5 degress every time when button0 is released. */
    uint16_t number;
    
    dbSerialPrintln("b0PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t0.getText(buffer, sizeof(buffer));
    
    number = atoi(buffer);
    number -= 5;
    newtarget = number;

    //Log.info("********************** in - 5 temps ***********************************\r\n");

    memset(buffer, 0, sizeof(buffer));
    itoa(number, buffer, 10);
    
    t0.setText(buffer);

    UpdateParameters();
}

void t10PopCallback(void *ptr) {   /* Text component pop callback function for Mode. */
    dbSerialPrintln("t10PopCallback");

    memset(buffer, 0, sizeof(buffer));
    t10.getText(buffer, sizeof(buffer));

    if(strcmp("Off", buffer) == 0) // on-off button pressed ... if set to "Off" then i need to force Shutdown if grill is currently HOT... so shutdown process runs to cool grill
    { 
        if(T1 > 115) 
        {
          Log.info("%s T10PopCallback - On/Off button pressed while in cook, setting mode to Shutdown for proper cool down procedure ", Time.timeStr().c_str());
          strcpy(buffer, "Shutdown");
          t10.setText(buffer); 
        }
    }
    strcpy(newmode, buffer);
    Log.info("%s T10PopCallback - newmode is: %s and buffer length is: %d\r\n", Time.timeStr().c_str(), newmode, strlen(buffer));
        
    UpdateParameters();
}

void t1PopCallback(void) {
    uint16_t integer_temp, decimal_temp;

    integer_temp = T1;
    decimal_temp = ((T1 - integer_temp)*10);
    //Log.info("T1PopCallback - integer is: %d and the decimal is: %d\r\n", integer_temp, decimal_temp);
 
    memset(buffer, 0, sizeof(buffer));
    memset(buffer1, 0, sizeof(buffer));
    itoa(integer_temp, buffer, 10);
    itoa(decimal_temp, buffer1, 10);

    strcat(buffer, ".");
    strcat(buffer, buffer1);
     
    t1.setText(buffer);
}

void t2PopCallback(void) {
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

void t3PopCallback(void) {
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

void sendToLCD(uint8_t type,String index, String cmd) {
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

void l4PopCallback(void *ptr) {  /* raise logging level by 1 from the logging level screen in Setup on the Nextion display everytime the Up+ button is released. */

    uint16_t number = 0;
    
    dbSerialPrintln("l4PopCallback");

    memset(buffer, 0, sizeof(buffer));
    l2.getText(buffer, sizeof(buffer));
    number = atoi(buffer);

    if (loglevel < 3) { //max loglevel is 3 at this time...
        number += 1;
        loglevel = number;
        memset(buffer, 0, sizeof(buffer));
        itoa(number, buffer, 10);
        
        l2.setText(buffer);
        Log.info("%s Logging level changed to: %d\r\n", Time.timeStr().c_str(), loglevel);
    }
}

void l3PopCallback(void *ptr) {  /* lower the logging level by 1 in logging level in Setup on the Nextion display everytime the Down- button is released. */
    uint16_t number = 0;
    
    dbSerialPrintln("b1PopCallback");

    memset(buffer, 0, sizeof(buffer));
    l2.getText(buffer, sizeof(buffer));
    number = atoi(buffer);

    if (loglevel > 0) { //max loglevel between 0 and 3 at this time don't want to subtract if we are already at loglevel 0
        number -= 1;
        loglevel = number;
        memset(buffer, 0, sizeof(buffer));
        itoa(number, buffer, 10);
    
        l2.setText(buffer);
        Log.info("%s Logging level changed to: %d\r\n", Time.timeStr().c_str(), loglevel);
    }
}

/* END Nextion Code *******************************************************************************************/
