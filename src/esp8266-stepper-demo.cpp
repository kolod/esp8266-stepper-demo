// Copyright (C) 2020  Aleksandr kolodki <alexandr.kolodkin@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>
#include <EasyDDNS.h>
#include <TinyUPnP.h>
#include <CheapStepper.h>


// SKETCH BEGIN
AsyncWebServer server(80);
StaticJsonDocument<1500> config;
TinyUPnP tinyUPnP(20000);
bool restart = false;


enum class LoadMode {
	none    = 0b00000000,
	config  = 0b00000001,
	status  = 0b00000010,
	stepper = 0b00000100,
	all     = (config | status | stepper)
};

LoadMode modeFromString(String mode, LoadMode defaultMode = LoadMode::all);

LoadMode modeFromString(String mode, LoadMode defaultMode) {
	if (mode == "all")      return LoadMode::all;
	if (mode == "config")   return LoadMode::config;
	if (mode == "status")   return LoadMode::status;
	if (mode == "stepper")  return LoadMode::stepper;
	return defaultMode;
}

bool operator == (LoadMode c1, LoadMode c2) {
	return static_cast<std::underlying_type<LoadMode>::type>(c1) &
	       static_cast<std::underlying_type<LoadMode>::type>(c2);
}

// Loads the configuration from a file
void loadConfiguration() {

	// Open file for reading
	File f = LittleFS.open("config", "r");

	// Deserialize the JSON document
	DeserializationError error = deserializeJson(config, f);
	if (error) {
		Serial.println(F("Failed to read file, using default configuration"));
		Serial.println(error.c_str());
	}

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

	// Serialize JSON to file
	if (serializeJson(config, f) == 0) Serial.println(F("Failed to write to file"));

	// Close the file
	f.close();
}

void setup_wifi() {
	const char* hostname = config["hostname"];
	const char* new_ssid = config["wifi"]["new_ssid"];
	const char* new_pass = config["wifi"]["new_pass"];
	const char* ssid     = config["wifi"]["ssid"];
	const char* pass     = config["wifi"]["pass"];

	// Set station mode
	WiFi.mode(WIFI_STA);

	// Connect to the new network if the new_ssid is set.
	if (new_ssid) {
		WiFi.begin(new_ssid, new_pass);
		// If connection successful save new ssid & pass and exit
		if (WiFi.waitForConnectResult() == WL_CONNECTED) {
			Serial.printf_P(PSTR("New SSID: %s, New PASS: %s"), new_ssid, new_pass);
			config["wifi"]["ssid"] = new_ssid;
			config["wifi"]["pass"] = new_pass;
			config["wifi"].remove("new_ssid");
			config["wifi"].remove("new_pass");
			saveConfiguration();
			return;
		}
	}

	// Connect to existing network
	WiFi.begin(ssid, pass);

	// If connection failed create access point
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("STA: Failed!");
		WiFi.mode(WIFI_AP);
		WiFi.softAP(hostname, pass);
	}

	// Remove new_ssid & new_pass
	if (new_ssid) {
		config["wifi"].remove("new_ssid");
		if (new_pass) config["wifi"].remove("new_pass");
		saveConfiguration();
	}
}

void setup_ota() {
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

	ArduinoOTA.setHostname(config["hostname"].as<char*>());
	ArduinoOTA.begin();
}

void setup_upnp() {
		// DDNS
	EasyDDNS.service(config["ddns"]["provider"].as<String>());
	EasyDDNS.client(
		config["ddns"]["domain"].as<String>(),
		config["ddns"]["user"].as<String>(),
		config["ddns"]["pass"].as<String>()
	);

	// UPnP for HTTP
	tinyUPnP.addPortMappingConfig(
		WiFi.localIP(),
		config["upnp"]["localport"].as<int>(),
		RULE_PROTOCOL_TCP,
		config["upnp"]["lease"].as<int>(),
		config["upnp"]["name"].as<String>()
	);

	// UPnP for ArduinoOTA
	tinyUPnP.addPortMappingConfig(
		WiFi.localIP(),
		8266,                                    // TODO: default OTA port hardcoded
		RULE_PROTOCOL_TCP,
		config["upnp"]["lease"].as<int>(),
		config["upnp"]["name"].as<String>() + "OTA"
	);

	boolean portMappingAdded = false;
	while (!portMappingAdded) {
		portMappingAdded = tinyUPnP.commitPortMappings();
		Serial.println("");

		if (!portMappingAdded) {
			// for debugging, you can see this in your router too under forwarding or UPnP
			tinyUPnP.printAllPortMappings();
			Serial.println(F("This was printed because adding the required port mapping failed"));
			delay(30000);  // 30 seconds before trying again
		}
	}

	Serial.println("UPnP done");
}

