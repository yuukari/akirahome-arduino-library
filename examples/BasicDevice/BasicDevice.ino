#include <akirahome.h>;

#define SSID "ssid"
#define PASSWORD "password"
#define HOSTNAME "example-device.lan"

Akirahome akirahome;

char strbuf[150];

struct {
  bool power = false;
} DeviceState;

device_t device = {
  "Example device",
  "d197dc347e94ea7997fb8b7b443e3e0a9205a58f07514d359b3f6a5f19da8403"
};

//* -------------------- Power -------------------- */

void powerInfoHandler(JsonObject state){  
  /* Your state logic here */

  state["value"] = DeviceState.power;
}

bool powerStateHandler(JsonObject state){
  DeviceState.power = state["value"];

  /* Your action logic here */

  sprintf(strbuf, "Power state changed to %s", state["value"] == true ? "ON" : "OFF");
  akirahome.writeLog(strbuf, AKIRAHOME_INFO_LOG);
  
  return true;
}

/* -------------------- Fields -------------------- */

field_t fields[1] = {
  {
    "power",
    "akirahome.fields.switch",

    &powerInfoHandler,
    &powerStateHandler
  }
};

void setup(){
  akirahome.init(device, fields, sizeof(fields) / sizeof(field_t));
  akirahome.setHostname(HOSTNAME);
  akirahome.connect(SSID, PASSWORD);
  
  akirahome.enableOTA("ota_password");
}

void loop(){
  akirahome.handleClient();  
  delay(200);
}