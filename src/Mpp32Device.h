#include <Arduino.h>
#include "Mpp32Parameters.h"
#include "Mpp32Json.h"

/*
 * MppDevice.h FOR ESP32!!!
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 */

#ifndef MPPDEVICE_H_
#define MPPDEVICE_H_

#define MPP_PORT 8898

#define MAX_ATTRIBUTES 30
#define ATTRIBUTE_LENGTH 30

// millis, 10m
#define SUBSCRIPTION_TIME 1000 * 10 * 60

// optional parameters
extern const char *P_LED_INVERT; // boolean
extern const char *P_SENSOR_INVERT; // boolean
extern const char *P_RELAY_INVERT; // boolean
extern const char *P_PULLUP; // boolean
extern const char *P_LED_PIN;
extern const char *P_SENSOR_PIN; // unsigned sensor pin
extern const char *P_RELAY_PIN; // unsigned relay pin
extern const char *P_INITIAL; // boolean startup state for relays
extern const char *P_USE_LAST; // persist last state for startup
extern const char *P_IP_CHECK; // frequency of "network available" checks in hours
extern const char *P_IP_ADDRESS; // target of network available connect request
extern const char *P_IP_PORT; // port of network available connect request
extern const char *P_MOMENTARY; // milliseconds, if non-zero outputs will pulse (change state and return)
extern const char *P_LONG_PRESS; // boolean, enable long press detection
extern const char *P_FOLLOW; // boolean, if specified followers will follow if true, toggle if false
extern const char *P_FOLLOWERS; // follower pins
// power
extern const char *P_POWER_PIN;
extern const char *P_VOLT_AMP_PIN;
extern const char *P_SELECT_PIN;
// notifiers
extern const char *P_SERVER_IP; // target of event messages (usually the AM server, enable the REST port!)
extern const char *P_IP_MESSAGE; // message to send

// extern String _MAC;
// extern String _IP;

// common device attributes
enum Attributes {
	STATE,
	ERROR,
	LPRESS,
	VALUE,
	FIRMWARE,
	GATED,
	TEMPERATURE,
	HUE,
	SATURATION,
	MESSAGE,
	BEACON,
	UDN,
	NAME,
	GROUP,
	CODE
};

// known/managed MppDevice types
enum Type {
	MppSensor,
	MppSwitch,
	MppMomentary,
	MppAnalog,
	MppLevel,
	MppTracker,
	MppPower,
	MppSleeper,
	MppAlert,
	MppReporter,
	MppGateway,
	MppColor,
	MppContact,
	MppSetup
};

extern String getDefaultUDN(Type t);
extern bool isBatteryDevice(String udn);
  const String& getUID();
 extern bool eth_connected;

class MppDevice {
public:
	MppDevice();
	virtual ~MppDevice();

	const char* getUdn();
	const char* getName();

	virtual void begin(String udn, String name);

	bool put(const char *key, const char *value); // update and notify
	bool put(Attributes attribute, const char *value); // update and notify
	bool has(Attributes attribute);
	bool update(const char *key, const char *value); // no notify
	bool update(Attributes attribute, const char *value); // no notify
	bool put(const char *key, String value) { // update and notify
		return put(key, value.c_str());
	}
	bool put(Attributes attribute, String value) { // update and notify
		return put(attribute, value.c_str());
	}
	bool update(const char *key, String value) {  // no notify
		return update(key, value.c_str());
	}
	bool update(Attributes attribute, String value) { // no notify
		return update(attribute, value.c_str());
	}
	bool clear(Attributes attribute); // no notify
	bool clear(const char *key); // no notify
	String get(Attributes attribute);
	const String getJson(); // get and refresh buffer
	static void addSubscriber(String ip, int port = MPP_PORT);
	void notifySubscribers(); // use after update, put notifies automatically

	// the handler should return true if successful
	// (allows use in sketch)
	void setActionHandler(
	bool (*handleAction)(String action, MppParameters parameters));

	// override to change the default action (which is to call the actionHandler)
	// when used in a class
	virtual bool handleAction(String action, MppParameters parms);

	virtual void handleDevice(unsigned long now); // millis since startup

	virtual void begin();

protected:
	bool (*actionHandler)(String action, MppParameters parameters) = nullptr;

private:
	// returns true if changed
	bool set(const char *key, const char *value);
	void setLocation();
	MppJson attributes;
};

#endif /* MPPDEVICE_H_ */
