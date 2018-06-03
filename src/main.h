#define TIMENOW Time.now() + 0.1; // needed because of BUG in JSON parser where it can't parse a float into a variable without a decimal point

// Forward declarations
//int devicesHandler(String data); // forward declaration
//void sendData(void);
void wifi_scan_callback(WiFiAccessPoint* wap, void* data);
void cloudConnect(void);
void getDataHandler(const char *event, const char *data);
void hopperInit(void);
void SetMode(void);
void handler(const char *topic, const char *data);
//void tempsHandler(const char *topic, const char *data);
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
void l3PopCallback(void *ptr);
void l4PopCallback(void *ptr);
int WifiSignalStrength(int);

/* END of Nextion Forward Declarations*/

