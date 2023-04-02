#include "akirahome.h";

void Akirahome::init(device_t d, field_t* fa, unsigned short fc){
  device = d;
  fields = fa;
  fieldsCount = fc;

  networkInfo = {
    NULL,
    NULL,

    NULL,

    0,
    0,
    0,

    false,
    false
  };
  
  Serial.begin(115200);

  Serial.println(F("\r\n    ___    __   _            __                       \r\n   /   |  / /__(_)________ _/ /_  ____  ____ ___  ___ \r\n  / /| | / //_/ / ___/ __ `/ __ \\/ __ \\/ __ `__ \\/ _ \\\r\n / ___ |/ ,< / / /  / /_/ / / / / /_/ / / / / / /  __/\r\n/_/  |_/_/|_/_/_/   \\__,_/_/ /_/\\____/_/ /_/ /_/\\___/ "));
  Serial.print(F("\r\n ----- This device running Akirahome SDK v"));
  Serial.print(AKIRAHOME_VER);
  Serial.println(F(" ----- "));
  
  Serial.print("\r\n> Starting device '");
  Serial.print(device.name);
  Serial.println("'");
  
  server.on("/", [&](){ handleIndex(); });
  server.on("/info", [&](){ handleInfo(); });
  server.on("/state", [&](){ handleState(); });
  server.begin();

  telnet = new WiFiServer(23);
  telnet->setNoDelay(true);
  telnet->begin();
  
  Serial.println("> Server started");
}

void Akirahome::setIP(IPAddress address, IPAddress gateway, IPAddress netmask){
  networkInfo.address = address;
  networkInfo.gateway = gateway;
  networkInfo.netmask = netmask;
  networkInfo.hasIP = true;
}

void Akirahome::setHostname(const char* hostname){
  networkInfo.hostname = hostname;
  networkInfo.hasHostname = true;
}

void Akirahome::connect(const char* ssid, const char* password){
  networkInfo.ssid = ssid;
  networkInfo.password = password;
  
  WiFi.mode(WIFI_STA);

  if (networkInfo.hasIP)
    WiFi.config(networkInfo.address, networkInfo.gateway, networkInfo.netmask);

  if (networkInfo.hasHostname)
    WiFi.hostname(networkInfo.hostname);
    
  WiFi.begin(networkInfo.ssid, networkInfo.password);

  Serial.print("> Connecting to ");
  Serial.print(networkInfo.ssid);
  Serial.print(" ");
  
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println(" OK");

  Serial.print("> Device IP address: ");
  Serial.println(WiFi.localIP());
}

void Akirahome::handleClient(){
  if (WiFi.status() != WL_CONNECTED)
    reconnect();
  
  server.handleClient();
  handleTelnet();
  ArduinoOTA.handle();
}

void Akirahome::setPreHook(hookHandler_t handler){
  preHookHandler = handler;
}

void Akirahome::setPostHook(hookHandler_t handler){
  postHookHandler = handler;
}

void Akirahome::sendUpdateRequest(char* url, JsonObject state){
  
}

void Akirahome::reconnect(){
  WiFi.reconnect();
  
  Serial.print("> WiFi has been disconnected, trying reconnect ");
  
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(3000);
  }
  
  Serial.println(" OK");
}

