/*
 * from Demo_NHD0420CW-Ax3_I2C.ino
 * 
 * Tutorial sketch for use of character OLED slim display family by Newhaven with Arduino Uno (Brian Beardmore: modifying for Particle Photon), using 
 * only Wire (I2C) library.  Models: NHD0420CW-Ax3, NHD0220CW-Ax3, NHD0216CW-Ax3. Controller: US2066
 * in this example, the display is connected to Photon via I2C interface.
 *
 * Displays on the OLED alternately a 4-line message and a sequence of character "block".
 * This pgm assumes the use of a 4x20 display; if different, modify the values of the two variables 
 * ROW_N and COLUMN_N.
 * The pgm uses the minimum possible of Photon pins; if you intend to use also /RES line, 
 * the related instructions are already present, it's sufficient to remove the comment markers.
 *
 * The circuit modified by Brian Beardmore for Particle Photon I2C:
 * OLED pin 1 (Vss)          to VSS ground
 * OLED pin 2 (VDD)          to 3.3V
 * OLED pin 3 (REGVDD)       to GND (not using 5V, docs say take to GND)
 * OLED pin 4 (SA0)          to VSS ground should use 0x3C address (to assign I2C address 0x3D, connect to VDD)
 * OLED pin 5 and 6          to VSS ground
 * OLED pin 7 (SCL)          to Photon D1 (SCL); 10K pull-up resistor on OLED pin to 3.3V
 * OLED pin 8 and 9          (SDAin,SDAout are jumpered) to Photon D0 (SDA); 10K pull-up resistor on each OLED pin 8 and 9 to 3.3V
 * OLED pin 10 to 15         to VSS ground
 * OLED pin 16 (/RES)        to VDD 3.3V    
 * OLED pin 17 (BS0)         to VSS ground  ** I2C config BSO to GND (low)
 * OLED pin 18 (BS1)         to VDD 3.3V    ** I2C config BS1 to 3.3V (high)
 * OLED pin 19 (BS2)         to Vss ground  ** I2C config Bs2 to GND (low)
 * OLED pin 20 (Vss)         to Vss ground
 *
 * Original example created by Newhaven Display International Inc.
 * Modified and adapted to Arduino Uno 15 Mar 2015 by Pasquale D'Antini
 * Modified 19 May 2015 by Pasquale D'Antini
 * Modified 06 Feb 2017 by Brian Beardmore for Particle Photon
 *
 * This example code is in the public domain.
 */
#include "OLED.h"

//declarations
void command(byte);
void data(byte);
void send_packet(byte);
void output(const byte Stringch[4][21]);
void outputMode(const byte Stringch[1][9]);
void update_OLED_mode(char mode[9], int modestate);
//void update_OLED_mode(char mode[9], int modestate);
void update_OLED_pid(double uPID);
//void update_OLED_temps(float (*)[3], int*);
void update_FIA(int, int, int);
void outputPID(const byte Stringch[1][5]);
void OLED_menu1(void);
void OLED_cook(void);
void menu1(void);
void update_OLED_target(float);

// the Button
const int buttonPinUp = 4;
const int buttonPinDn = 5;
const int buttonPinLt = 3;
const int buttonPinRt = 2;
const int buttonPinSel = 6;

ClickButton buttonUp(buttonPinUp, LOW, CLICKBTN_PULLUP);
ClickButton buttonDn(buttonPinDn, LOW, CLICKBTN_PULLUP);
ClickButton buttonLt(buttonPinLt, LOW, CLICKBTN_PULLUP);
ClickButton buttonRt(buttonPinRt, LOW, CLICKBTN_PULLUP);
ClickButton buttonSel(buttonPinSel, LOW, CLICKBTN_PULLUP);

// Button results
int function = 0;
const int incrementDown = -5;
const int incrementUp = 5;

