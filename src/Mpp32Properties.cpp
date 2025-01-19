#include "Mpp32Properties.h"
#include <stdlib.h>
#include <EEPROM.h>
#include <nvs.h> 
#include <nvs_flash.h>

/*
 * Properties ESP32.cpp
  *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 *
 */

#define MaxProps 1024
#define MppMarkerLength 14
#define MppPropertiesLength MaxProps - MppMarkerLength
static const char MppMarker[MppMarkerLength] = "MppProperties";

const char *P_PASSWORD = "Password";
const char *P_GATEWAY_PW = "GatewayPassword";

static bool writeProperties(String target) {
// Serial.print("writeProperty 1: "+target);
	if (target.length() < MppPropertiesLength) {
		for (unsigned i = 0; i < MppPropertiesLength && i < target.length() + 1;
				i++)
//        EEPROM.writeChar(i + MppMarkerLength, target.charAt(i));
			EEPROM.put(i + MppMarkerLength, target.charAt(i));
// Serial.printf("Properties heap=%d \n", ESP.getFreeHeap());
		EEPROM.commit();
		return true;
	} else {
		Serial.println("Properties do not fit in reserved EEPROM space.");
		return false;
	}
}

MppProperties::MppProperties() {
}

static char propertiesString[MppPropertiesLength];

void MppProperties::begin() {
  
  esp_err_t err = nvs_flash_init();   // Initialize NVS 
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    { ESP_ERROR_CHECK(nvs_flash_erase()); 
    err = nvs_flash_init(); 
    } 
      ESP_ERROR_CHECK(err);
  
	if(!EEPROM.begin(MaxProps)) { Serial.println("Error Initializing 1024 bytes of EEPROM !"); return; }
	for (int i = 0; i < MppMarkerLength; i++) {
		if (MppMarker[i] != EEPROM.read(i)) {
			Serial.print("EEPROM marker mismatch, found '");
			for (int x = 0; x < MppMarkerLength; x++)
				Serial.print(EEPROM.read(x));
			Serial.print("'\n");
			Serial.println("EEPROM initializing...");
//     EEPROM.writeString(0, MppMarker);
			EEPROM.put(0, MppMarker);
 //    EEPROM.commit(); /// for test
    	properties.clear();
			save();
			Serial.println("EEPROM initialized");
			break;
		}
	}
	for (unsigned i = 0; i < MppPropertiesLength; i++) {
		propertiesString[i] = EEPROM.read(i + MppMarkerLength);
		if (propertiesString[i] == 0)
			break;	
			}
// Serial.printf("Properties string:%s\n",propertiesString);
	const char* error = properties.loadFrom(propertiesString);
	if (error) {
		Serial.printf("Properties.load failed: %s\n", error);
		properties.clear();
		save();
	}
}

MppProperties::~MppProperties() {
}

void MppProperties::remove(const char *key) {
	properties.remove(key);
}

void MppProperties::put(const char *k_ptr, const char *v_ptr) {
	properties.put(k_ptr, v_ptr);
}

const char* MppProperties::get(const char *key) {
	return properties.get(key);
}

bool MppProperties::contains(const char *key) {
	return properties.contains(key);
}

bool MppProperties::has(const char *key) {
	return properties.has(key);
}

bool MppProperties::is(const char *key) {
	return properties.is(key);
}

int MppProperties::getInt(const char *key) {
	return properties.getInt(key);
}

unsigned MppProperties::getUnsigned(const char *key) {
	return properties.getUnsigned(key);
}

float MppProperties::getFloat(const char *key) {
	return properties.getFloat(key);
}

bool MppProperties::update(const String &newProperties) {
	const char *p_ptr = has(P_PASSWORD) ? get(P_PASSWORD) : NULL;
	const char *g_ptr = has(P_GATEWAY_PW) ? get(P_GATEWAY_PW) : NULL;
	String password = p_ptr; // cache it
	String gatewayPW = g_ptr;
	const char* error = properties.loadFrom(newProperties);
	if (error) {
		Serial.printf("Properties.load failed: %s\n", error);
		return false;
	} else {
		if (p_ptr != NULL)
			put(P_PASSWORD, password.c_str());
		const char *gwpw = get(P_GATEWAY_PW);
		if (g_ptr != NULL
				&& (gwpw == NULL || strlen(gwpw) == 0
						|| strcmp("********", gwpw) == 0))
			put(P_GATEWAY_PW, gatewayPW.c_str());
		save();
		return true;
	}
}

void MppProperties::clear() {
	properties.clear();
	save();
}

bool MppProperties::save() {
	String target = properties.toString();
	if (writeProperties(target)) {
		Serial.printf("Saved properties (%d bytes).\n", target.length());
//   Serial.println("Properies string"+target);
		return true;
	} else
		return false;
}

String MppProperties::toString() {
	MppJson result;
	result.loadFrom(properties.toString());
	if (result.has(P_PASSWORD) && strlen(result.get(P_PASSWORD)) > 0)
		result.put(P_PASSWORD, "********");
	else
		result.remove(P_PASSWORD);
	if (result.has(P_GATEWAY_PW))
		result.put(P_GATEWAY_PW,
				strlen(result.get(P_GATEWAY_PW)) == 0 ? "" : "********");
	return result.toString();
}

int MppProperties::size() {
	return properties.size();
}
