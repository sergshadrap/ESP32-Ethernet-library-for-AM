#include <Arduino.h>
#include <NetworkUdp.h>
#include "Mpp32Device.h"
#include "config.h"

// #define UDP_TX_PACKET_MAX_SIZE 2048


/*
 * MppDevice.cpp FOR ESP32!!
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 */

// optional parameters
const char *P_LED_INVERT = "LedInvert"; // boolean
const char *P_SENSOR_INVERT = "SensorInvert"; // boolean
const char *P_RELAY_INVERT = "RelayInvert"; // boolean
const char *P_PULLUP = "PullUp"; // boolean
const char *P_LED_PIN = "LedPin";
const char *P_SENSOR_PIN = "SensorPin"; // unsigned sensor pin
const char *P_RELAY_PIN = "RelayPin"; // unsigned relay pin
const char *P_INITIAL = "Initial"; // boolean startup state for relays
const char *P_USE_LAST = "UseLast"; // persist last state and use on startup
const char *P_IP_CHECK = "IpCheck"; // frequency of "network available" checks in hours
const char *P_IP_ADDRESS = "IpAddress"; // target of network available connect request
const char *P_IP_PORT = "IpPort"; // port of network available connect request
const char *P_MOMENTARY = "Momentary"; // milliseconds, if non-zero outputs will pulse (change state and return)
const char *P_LONG_PRESS = "LongPress"; // boolean, enable long press detection
const char *P_FOLLOW = "Follow"; // boolean, if specified followers will follow if true, toggle if false
const char *P_FOLLOWERS = "Followers"; // follower pins
// power
const char *P_POWER_PIN = "PowerPin";
const char *P_VOLT_AMP_PIN = "VoltAmpPin";
const char *P_SELECT_PIN = "SelectPin";
// notifiers
const char *P_SERVER_IP = "ServerIp"; // target of event messages (usually the AM server, enable the REST port!)
const char *P_IP_MESSAGE = "IpMessage"; // message to send
static String UID;

// as indexes to the attribute names
const char *ATTRIBUTES[] = { "state", "error", "lpress", "value", "firmware", "gated",
		"temperature", "hue", "saturation", "message", "beacon", "udn", "name", "group","code" };

// as indexes to the type names
const char *types[] = { "MppSensor", "MppSwitch", "MppMomentary", "MppAnalog",
		"MppLevel", "MppTracker", "MppPower", "MppSleeper", "MppAlert", "MppReporter",
		"MppGateway", "MppColor", "MppContact", "MppSetup" };

bool isBatteryDevice(String udn) {
	if (udn.indexOf('_') > 0) {
		String type = udn.substring(0,udn.indexOf('_'));
		return type == types[MppReporter]
			|| type == types[MppAlert]
			|| type == types[MppContact]
			|| type == types[MppSleeper];
	} else
		return false;
}

static class Subscriptions {
public:
	Subscriptions();
	~Subscriptions();
	void addSubscriber(const String &ip, int port = MPP_PORT);
	void notifySubscribers(MppDevice *device);
private:
	struct Subscription {
		char ip[18];
		int port;
		unsigned long expires;
	};
	int count = 0;
	Subscription *subscriptions;
	NetworkUDP deviceUdp;
} subscriptions;

Subscriptions::Subscriptions() {
	subscriptions = (struct Subscription*) calloc(0,
			sizeof(struct Subscription));
}

Subscriptions::~Subscriptions() {
	free(subscriptions);
}

void Subscriptions::addSubscriber(const String &ip, int port) {
	Subscription *subscription = NULL;
	for (int i = 0; i < count; i++) {
		if (strcmp(subscriptions[i].ip, ip.c_str()) == 0 && subscriptions[i].port == port) {
			subscription = &subscriptions[i];
			break;
		}
	}
	if (subscription == NULL) {
		++count;
		subscriptions = (Subscription*) realloc(subscriptions,
				count * sizeof(struct Subscription));
		subscription = &subscriptions[count - 1];
		strcpy(subscription->ip, ip.c_str());
		subscription->port = port == 0 ? MPP_PORT : port;
		Serial.printf("added subscriber %s:%d\n", subscription->ip,port);
	}
	subscription->expires = millis() + SUBSCRIPTION_TIME; // 10m
	Serial.printf("%s subscribed until %lu\n", subscription->ip,
			subscription->expires);
}

