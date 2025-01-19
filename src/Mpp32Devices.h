#include <Arduino.h>

#include "Mpp32Device.h"

/*
 * MppDevices.h FOR ESP32!!
 *
 *
 * *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 *      These classes are used in the MppLibrary implementation classes to capture some common behavior in basic sensor and relay devices
 *      allowing them to be easily combined (e.g. so that a sensor can be used as an input switch to trigger a relay our output LED).
 *
 *      When implementing custom devices it is often better to derive it from the MppDevice class for integration with the MppServer/AM
 *      and use the code here in MppDevices as reference examples.
 */

#ifndef MPPDEVICES_H_
#define MPPDEVICES_H_

// a generic sensor handler
class MppSensor : public MppDevice {
public:
	MppSensor(unsigned pin, bool invert, bool pullup);
	void enableLongPress(bool activeLevel);
	// use to add additional handling on state change
	void setSensorHandler(void (*handleState)(bool state, unsigned pin));

	void setFollower(unsigned pin);

	// called by MppServer, do not invoke separately
	void handleDevice(unsigned long now) override;
	void begin() override;

private:
	unsigned pin, follow;
	volatile byte interruptCounter = 0;
	bool invert = false;
	void (*sensorHandler)(bool state, unsigned pin) = nullptr;
	void reportSensorState();
	void handleInterrupt();
	bool longPress = false; // true to support longpress
	bool pressLevel = false; // active sense level
	bool lastPress = false;
	unsigned long pressStart = 0;
};

// a generic relay handler
class MppRelay : public MppDevice {
public:
	// pulse in seconds, use 0 for normal operation
	MppRelay(unsigned pin, unsigned pulse, bool baseState = false);
	// use to add additional handling on state change
	void setRelayHandler(void (*handleState)(bool state, unsigned pin));
	// momentary in milliseconds
	void setRelay(bool state, unsigned momentary_ms);
	// momentary in milliseconds
	void toggleRelay(unsigned momentary_ms);
	// use 0 to stop flashing
	void flashRelay(unsigned period_ms);

	// set a pin to follow the relay state (e.g. an LED)
	void setFollower(unsigned pin, bool invert);

	// true to invert the sense of the output relays
	void setRelayInvert(bool invert);

	bool getRelay();

  // ipCheckTime in minutes, port==0 use 80, returns true if check fails
  bool doIpCheck(unsigned ipCheckTime, String hostAddress, unsigned port);

	// called by MppServer, do not invoke separately
	void handleDevice(unsigned long now) override;
	bool handleAction(String action, MppParameters parms) override;
	void begin() override;

private:
	void cancelFlashing();
	unsigned pin, follow;
	unsigned pulse = 0;
	bool baseState, ledInvert = false, relayInvert = false;
	unsigned long expires = 0;
	bool restoreState = false;
	unsigned flashPeriod = 0;
	unsigned long flashNext = 0;
	void reportRelayState();
	void (*relayHandler)(bool state, unsigned pin) = nullptr;
	void restoreRelay(bool state);
	unsigned long lastIpCheck = millis();
};

// generic PWM handler, on any pin
class MppPWM: public MppDevice {
public:
	MppPWM(unsigned pin);

	bool handleAction(String action, MppParameters parms) override;

	// use to add additional handling on state change
	void setStateHandler(void (*handleState)(bool state)) {
		stateHandler = handleState;
	}

	void setLevel(unsigned level);

	unsigned getLevel() {
		return level;
	}

	void setState(bool state);

	bool getState() {
		return state;
	}

private:
	unsigned pin = 0;
	unsigned level = 0;bool state = false;
	void (*stateHandler)(bool state) = nullptr;

	void notifyLevel();

};

// base tracker
class MppTracker: public MppDevice {
public:
	MppTracker() {
		setState(false); // to match startup parms
	}

	bool handleAction(String action, MppParameters parms) override;

	virtual void setValue(float value);

	float getValue() {
		return value;
	}

	virtual void setState(bool state);

	bool getState() {
		return state;
	}

private:
	float value = 0;
	bool state = false;
	void notifyValue();

};

#endif /* MPPDEVICES_H_ */
