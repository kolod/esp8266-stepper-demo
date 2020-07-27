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
#include <uTimerLib.h>
#include <EasyDDNS.h>
#include <CheapStepper.h>

bool restart = false;

// SKETCH BEGIN
AsyncWebServer server(80);
CheapStepper stepper(14,12,13,15);
StaticJsonDocument<500> config;

void stepper_run() {
	stepper.run();
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

void setup(){

	// Setup debug port
	Serial.begin(74880);
	Serial.setDebugOutput(true);

	// Mount FS
	LittleFS.begin();

	// Load configuration from file
	loadConfiguration();
	serializeJsonPretty(config, Serial);

	// Stepper
	stepper.setRpm(config["stepper"]["rpm"].as<int>());
	stepper.setSpr(config["stepper"]["spr"].as<int>());
	TimerLib.setInterval_us(stepper_run, stepper.interval());

	// Setup wifi connection
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP(config["hostname"].as<char*>());
	WiFi.begin(config["wifi"]["ssid"].as<char*>(), config["wifi"]["pass"].as<char*>());
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.printf("STA: Failed!\n");
		WiFi.disconnect(false);
		delay(1000);
		WiFi.begin(config["wifi"]["ssid"].as<char*>(), config["wifi"]["pass"].as<char*>());
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

	ArduinoOTA.setHostname(config["hostname"].as<char*>());
	ArduinoOTA.begin();

	// DDNS
	EasyDDNS.service(config["ddns"]["provider"].as<String>());
	EasyDDNS.client(
		config["ddns"]["domain"].as<String>(),
		config["ddns"]["user"].as<String>(),
		config["ddns"]["pass"].as<String>()
	);

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

		StaticJsonDocument<500> doc;

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

			int n = WiFi.scanComplete();

			if (n == -2) {
				WiFi.scanNetworks(true);
				doc["result"] = "Ok";

			} else if (n > 0) {
				JsonArray arr = doc.createNestedArray("wifi");

				for (int i = 0; i < n; ++i) {
					JsonObject obj = arr.createNestedObject();
					obj["ssid"]    = WiFi.SSID(i);
					obj["bssid"]   = WiFi.BSSIDstr(i);
					obj["rssi"]    = WiFi.RSSI(i);
					obj["channel"] = WiFi.channel(i);
					obj["secure"]  = WiFi.encryptionType(i);
					obj["hidden"]  = WiFi.isHidden(i);
				}

				WiFi.scanDelete();
				if (WiFi.scanComplete() == -2) {
					WiFi.scanNetworks(true);
				}
				
				doc["result"] = "Ok";
			}

		} else if (command == "save") {


/*

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
			config.wifi.ssid = request->getParam("ssid")->value();
			config.wifi.pass = request->getParam("pass")->value();
			Serial.println("New SSID: " + config.wifi.ssid);
			Serial.println("New PASS: " + config.wifi.pass);
			saveConfiguration();

		}
			*/
			
			;
		} else if (command == "load") {
			;
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
						stepper.moveCW(value);
						Serial.println("movecw");
						doc["result"] = "Ok";
					} else if (mode == "moveccw") {
						stepper.moveCCW(value);
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