const byte ROW_N = 4;     // Number of display rows
const byte COLUMN_N = 20; // Number of display columns
//const byte RES = 13;                // Arduino's pin assigned to the Reset line (optional, can be always high)
const byte SLAVE2W = 0x3C; // Display I2C address, in 7-bit form: 0x3C if SA0=LOW, 0x3D if SA0=HIGH
const byte SPLASH[4][21] = {"It's a Pirate's Life",
                            "......for me!.......",
                            "  pelletpirate.com  ",
                            "------- C2017 ------"};
const byte MENU1[4][21] = {"  pelletpirate.com  ",
                           " 1. Start a cook....",
                           " 2. Setup...........",
                           "                    "};
const byte COOK[4][21] = {"gr:     set:        ",
                          "m1:     net:        ",
                          "m2:     pid:        ",
                          "Tg:     pelletpirate"};

const byte MODER[6][9] = {"Off     ", "Start   ", "Smoke   ", "Ignite  ", "Hold    ", "Shutdown"};

byte ppid[1][5] = {"0.00"};

byte ttemp[3][4] = {"000"};

//std::vector<std::string> MODEX = {"Off", "Shutdown", "Start", "Smoke", "Ignite", "Hold"}; // possible modes: Off, Shutdown, Start, Smoke, Ignite, Hold