void setup(){

	// Setup debug port
	Serial.begin(74880);
	Serial.setDebugOutput(true);

	// Mount FS
	LittleFS.begin();

	// Load configuration from file
	loadConfiguration();

	// Stepper
	Stepper.init(14, 12, 13, 15, config["stepper"]["spr"].as<int>(), config["stepper"]["rpm"].as<int>());

	// Setup wifi connection
	setup_wifi();

	// Setup OTA
	setup_ota();

	// Setup UPnP
//	setup_upnp();

	// HTTP Server
	MDNS.addService("http", "tcp", 80);

	server.addHandler(new SPIFFSEditor(config["auth"]["user"].as<char*>(), config["auth"]["pass"].as<char*>(), LittleFS));

	server
		.serveStatic("/", LittleFS, "/www/")                      // Включаем предоставление файлов из флеш памяти
		.setDefaultFile("index.html")                             // Страница по умолчанию
		.setAuthentication(config["auth"]["user"].as<char*>(), config["auth"]["pass"].as<char*>());   // Включение базовой аутентификации

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

	server.on("/api", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
		if (!request->authenticate(config["auth"]["user"].as<char*>(), config["auth"]["pass"].as<char*>()))
			return request->requestAuthentication();

		StaticJsonDocument<2000> doc;

		if (!request->hasParam("command")) {
			doc["result"] = "Fail";
			doc["msg"]    = "Command not spesified";
		}

		String command = request->getParam("command")->value();

		if (command == "reboot") {
			restart = true;
			Serial.println("Reset..");
			doc["result"] = "Ok";

		} else if (command == "scan") {
			// First request will return 0 results unless you start scan from somewhere else (loop/setup)
			// Do not request more often than 3-5 seconds

			if (WiFi.isConnected()) {
				doc["ssid"] = WiFi.SSID();
			}

			int n = WiFi.scanComplete();

			if (n == -2) {
				WiFi.scanNetworks(true);
				doc["result"] = "Ok";

			} else if (n > 0) {
				JsonArray arr = doc.createNestedArray("wifi");

				for (int i = 0; i < n; ++i) {
					JsonObject obj = arr.createNestedObject();
					obj["ssid"]    = WiFi.SSID(i);
					obj["rssi"]    = WiFi.RSSI(i);
					obj["channel"] = WiFi.channel(i);
					obj["secure"]  = WiFi.encryptionType(i);
				}

				WiFi.scanDelete();
				if (WiFi.scanComplete() == -2) {
					WiFi.scanNetworks(true);
				}

				doc["result"] = "Ok";
			}

		} else if (command == "save") {
			if (!request->hasParam("ssid")) {
				doc["result"]    = "Fail";
				doc["error_msg"] = "'ssid' must be difined.";
			} else if (!request->hasParam("pass")) {
				doc["result"]    = "Fail";
				doc["error_msg"] = "'pass' must be difined.";
			} else if (request->getParam("ssid")->value() == "") {
				doc["result"]    = "Fail";
				doc["error_msg"] = "'ssid' is empty.";
			} else {
				doc["result"]    = "Ok";
				config["wifi"]["new_ssid"]  = request->arg("ssid");
				config["wifi"]["new_pass"]  = request->arg("pass");
				saveConfiguration();
			}

		} else if (command == "load") {

			Serial.print("Mode: ");
			Serial.println(request->arg("mode"));

			LoadMode mode = modeFromString(request->arg("mode"));

			doc["result"] = "Ok";

			if (mode == LoadMode::status) {
				doc["status"]["mac"]  = WiFi.macAddress();
				doc["status"]["ip"]   = WiFi.localIP().toString();
			}

			if (mode == LoadMode::stepper) {
				doc["stepper"]["pos"]  = Stepper.position();
				doc["stepper"]["done"] = Stepper.isReady();
			}

			if (mode == LoadMode::config) {
				doc["config"] = config;
			}

		} else if (command == "stepper") {

			// Check if parameters exists
			if (!request->hasParam("mode") || !request->hasParam("value")) {
				doc["result"] = "Fail";
				doc["msg"] = "The stepper command must have 'mode' & 'value' parameters.";
			} else {
				String mode = request->arg("mode");
				long value  = request->arg("value").toInt();

				if (value == 0) {
					doc["result"] = "Fail";
					doc["msg"] = "The 'value' parameter must not be zero.";
				} else {
					if (mode == "movecw") {
						Stepper.moveCW(value);
						Serial.println("movecw");
						doc["result"] = "Ok";
					} else if (mode == "moveccw") {
						Stepper.moveCCW(value);
						Serial.println("moveccw");
						doc["result"] = "Ok";
					} else {
						doc["result"] = "Fail";
						doc["msg"] = "The 'mode' parameter has an invalid value.";
					}
				}

			}
		}

		String json;
		serializeJson(doc, json);
		request->send(200, "application/json", json);
	});

	server.begin();
}

void loop() {
	ArduinoOTA.handle();
	EasyDDNS.update(10000);
	if (restart) ESP.restart();
}