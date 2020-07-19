

#define ARDUINOJSON_ENABLE_STD_STRING 1


#include <string>

#include <ArduinoJson.h>
#include <uTimerLib.h>
#include <CheapStepper.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

bool restart = false;

typedef struct {
	std::string ssid;
  	std::string pass;
} wifiConfig;

typedef struct {
	std::string user;
	std::string pass;
} authConfig;

typedef struct {
  std::string hostname;
  wifiConfig wifi;
  authConfig auth;
} Config;


// SKETCH BEGIN
Config config;
AsyncWebServer server(80);
CheapStepper stepper(14,12,13,15);

void convert_string(std::string &output, String input) {
	size_t len = input.length();
	output.reserve(len);
	for (size_t i = 0; i < len; i++) {
		output[i] = input.charAt(i);
	}
}

void stepper_run() {
	stepper.run();
}

// Loads the configuration from a file
void loadConfiguration() {
	
	// Open file for reading
	File f = LittleFS.open("config", "r");

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/v6/assistant to compute the capacity.
	StaticJsonDocument<512> doc;

	// Deserialize the JSON document
	DeserializationError error = deserializeJson(doc, f);
	if (error) Serial.println(F("Failed to read file, using default configuration"));

	JsonObject obj = doc.as<JsonObject>();

	// Copy values from the JsonDocument to the Config
	config.hostname  = obj["hostname"].as<std::string>();
	config.wifi.ssid = obj["wifi"]["ssid"].as<std::string>();
	config.wifi.pass = obj["wifi"]["pass"].as<std::string>();
	config.auth.user = obj["auth"]["user"].as<std::string>();
	config.auth.pass = obj["auth"]["pass"].as<std::string>();

	// Close the file (Curiously, File's destructor doesn't close the file)
	f.close();
}