void Akirahome::handleIndex(){
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void Akirahome::handleInfo(){
  char json[2048];
  
  DynamicJsonDocument response(2048);
  response["status"] = "ok";
  
  JsonObject payload = response.createNestedObject("payload");
  payload["name"] = device.name;
  payload["uid"] = device.uid;
  payload["firmware"] = AKIRAHOME_VER;
  
  JsonArray payloadFields = payload.createNestedArray("fields");

  for (unsigned short i = 0; i < fieldsCount; i++){
    JsonObject field = payloadFields.createNestedObject();
    field["id"] = fields[i].id;
    field["type"] = fields[i].type;

    JsonObject state = field.createNestedObject("state");

    fieldInfoHandler_t handler = fields[i].infoHandler;
    handler(state);
  }
  
  serializeJson(response, json);
  server.send(200, "application/json", json);
}

void Akirahome::handleState(){
  if (server.method() != HTTP_POST){
    sendError("Only POST requests allowed", 400);
    return;
  }

  if (preHookHandler != NULL)
    preHookHandler();
  
  StaticJsonDocument<512> request;
  deserializeJson(request, server.arg("plain"));
  JsonArray requestFields = request["fields"];

  char json[1024];
  DynamicJsonDocument response(1024);
  response["status"] = "ok";

  JsonObject payload = response.createNestedObject("payload");
  JsonArray payloadFields = payload.createNestedArray("fields");

  for (unsigned int i = 0; i < requestFields.size(); i++){
    const char* id = requestFields[i]["id"];
    field_t* deviceField;
    
    JsonObject payloadField = payloadFields.createNestedObject();
    payloadField["id"] = id;

    if (getField(id, deviceField)){
      fieldStateHandler_t handler = deviceField->stateHandler;
      
      if (handler(requestFields[i]["state"])){
        payloadField["result"] = "ok";
      } else {
        payloadField["result"] = "error";
        payloadField["error"] = "HANDLER_INTERNAL_ERROR";
      }
    } else {
      payloadField["result"] = "error";
      payloadField["error"] = "FIELD_NOT_FOUND";
    }
  }

  if (postHookHandler != NULL)
    postHookHandler();

  serializeJson(response, json);
  server.send(200, "application/json", json);
}

unsigned short Akirahome::getField(const char* id, field_t* &field){
  for (unsigned short i = 0; i < fieldsCount; i++)
    if (strcmp(fields[i].id, id) == 0){
      field = &fields[i];
      return true;
    }

  return false;
}

void Akirahome::sendError(char* message, unsigned int code = 500){
  char json[256];
  
  DynamicJsonDocument response(256);
  response["status"] = "error";
  response["error_message"] = message;

  serializeJson(response, json);
  server.send(code, "application/json", json);
}

/* ---------------------------------- Telnet ---------------------------------- */

void Akirahome::handleTelnet(){  
  if (telnet->hasClient()){
    Serial.println("New telnet connection");
    
    if (!telnetClient || !telnetClient.connected()){
      if (telnetClient)
        telnetClient.stop();

      telnetClient = telnet->available();

      telnetClient.println(F("\r\n    ___    __   _            __                       \r\n   /   |  / /__(_)________ _/ /_  ____  ____ ___  ___ \r\n  / /| | / //_/ / ___/ __ `/ __ \\/ __ \\/ __ `__ \\/ _ \\\r\n / ___ |/ ,< / / /  / /_/ / / / / /_/ / / / / / /  __/\r\n/_/  |_/_/|_/_/_/   \\__,_/_/ /_/\\____/_/ /_/ /_/\\___/ "));
      telnetClient.print(F("\r\n ----- This device running Akirahome SDK v"));
      telnetClient.print(AKIRAHOME_VER);
      telnetClient.println(F(" ----- "));

      telnetClient.print("\r\nDevice '");
      telnetClient.print(device.name);
      telnetClient.print("' has ");
      telnetClient.print(fieldsCount);
      telnetClient.println(" fields:\r\n");

      for (unsigned int i = 0; i < fieldsCount; i++){
        telnetClient.print("Field ");
        telnetClient.print(i);
        telnetClient.print(":\r\n\tID: ");
        
        telnetClient.print(fields[i].id);
        telnetClient.print("\r\n\tType: ");
        telnetClient.print(fields[i].type);
        telnetClient.print("\r\n\r\n");
      }

      telnetClient.println(" ----------------------------------------------------\r\n");
    }
  }
}

/* -------------------------------- OTA Update -------------------------------- */

void Akirahome::enableOTA(const char* password){
  ArduinoOTA.setHostname(device.name);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([&](){ handleOTAStart(); });
  ArduinoOTA.onEnd([&](){ handleOTAEnd(); });
  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total){ handleOTAProgress(progress, total); });
  ArduinoOTA.onError([&](ota_error_t error){ handleOTAError(error); });
  ArduinoOTA.begin();
}

void Akirahome::handleOTAStart(){
  Serial.println("\r\n---------------- Starting OTA update ----------------\r\n");
}

void Akirahome::handleOTAEnd(){
  Serial.println("\r\n\r\n------------------ Device updated -------------------");
  Serial.println("\r\n> Rebooting...");
}

void Akirahome::handleOTAProgress(unsigned int progress, unsigned int total){
  Serial.printf("%s Update progress: %u%%\r", getUpdateSpinner(), (progress / (total / 100)));
}

void Akirahome::handleOTAError(ota_error_t error){
  Serial.printf("> Error: %u", error);
  Serial.println("---------------- Device not updated -----------------");
}

const char* Akirahome::getUpdateSpinner(){  
  String spinner = "";

  switch (updateSpinnerStep){
    case 0: spinner = "/"; break;
    case 1: spinner = "-"; break;
    case 2: spinner = "\\"; break;
    case 3: spinner = "|"; break;
  }
  
  updateSpinnerDelay++;
  
  if (updateSpinnerDelay > 3){
    updateSpinnerStep++;
    updateSpinnerDelay = 0;
  }

  if (updateSpinnerStep > 3)
    updateSpinnerStep = 0;

  return spinner.c_str();
}

/* --------------------------------- Helpers ---------------------------------- */

void Akirahome::writeLog(char* message, byte type){
  String text = getFormattedMessage(message, type);
  
  Serial.println(text);

  if (telnetClient && telnetClient.connected())
    telnetClient.println(text);
}

String Akirahome::getFormattedMessage(char* message, byte type){
  String text = "> [";

  switch (type){
    case AKIRAHOME_INFO_LOG: text += "Info"; break;
    case AKIRAHOME_WARN_LOG: text += "Warning"; break;
    case AKIRAHOME_ERR_LOG: text += "Error"; break;
  }

  text += "] ";
  text += message;

  return text;
}
