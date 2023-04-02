#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPClient.h>
#else
  #include <WiFi.h>
  #include <WebServer.h>
  #include <HTTPClient.h>
#endif

#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#define AKIRAHOME_VER "1.0.3"

#define AKIRAHOME_INFO_LOG 0
#define AKIRAHOME_WARN_LOG 1
#define AKIRAHOME_ERR_LOG 2

typedef void (*fieldInfoHandler_t)(JsonObject);
typedef bool (*fieldStateHandler_t)(JsonObject);
typedef void (*hookHandler_t)();

struct device_t {
  char* name;
  char* uid;
};

struct field_t {
  const char* id;
  const char* type;

  fieldInfoHandler_t infoHandler;
  fieldStateHandler_t stateHandler;
};

struct networkInfo_t {
  const char* ssid;
  const char* password;

  const char* hostname;

  IPAddress address;
  IPAddress gateway;
  IPAddress netmask;

  boolean hasIP;
  boolean hasHostname;
};

class Akirahome {
  public:
    /* ---------- Core API ---------- */
    
    void init(device_t d, field_t* fa, unsigned short fc);
    void setIP(IPAddress address, IPAddress gateway, IPAddress netmask);
    void setHostname(const char* hostname);
    void connect(const char* ssid, const char* password);
    void handleClient();

    void enableOTA(const char* password);
    
    void setPreHook(hookHandler_t handler);
    void setPostHook(hookHandler_t handler);

    void sendUpdateRequest(char* url, JsonObject state);

    /* ----------- Utils ------------ */

    void writeLog(char* message, byte type);
  private:
    /* ---------- Core API ---------- */

    #ifdef ESP8266
      ESP8266WebServer server;
    #else
      WebServer server;
    #endif
 
    device_t device;   
    field_t* fields;
    unsigned short fieldsCount;
    
    networkInfo_t networkInfo;

    hookHandler_t preHookHandler = NULL;
    hookHandler_t postHookHandler = NULL;

    void reconnect();

    void handleIndex();
    void handleInfo();
    void handleState();

    unsigned short getField(const char* id, field_t* &field);
    void sendError(char* message, unsigned int code);

    /* ----------- Telnet ----------- */

    WiFiServer* telnet;
    WiFiClient telnetClient;
    String telnetString;
    
    void handleTelnet();

    /* --------- OTA update --------- */

    unsigned char updateSpinnerStep = 0;
    unsigned char updateSpinnerDelay = 0;

    void handleOTAStart();
    void handleOTAEnd();
    void handleOTAProgress(unsigned int progress, unsigned int total);
    void handleOTAError(ota_error_t error);

    const char* getUpdateSpinner();

    /* ----------- Helpers ---------- */

    String getFormattedMessage(char* message, byte type);
};