// Saves the configuration to a file
void saveConfiguration() {

	// Open file for writing
	File f = LittleFS.open("config", "w");
	if (!f) {
		Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	StaticJsonDocument<256> doc;

	// Set the values in the document
	doc["hostname"]     = config.hostname;
	doc["wifi"]["ssid"] = config.wifi.ssid;
	doc["wifi"]["pass"] = config.wifi.pass;
	doc["auth"]["user"] = config.auth.user;
	doc["auth"]["pass"] = config.auth.pass;
	
	// Serialize JSON to file
	if (serializeJson(doc, f) == 0) Serial.println(F("Failed to write to file"));

	// Close the file
	f.close();
}

void setup(){

	// Setup debug port
	Serial.begin(74880);
	Serial.setDebugOutput(true);

	// Mount FS
	LittleFS.begin();

	// Load configuration from file
	loadConfiguration();
	Serial.print("ssid: "); Serial.println(config.wifi.ssid.c_str());
	Serial.print("pass: "); Serial.println(config.wifi.pass.c_str());

	// Stepper
	stepper.setRpm(10);
	stepper.setSpr(4096);
	TimerLib.setInterval_us(stepper_run, stepper.interval());

	// Setup wifi connection
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP(config.hostname.c_str());
	WiFi.begin(config.wifi.ssid.c_str(), config.wifi.pass.c_str());
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.printf("STA: Failed!\n");
		WiFi.disconnect(false);
		delay(1000);
		WiFi.begin(config.wifi.ssid.c_str(), config.wifi.pass.c_str());
	}

	//Send OTA events to the browser
	ArduinoOTA.onStart([]() {
		Serial.println("Start");  //  "Начало OTA-апдейта"
	});

	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");  //  "Завершение OTA-апдейта"
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
    	if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");    //  "Ошибка при аутентификации"
    	else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");   //  "Ошибка при начале OTA-апдейта"
    	else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed"); //  "Ошибка при подключении"
    	else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed"); //  "Ошибка при получении данных"
    	else if (error == OTA_END_ERROR)     Serial.println("End Failed");     //  "Ошибка при завершении OTA-апдейта"
	});

	ArduinoOTA.setHostname(config.hostname.c_str());
	ArduinoOTA.begin();

	MDNS.addService("http", "tcp", 80);

	server.addHandler(new SPIFFSEditor(config.auth.user.c_str(), config.auth.pass.c_str(), LittleFS));

	server
		.serveStatic("/", LittleFS, "/www/")                      // Включаем предоставление файлов из флеш памяти
		.setDefaultFile("index.html")                             // Страница по умолчанию
		.setAuthentication(config.auth.user.c_str(), config.auth.pass.c_str());   // Включение базовой аутентификации

	server.onNotFound([](AsyncWebServerRequest *request){
		Serial.printf("NOT_FOUND: ");
		if      (request->method() == HTTP_GET    )    Serial.printf("GET");
		else if (request->method() == HTTP_POST   )    Serial.printf("POST");
		else if (request->method() == HTTP_DELETE )    Serial.printf("DELETE");
		else if (request->method() == HTTP_PUT    )    Serial.printf("PUT");
		else if (request->method() == HTTP_PATCH  )    Serial.printf("PATCH");
		else if (request->method() == HTTP_HEAD   )    Serial.printf("HEAD");
		else if (request->method() == HTTP_OPTIONS)    Serial.printf("OPTIONS");
		else                                           Serial.printf("UNKNOWN");

		Serial.printf("http://%s%s\n", request->host().c_str(), request->url().c_str());

		if (request->contentLength()){
			Serial.printf("_CONTENT_TYPE  : %s\n", request->contentType().c_str());
			Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
		}

		int headers = request->headers();
		for (int i=0; i<headers; i++) {
			AsyncWebHeader* h = request->getHeader(i);
			Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
		}

		int params = request->params();
		for (int i=0; i<params; i++) {
			AsyncWebParameter* p = request->getParam(i);
			if      (p->isFile()) Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
			else if (p->isPost()) Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
			else                  Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
		}

		request->send(404);
	});

	server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
		if (!index) Serial.printf("BodyStart: %u\n", total);
		Serial.printf("%s", (const char*) data);
		if (index + len == total) Serial.printf("BodyEnd: %u\n", total);
	});

	server.on("/stepper", HTTP_GET, [](AsyncWebServerRequest *request){
		if (!request->authenticate(config.auth.user.c_str(), config.auth.pass.c_str())) 
			return request->requestAuthentication();		

		// Check if parameters exists
		if (!request->hasParam("mode") || !request->hasParam("value"))
			request->send(200, "application/json", "{result='Fail'}");
  			
		String mode = request->getParam("mode")->value();
		long value  = request->getParam("value")->value().toInt();

		if (value == 0) return request->send(200, "application/json", "{result='Fail'}");

		if (mode == "movecw") {
			stepper.moveCW(value);
			Serial.println("movecw");
			return request->send(200, "application/json", "{result='OK'}");
		} else if (mode == "moveccw") {
			stepper.moveCCW(value);
			Serial.println("moveccw");
			return request->send(200, "application/json", "{result='OK'}");
		}
	});

	// First request will return 0 results unless you start scan from somewhere else (loop/setup)
	// Do not request more often than 3-5 seconds
	server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
		if(!request->authenticate(config.auth.user.c_str(), config.auth.pass.c_str())) 
			return request->requestAuthentication();
			
		String json = "[";
		int n = WiFi.scanComplete();
		if (n == -2) {
			WiFi.scanNetworks(true);
		} else if (n) {
			for (int i = 0; i < n; ++i){
				if (i) json += ",";
				json += "{\"rssi\":"    + String(WiFi.RSSI(i));
				json += ",\"ssid\":\""  + WiFi.SSID(i)     + "\"";
				json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
				json += ",\"channel\":" + String(WiFi.channel(i));
				json += ",\"secure\":"  + String(WiFi.encryptionType(i));
				json += ",\"hidden\":"  + String(WiFi.isHidden(i)?"true":"false");
				json += "}";
			}
			WiFi.scanDelete();
			if (WiFi.scanComplete() == -2) {
				WiFi.scanNetworks(true);
			}
		}
		json += "]";
		request->send(200, "application/json", json);
	});

	// Set wifi ssid & password
	server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
		if(!request->authenticate(config.auth.user.c_str(), config.auth.pass.c_str())) 
			return request->requestAuthentication();

		Serial.println("save");
		int params = request->params();
		for(int i=0;i<params;i++){
  			AsyncWebParameter* p = request->getParam(i);
  			if(p->isFile()){ //p->isPost() is also true
    			Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
  			} else if(p->isPost()){
    			Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  			} else {
    			Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
  			}
		}
			
		String result = "Fail";
		String error_msg;

		if (!request->hasArg("ssid")) {
			error_msg = "ssid not filled out.";
		} else if (!request->hasArg("pass")) {
			error_msg = "password not filled out.";
		} else {
			String ssid = request->arg("ssid");
			config.wifi.ssid.reserve(ssid.length());
			for (int i = 0; i < ssid.length(); i++) config.wifi.ssid[i] = ssid.charAt(i);
			convert_string(config.wifi.ssid, request->arg("ssid"));
			convert_string(config.wifi.pass, request->arg("pass"));
//			saveConfiguration();
		}
		
		request->send(200, "application/json", "{\"result\"=\"" + result + "\",\"error\"=\"" + error_msg + "\"}");
	});

	server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
		if(!request->authenticate(config.auth.user.c_str(), config.auth.pass.c_str())) 
			return request->requestAuthentication();
			
		Serial.println("Reset..");
		restart = true;
		request->send(200, "application/json", "{\"result\"=\"Ok\"}");
	});

	server.begin();
}

void loop(){
	ArduinoOTA.handle();

	if (restart) ESP.restart();
}