byte new_line[4] = {0x80, 0xA0, 0xC0, 0xE0}; // DDRAM address for each line of the display
byte rows = 0x08;                            // Display mode: 1/3 lines or 2/4 lines; default 2/4 (0x08)
byte tx_packet[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// Packet to be transmitted (max 20 bytes)

uint16_t menu;   /* menu number that the LCD is currently displaying */
uint16_t cursor; /* HEX address of where the cursor is currently */

OLED::OLED(void)
{
    OLED_init();
}

// _______________________________________________________________________________________

void OLED::command(byte c) // SUBROUTINE: PREPARES THE TRANSMISSION OF A COMMAND
{
    tx_packet[0] = 0x00; // Control Byte; C0_bit=0, D/C_bit=0 -> following Data Byte contains command
    tx_packet[1] = c;    // Data Byte: the command to be executed by the display
    send_packet(2);      // Transmits the two bytes
}
// _______________________________________________________________________________________

void OLED::data(byte d) // SUBROUTINE: PREPARES THE TRANSMISSION OF A BYTE OF DATA
{
    tx_packet[0] = 0x40; // Control Byte; C0_bit=0, D/C_bit=1 -> following Data Byte contains data
    tx_packet[1] = d;    // Data Byte: the character to be displayed
    send_packet(2);      // Transmits the two bytes
}
// _______________________________________________________________________________________

void OLED::send_packet(byte x) // SUBROUTINE: SEND TO THE DISPLAY THE x BYTES STORED IN tx_packet
{
    byte ix = 0; // Bytes index

    Wire.beginTransmission(SLAVE2W); // Begin the transmission via I2C to the display with the given address
    delayMicroseconds(10);
    for (ix = 0; ix < x; ix++) // One byte at a time,
    {
        Wire.write(tx_packet[ix]); //  queue bytes for transmission
        delayMicroseconds(10);
    }
    Wire.endTransmission(); // Transmits the bytes that were queued
    delayMicroseconds(10);
}
// _______________________________________________________________________________________

void OLED::output(const byte Stringch[4][21]) // Splash Screen
{
    byte r = 0; // Row index
    byte c = 0; // Column index

    command(0x01); // Clears display (and cursor home)
    delay(3);      // After a clear display, a minimum pause of 1-2 ms is required

    for (r = 0; r < ROW_N; r++) // One row at a time,
    {
        command(new_line[r]);          //  moves the cursor to the first column of that line
        for (c = 0; c < COLUMN_N; c++) // One character at a time,
        {
            data(Stringch[r][c]); //  displays the correspondig string
        }
    }
}
// _______________________________________________________________________________________

void OLED::outputMode(const byte Stringch[1][9]) // Mode
{

    byte r = 0;  // Row index
    byte c = 13; // Column index

    for (r = 0; r < 1; r++) // One row at a time,
    {
        //command(new_line[r]);           //  moves the cursor to the first column of that line
        for (c = 0; c < COLUMN_N - 12; c++) // One character at a time,
        {
            data(Stringch[r][c]); //  displays the corresponding string
        }
    }
}

void OLED::outputPID(const byte Stringch[1][5]) // Mode
{

    byte r = 0;  // Row index
    byte c = 13; // Column index

    for (r = 0; r < 1; r++) // One row at a time,
    {
        //command(new_line[r]);           //  moves the cursor to the first column of that line
        for (c = 0; c < COLUMN_N - 16; c++) // One character at a time,
        {
            data(Stringch[r][c]); //  displays the corresponding string
        }
    }
}

void OLED::outputTTemp(const byte Stringch[4][4]) // Temps
{

    byte r = 0; // Row index
    byte c = 4; // Column index

    for (r = 0; r < 1; r++) // One row at a time,
    {
        //command(new_line[r]);           //  moves the cursor to the first column of that line
        for (c = 0; c < COLUMN_N - 17; c++) // One character at a time,
        {
            data(Stringch[r][c]); //  displays the corresponding string
        }
    }
}

// _______________________________________________________________________________________

void OLED::OLED_menu1(void)
{
    command(0x01); // Clear display
    delay(2);      // After a clear display, a minimum pause of 1-2 ms is required
    output(MENU1);
    command(0xF3); /* move the cursor to the bottom last cell of the LCD */
    data(0x31);    /* output the menu number at the bottom right cell of the LCD */
    command(0xA0); /* move to position A0 */
    data(0X3E);    /* print a > at position A0 */
    menu = 1;      /* set the menu variable value so you know what menu you are on */
    command(0xA0); /* move the cursor to the first row of the menu */
    cursor = 0xA0; /* set current cursor location */
}
// _______________________________________________________________________________________

void OLED::OLED_cook(void) /* the COOK screen */
{
    command(0x01); // Clear display
    delay(2);      // After a clear display, a minimum pause of 1-2 ms is required
    output(COOK);
    menu = 2;      /* set the menu variable value so you know what menu you are on */
    command(0x80); /* move the cursor to the first row of the menu */
    cursor = 0x80; /* set current cursor location */
}
// _______________________________________________________________________________________

void OLED::update_OLED_mode(char mode[9], int modestate) /////////////////////////////////////////////////////////////// NEED to FIX THIS....just a stub
{
    //Serial.printf("OLED::update_OLED_mode %s %d\r\n", mode, modestate);
    if (menu == 2 || modestate == 0)
    {
        //Serial.println("in IF statement for update_OLED_mode menu=2");
        strcpy(MODE, mode);

        //strcpy(MODE, MODEarray[0]);

        //strncpy((char*)&ppid[0], buffer, sizeof(ppid[0]));

        //Serial.println("OLED::update_OLED_mode");
        //strncpy((char*)&MODER[0], mode, sizeof(MODER[0]));

        //Serial.printf("%s %s %s %s %s %s\r\n", MODER[0], MODER[1], MODER[2], MODER[3], MODER[4], MODER[5]);
        //Serial.printf("MODER is: %s\r\n", MODER);
        //Serial.printf("MODE is: %s\r\n", MODE);
        command(0x8C); /* move the cursor to the starting print cell of "set" or MODE section of the OLED */
        cursor = 0x8C; /* set the cursor location */
        outputMode(&MODER[modestate]);
    }
}
// _______________________________________________________________________________________

void OLED::update_OLED_pid(double uPID) /////////////////////////////////////////////////////////////// NEED to FIX THIS....just a stub
{
    //Serial.printf("OLED::update_OLED_pid - menu is: %d \r\n", menu);
    if (menu == 2)
    {
        char buffer[5];
        snprintf(buffer, sizeof(buffer), "%.2f", uPID);

        strncpy((char *)&ppid[0], buffer, sizeof(ppid[0]));
        //Serial.printf("PID is: %s\r\n", ppid);
        //Serial.printf("OLED::update_OLED_pid - in menu=2 if statement, uPID: %.2f\r\n", uPID);
        command(0xCC); /* move the cursor to the starting print cell of "pid" of the OLED */
        cursor = 0xCC; /* set the cursor location */
        outputPID(&ppid[0]);
    }
}

void OLED::update_FIA(int Fpin, int Ipin, int Apin)
{
    if (menu == 2)
    {
        command(0xD0); /* move the cursor to the starting print cell of "pid" of the OLED */
        data(0x20);    // prints a space
        command(0xD1);
        if (Fpin == 1)
        {
            data(0x46);
        }
        else
        {
            data(0x20);
        }
        if (Ipin == 1)
        {
            data(0x49);
        }
        else
        {
            data(0x20);
        }
        if (Apin == 1)
        {
            data(0x41);
        }
        else
        {
            data(0x20);
        }
    }
}

void OLED::update_OLED_target(float tt)
{

    targetTemp = tt;
    char buffer[1][4];

    if (menu == 2)
    {
        snprintf(buffer[0], sizeof(buffer), "%.0f", tt);
        strncpy((char *)&ttemp[3], buffer[0], sizeof(ttemp[3]));

        //Serial.printf("Length of all 4 ttemps %s %s %s %s\r\n", ttemp[0], ttemp[1], ttemp[2], ttemp[3]);

        command(0xE3);          /* move the cursor to the starting cell of TT Target temp */
        outputTTemp(&ttemp[3]); /* print the temp */
        if (tt < 100)
        {
            command(0xE5); /* move the cursor to the cell right after the 2-digit temp */
            data(0xDF);    /* print a degree symbol */
            command(0xE6);
            data(0x20);
        }
        else /* must be 3 digit temp.... we be BBQ-n! move to the end of the temp and print a degree symbol */
        {
            command(0xE6);
            data(0xDF); /* print a degree symbol */
        }
    }
}

void OLED::update_OLED_temps(float tt, float grillTemp, float meat1Temp, float meat2Temp) //////////////////////// I'm sure this can be written much better... LATER!  it works for now!
{
    char buffer[3][4];

    if (menu == 2)
    {
        snprintf(buffer[0], sizeof(buffer), "%.0f", grillTemp);
        snprintf(buffer[1], sizeof(buffer), "%.0f", meat1Temp);
        snprintf(buffer[2], sizeof(buffer), "%.0f", meat2Temp);

        strncpy((char *)&ttemp[0], buffer[0], sizeof(ttemp[0]));
        strncpy((char *)&ttemp[1], buffer[1], sizeof(ttemp[1]));
        strncpy((char *)&ttemp[2], buffer[2], sizeof(ttemp[2]));

        command(0x83);          /* move the cursor to the starting cell of GRILL temp */
        outputTTemp(&ttemp[0]); /* print the temp */

        if (grillTemp < 10) /* need to deal with single digit temps or negative (error) temps with two digit mask -- */
        {
            command(0x83); /* move to the 1st digit of grill temp */
            data(0x2D);    /* print a - */
            command(0x84); /* move to the space right after the 1-digit grill temp */
            data(0x2D);    /* print a -  */
        }

        if (grillTemp < 100) /* need to deal with 2 digit temps and this prints a degree symbol used by 1-digit temps */
        {
            command(0x85); /*move the cursor to the cell right after the 2-digit temp */
            data(0xDF);    /* print a degree symbol */
            command(0x86);
            data(0x20);
        }
        else
        {
            command(0x86);
            data(0xDF); /* print a degree symbol for a 3 digit temp */
        }

        command(0xA3);          /* move the cursor to the starting cell of Meat1 temp */
        outputTTemp(&ttemp[1]); /* print the temp */

        if (meat1Temp < 10) /* need to deal with single digit temps or negative (error) temps with two digit mask -- */
        {
            command(0xA3); /* move to the 1st digit space of meat1 temp */
            data(0x2D);    /* print a - */
            command(0xA4); /* move to the space right after the 1-digit meat1 temp */
            data(0x2D);    /* print a -  */
        }

        if (meat1Temp < 100)
        {
            command(0xA5); /* move the cursor to the cell right after the 2-digit temp */
            data(0xDF);    /* print a degree symbol */
            command(0xA6);
            data(0x20);
        }
        else /* must be 3 digit temp.... we be BBQ-n! move to the end of the temp and print a degree symbol */
        {
            command(0xA6);
            data(0xDF); /* print a degree symbol */
        }

        command(0xC3);          /* move the cursor to the starting cell of Meat2 temp */
        outputTTemp(&ttemp[2]); /* print the temp */

        if (meat2Temp < 10) /* need to deal with single digit temps or negative (error) temps with two digit mask -- */
        {
            command(0xC3); /* move to the 1st digit space of meat2 temp */
            data(0x2D);    /* print a - */
            command(0xC4); /* move to the space right after the 1-digit meat2 temp */
            data(0x2D);    /* print a -  */
        }

        if (meat2Temp < 100)
        {
            command(0xC5); /* move the cursor to the cell right after the 2-digit temp */
            data(0xDF);    /* print a degree symbol */
            command(0xC6);
            data(0x20);
        }
        else /* must be 3 digit temp.... we be BBQ-n! move to the end of the temp and print a degree symbol */
        {
            command(0xC6);
            data(0xDF); /* print a degree symbol */
        }
    }
}

void toggleMode()
{
}

void OLED::buttonChangeTarget(bool direction)
{
    if (direction && (targetTemp + 5 <= 500.0))
    {
        targetTemp += 5;
        OLED::update_OLED_target(targetTemp);
    }

    if (!direction && (targetTemp - 5 >= 10.0))
    {
        targetTemp += -5;
        update_OLED_target(targetTemp);
    }
    Serial.printf("New button target temp is: %.0f\r\n", targetTemp);
}

void OLED::buttonPress(void)
{
    // Update button state
    buttonUp.Update();
    buttonDn.Update();
    buttonLt.Update();
    buttonRt.Update();
    buttonSel.Update();

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonUp.clicks != 0)
        function = buttonUp.clicks;
    if (function == 1)
    {
        Serial.println("****** UP SINGLE click");
        menu1();
        if (menu == 2)
        {
            buttonChangeTarget(TRUE); /* decrement the Target temp by the constant value stored in incrementDown */
        }
    }
    if (function == 2)
        Serial.println("UP DOUBLE click");
    if (function == 3)
        Serial.println("UP TRIPLE click");
    if (function == -1)
        Serial.println("UP SINGLE LONG click");
    if (function == -2)
        Serial.println("UP DOUBLE LONG click");
    if (function == -3)
        Serial.println("UP TRIPLE LONG click");
    function = 0;
    delay(5);
    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonDn.clicks != 0)
        function = buttonDn.clicks;
    if (function == 1)
    {
        Serial.println("****** DN SINGLE click");
        menu1();
        if (menu == 2)
        {
            buttonChangeTarget(FALSE); /* decrement the Target temp by the constant value stored in incrementDown */
        }
    }
    if (function == 2)
        Serial.println("DN DOUBLE click");
    if (function == 3)
        Serial.println("DN TRIPLE click");
    if (function == -1)
        Serial.println("DN SINGLE LONG click");
    if (function == -2)
        Serial.println("DN DOUBLE LONG click");
    if (function == -3)
        Serial.println("DN TRIPLE LONG click");
    function = 0;
    delay(5);

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonLt.clicks != 0)
        function = buttonLt.clicks;
    if (function == 1)
    {
        Serial.println("****** LEFT SINGLE click");
        if (menu != 1)
        {
            OLED_menu1();
        }
    }

    if (function == 2)
        Serial.println("LEFT DOUBLE click");
    if (function == 3)
        Serial.println("LEFT TRIPLE click");
    if (function == -1)
        Serial.println("LEFT SINGLE LONG click");
    if (function == -2)
        Serial.println("LEFT DOUBLE LONG click");
    if (function == -3)
        Serial.println("LEFT TRIPLE LONG click");
    function = 0;
    delay(5);

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonRt.clicks != 0)
        function = buttonRt.clicks;
    if (function == 1)
    {
        Serial.println("****** RIGHT SINGLE click");
        if (menu == 2)
        {
            if (ModeState == 5)
            {
                ModeState = 0;
                strcpy(MODEbutton, MODEarray[0]);
                Serial.printf("ModeState == 0 %s", MODEbutton);
            }
            else
            {
                ModeState += 1;
                strcpy(MODEbutton, MODEarray[ModeState]);
                Serial.printf("ModeState == %d %s", ModeState, MODEbutton);
            }

            update_OLED_mode(MODEbutton, ModeState);
        }
    }
    if (function == 2)
        Serial.println("RIGHT DOUBLE click");
    if (function == 3)
        Serial.println("RIGHT TRIPLE click");
    if (function == -1)
        Serial.println("RIGHT SINGLE LONG click");
    if (function == -2)
        Serial.println("RIGHT DOUBLE LONG click");
    if (function == -3)
        Serial.println("RIGHT TRIPLE LONG click");
    function = 0;
    delay(5);

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonSel.clicks != 0)
        function = buttonSel.clicks;
    if (function == 1)
    {
        Serial.println("****** SELECT SINGLE click");
        if (menu == 1)
        {
            if (cursor == 0xA0) // if cursor is sitting on menu option 1. Start a Cook, then a select press at this time would display the LCD_cook() menu!  Let's make some BBQ for hungry Pirates!
                //Serial.println("DEBUG: Displaying Cook Menu");
                OLED_cook();
        }
        if (menu == 2)
        {
            update_OLED_target(targetTemp);
            update_OLED_mode(MODE, ModeState);

            //strcpy(newmode, myOLED.MODE);     // WORKING ON THESE TWO ITEMS FROM THE MAIN PAGE ROUTINE
            //modeState = myOLED.ModeState;
        }
    }

    if (function == 2)
        Serial.println("SELECT DOUBLE click");
    if (function == 3)
        Serial.println("SELECT TRIPLE click");
    if (function == -1)
        Serial.println("SELECT SINGLE LONG click");
    if (function == -2)
        Serial.println("SELECT DOUBLE LONG click");
    if (function == -3)
    {
        Serial.println("SELECT TRIPLE LONG click");
        OLED_menu1();
    }
    function = 0;
    delay(5);
}