void Subscriptions::notifySubscribers(MppDevice *device) {
	if (eth_connected) {
		unsigned long now = millis();
		String message = device->getJson();
		Serial.printf("Notifying with %s...\n", message.c_str());
		for (int i = 0; i < count; i++) {
			if (now < subscriptions[i].expires) {
				deviceUdp.beginPacket(subscriptions[i].ip, subscriptions[i].port);
				int result = deviceUdp.write((const uint8_t *)message.c_str(),message.length());
				deviceUdp.endPacket();
				yield(); // let the UDP notifications go (avoids loss during transmission)
				Serial.printf("Sent notification to %s:%d (%d bytes sent)\n",
						subscriptions[i].ip, subscriptions[i].port, result);
			}
		}
		Serial.println("Notifications sent.");
	}
}

const String& getUID() {
  if(ETH.macAddress()=="00:00:00:00:00:00") {
    
    ETH.begin(ETH_PHY_TYPE,ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
    Serial.printf("Reinitialization of Ethernet, MAC:%s\n",ETH.macAddress().c_str());
    Serial.println("Current IP:"+ETH.localIP());
    if(ETH.localIP()=="0.0.0.0" || ETH.localIP()=="") eth_connected= false; 
      else  eth_connected= true; 
  }
    
  if (UID.length() == 0) {
    UID = ETH.macAddress();
    while (UID.indexOf(':') > 0)
      UID.replace(":", "");
    UID.toLowerCase(); // compatible with V2
  }
 // Serial.printf("UID from DEvice:%s",UID.c_str());
  return UID;
}

String getDefaultUDN(Type t) {
	return String(t <= MppSetup ? types[t] : "MppSetup") + "_" + getUID();
}

MppDevice::MppDevice() {
}

MppDevice::~MppDevice() {
}

const char* MppDevice::getUdn() {
	return attributes.get("udn");
}

const char* MppDevice::getName() {
	return attributes.get("name");
}

void MppDevice::begin(String udn, String name) {

  Serial.printf("Device UDN:%s begin MAC: %s,  IP :%s  \n",udn.c_str(),ETH.macAddress().c_str(), ETH.localIP().toString().c_str());
	update(UDN, udn.c_str());
	update("mac", ETH.macAddress());
	update(NAME, name.c_str());
	update("group", getUID().c_str());
}

void MppDevice::setLocation() {
//	if (_IP.length()>10)
// TODO	if (ETH.localIP() && ETH.localIP().isSet())
		set("location",
				String("http://" + ETH.localIP().toString() + ":" + MPP_PORT).c_str());
}

// no notify
bool MppDevice::clear(const char *key) {
	if (attributes.contains(key)) {
		attributes.remove(key);
		return true;
	} else
		return false;
}

bool MppDevice::clear(Attributes attribute) {
	return clear(ATTRIBUTES[attribute]);
}


bool MppDevice::set(const char *key, const char *value) {
	bool result = false;
	const char *oldValue = attributes.get(key);
//	String temp = oldValue == NULL ? "null" : oldValue; // TEMP
	if (value == NULL || strlen(value) == 0) {
		if (oldValue != NULL) {
			attributes.remove(key);
			result = true;
		}
	} else if (oldValue == NULL || strcmp(oldValue, value) != 0) {
		attributes.put(key,value);
		result = true;
	}
//	Serial.printf("key=%s val=%s old=%s result=%d\n",key,(value == NULL ? "null" : value),temp.c_str(),result); // TODO
	return result;
}

// no notify
bool MppDevice::update(Attributes attribute, const char* value) {
	return set(ATTRIBUTES[attribute], value);
}
bool MppDevice::update(const char *key, const char* value) {
	return set(key, value);
}

bool MppDevice::put(const char *key, const char* value) {
	bool result = set(key, value);
	if (result)
		notifySubscribers();
	return result;
}
bool MppDevice::put(Attributes attribute, const char* value) {
	return put(ATTRIBUTES[attribute], value);
}

// ArduinoJson needs to be refreshed as it leaks memory
// take the opportunity to refresh it...
const String MppDevice::getJson() {
	setLocation();
	return attributes.toString();
}

void MppDevice::addSubscriber(String ip, int port) {
	Serial.printf("addSubscriber %s:%d\n", ip.c_str(),port);
	subscriptions.addSubscriber(ip, port);
}

void MppDevice::notifySubscribers() {
	subscriptions.notifySubscribers(this);
}

String MppDevice::get(Attributes attribute) {
	return String(attributes.get(ATTRIBUTES[attribute]));
}

bool MppDevice::has(Attributes attribute) {
	return attributes.has(ATTRIBUTES[attribute]);
}

void MppDevice::setActionHandler(
bool (*handleAction)(String action, MppParameters parameters)) {
	actionHandler = handleAction;
}

bool MppDevice::handleAction(String action, MppParameters parms) {
	return actionHandler == nullptr ? false : actionHandler(action, parms);
}

void MppDevice::handleDevice(unsigned long now) {
	(void) now; // suppress warning
}

void MppDevice::begin() {

}