void OLED::menu1(void)
{
    if (menu == 1)
    {
        if (cursor == 0xA0)
        {
            command(0xA0);
            data(0x20);    /* print a space to cover the Chevron and move down one */
            command(0xC0); /* move the cursor to the second row of menu 1 */
            data(0x3E);    /* print a > */
            command(0xC0);
            cursor = 0xC0;
        }
        else
        {
            if (cursor == 0xC0)
            {
                command(0xC0);
                data(0x20);    /* print a space to cover the Chevron and move up one */
                command(0xA0); /* move the cursor to the 1st row of menu 1 */
                data(0x3E);    /* print a > */
                command(0xA0);
                cursor = 0xA0;
            }
        }
    }
}

void OLED::OLED_init(void)
{
    pinMode(D2, INPUT_PULLUP); // Left Button
    pinMode(D3, INPUT_PULLUP); // Right Button
    pinMode(D4, INPUT_PULLUP); // Up Button
    pinMode(D5, INPUT_PULLUP); // Down Button
    pinMode(D6, INPUT_PULLUP); // Select button

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    buttonUp.debounceTime = 20;    // Debounce timer in ms
    buttonUp.multiclickTime = 250; // Time limit for multi clicks
    buttonUp.longClickTime = 2000; // time until "held-down clicks" register

    buttonDn.debounceTime = 20;    // Debounce timer in ms
    buttonDn.multiclickTime = 250; // Time limit for multi clicks
    buttonDn.longClickTime = 2000; // time until "held-down clicks" register

    buttonLt.debounceTime = 20;    // Debounce timer in ms
    buttonLt.multiclickTime = 250; // Time limit for multi clicks
    buttonLt.longClickTime = 2000; // time until "held-down clicks" register

    buttonRt.debounceTime = 20;    // Debounce timer in ms
    buttonRt.multiclickTime = 250; // Time limit for multi clicks
    buttonRt.longClickTime = 2000; // time until "held-down clicks" register

    buttonSel.debounceTime = 20;    // Debounce timer in ms
    buttonSel.multiclickTime = 250; // Time limit for multi clicks
    buttonSel.longClickTime = 2000; // time until "held-down clicks" register

    //   pinMode(RES, OUTPUT);            // Initializes Arduino pin for the Reset line (optional)
    //   digitalWrite(RES, HIGH);         // Sets HIGH the Reset line of the display (optional, can be always high)
    delayMicroseconds(200); // Waits 200 us for stabilization purpose
    Wire.begin();           // Initiate the Wire library and join the I2C bus as a master
    delay(20);              // Waits 20 ms for stabilization purpose

    if (ROW_N == 2 || ROW_N == 4)
        rows = 0x08; // Display mode: 2/4 lines
    else
        rows = 0x00; // Display mode: 1/3 lines

    command(0x22 | rows); // Function set: extended command set (RE=1), lines #
    command(0x71);        // Function selection A:
    data(0x5C);           //  enable internal Vdd regulator at 5V I/O mode (def. value) (0x00 for disable, 2.8V I/O)
    command(0x20 | rows); // Function set: fundamental command set (RE=0) (exit from extended command set), lines #
    command(0x08);        // Display ON/OFF control: display off, cursor off, blink off (default values)
    command(0x22 | rows); // Function set: extended command set (RE=1), lines #
    command(0x79);        // OLED characterization: OLED command set enabled (SD=1)
    command(0xD5);        // Set display clock divide ratio/oscillator frequency:
    command(0x70);        //  divide ratio=1, frequency=7 (default values)
    command(0x78);        // OLED characterization: OLED command set disabled (SD=0) (exit from OLED command set)

    if (ROW_N > 2)
        command(0x09); // Extended function set (RE=1): 5-dot font, B/W inverting disabled (def. val.), 3/4 lines
    else
        command(0x08); // Extended function set (RE=1): 5-dot font, B/W inverting disabled (def. val.), 1/2 lines

    command(0x06);        // Entry Mode set - COM/SEG direction: COM0->COM31, SEG99->SEG0 (BDC=1, BDS=0)
    command(0x72);        // Function selection B:
    data(0x0A);           //  ROM/CGRAM selection: ROM C, CGROM=250, CGRAM=6 (ROM=10, OPR=10)
    command(0x79);        // OLED characterization: OLED command set enabled (SD=1)
    command(0xDA);        // Set SEG pins hardware configuration:
    command(0x10);        //  alternative odd/even SEG pin, disable SEG left/right remap (default values)
    command(0xDC);        // Function selection C:
    command(0x00);        //  internal VSL, GPIO input disable
    command(0x81);        // Set contrast control:
    command(0x7F);        //  contrast=127 (default value)
    command(0xD9);        // Set phase length:
    command(0xF1);        //  phase2=15, phase1=1 (default: 0x78)
    command(0xDB);        // Set VCOMH deselect level:
    command(0x40);        //  VCOMH deselect level=1 x Vcc (default: 0x20=0,77 x Vcc)
    command(0x78);        // OLED characterization: OLED command set disabled (SD=0) (exit from OLED command set)
    command(0x20 | rows); // Function set: fundamental command set (RE=0) (exit from extended command set), lines #
    command(0x01);        // Clear display
    delay(2);             // After a clear display, a minimum pause of 1-2 ms is required
    command(0x80);        // Set DDRAM address 0x00 in address counter (cursor home) (default value)
    command(0x0C);        // Display ON/OFF control: display ON, cursor off, blink off
    delay(250);           // Waits 250 ms for stabilization purpose after display on

    if (ROW_N == 2)
        new_line[1] = 0xC0; // DDRAM address for each line of the display (only for 2-line mode)

    output(SPLASH); // Execute subroutine "output"
    delay(5000);    // Waits, only for visual effect purpose
    OLED_menu1();   // display the main menu
    delay(250);
}
