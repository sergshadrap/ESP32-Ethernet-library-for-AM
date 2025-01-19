#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include "Mpp32Devices.h"
#include <NetworkClient.h>
/*
 * MppDevices.cpp FOR ESP32!!
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 */

/******************************************************************************
 * MppSensor
 *****************************************************************************/

MppSensor::MppSensor(unsigned pin, bool invert, bool pullup) {
	this->pin = pin;
	this->follow = pin;
	this->invert = invert;
	pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
	Serial.printf("Added MppSensor on pin %d\n", pin);
	attachInterrupt(digitalPinToInterrupt(pin),
			std::bind(&MppSensor::handleInterrupt, this), CHANGE);
}

void MppSensor::handleDevice(unsigned long now) {
	// check and handle any interrupt
	if (interruptCounter > 0) {
		interruptCounter = 0; // do first so no interrupts are missed
		Serial.printf("Sensor interrupt pin%d!\n", pin);
		String current = get(STATE);
		// force a report by toggling the state
		// this makes sure every interrupt is reported
		if (current == "off")
			put(STATE, "on");
		else if (current == "on")
			put(STATE, "off");
		// restore state using current sensor
		reportSensorState(); // report the final value (if changed)
	}
	MppDevice::handleDevice(now);
}

void MppSensor::reportSensorState() {
	// read and report the sensor state
	bool sensorState = digitalRead(pin) == HIGH;
	if (invert)
		sensorState = !sensorState;

	if (longPress) {
		String longPressed;
		if (lastPress != sensorState) {
			unsigned long duration = 0;
			unsigned long now = millis();
			if (sensorState != pressLevel) { // end of active state
				duration = now - pressStart;
				Serial.printf("Pressed for %lu ms\n", duration);
				if (duration > 100) // debounce
					longPressed = duration > 1000 ? "true" : "false";
			}
			pressStart = now;
		}
		lastPress = sensorState;
		// long press is removed for none, false for short, true for long
		update(LPRESS, longPressed.length() == 0 ? NULL : longPressed.c_str());
		Serial.printf("longPressed='%s'\n", longPressed.c_str());
	}

	if (follow != pin)
		digitalWrite(follow, sensorState ? HIGH : LOW);

	put(STATE, sensorState ? "on" : "off");

	if (sensorHandler != nullptr)
		sensorHandler(sensorState, pin);
}

void MppSensor::begin() {
	// capture the current state
	reportSensorState();
	MppDevice::begin();
}

void MppSensor::setSensorHandler(
		void (*handleState)(bool state, unsigned pin)) {
	sensorHandler = handleState;
}

ICACHE_RAM_ATTR void MppSensor::handleInterrupt() {
	++interruptCounter;
}

void MppSensor::enableLongPress(bool activeLevel) {
	longPress = true; // enable
	pressLevel = activeLevel;
	Serial.printf("LongPress enabled on sensor %s\n",
			(activeLevel ? "HIGH" : "LOW"));
}

void MppSensor::setFollower(unsigned followPin) {
	follow = followPin;
	if (follow != this->pin) {
		Serial.printf("Sensor follower on pin %d\n", follow);
		pinMode(follow, OUTPUT);
	}
}

/******************************************************************************
 * MppRelay
 *****************************************************************************/

MppRelay::MppRelay(unsigned pin, unsigned pulse, bool baseState) {
	this->pin = pin;
	this->follow = pin; // set unequal to enable follow
	this->pulse = pulse;
	this->baseState = baseState;
	Serial.printf("Added MppRelay on pin %d\n", pin);
	pinMode(pin, OUTPUT);
}

void MppRelay::handleDevice(unsigned long now) {
	// check whether momentary timer has expired
	if (expires != 0 && expires < now)
		restoreRelay(restoreState);
	if (flashPeriod > 0 && flashNext < now) {
		toggleRelay(flashPeriod);
		flashNext = flashPeriod * 2 + now;
	}
}

void MppRelay::begin() {
	reportRelayState();
}

void MppRelay::reportRelayState() {
	// read and report the sensor state
	bool relayState = digitalRead(pin) == HIGH;
	if (relayInvert)
		relayState = !relayState;
	put(STATE, relayState ? "on" : "off");
	// follow the relay state if configured
	if (follow != pin) {
		bool followState = ledInvert ? !relayState : relayState;
		digitalWrite(follow, followState ? HIGH : LOW);
	}
	if (relayHandler != nullptr)
		relayHandler(relayState, pin);
}

// return relay state (true == on)
bool MppRelay::getRelay() {
	return digitalRead(pin) == HIGH;
}

void MppRelay::setRelayHandler(void (*handleState)(bool state, unsigned pin)) {
	relayHandler = handleState;
}

bool MppRelay::doIpCheck(unsigned ipCheckTime, String hostAddress, unsigned port) {
  bool result = false;
  ipCheckTime *= 60000;
  if (hostAddress.length() && ipCheckTime > 0 && millis() > ipCheckTime + lastIpCheck) {
      if (port == 0)
        port = 80;
      bool connected = false;
      for (int i = 0; i < 3 && !connected; i++) {
        Serial.printf("CheckIp connecting to %s:%d...\n",
            hostAddress.c_str(), port);
        // Use Network class to create TCP connections
        NetworkClient client;
        connected = client.connect(hostAddress.c_str(), port); 
        if (connected)
          Serial.printf(
              "CheckIp connected successfully to %s:%d.\n",
              hostAddress.c_str(), port);
        else
          Serial.printf(
              "CheckIp failed to connect to %s:%d...\n",
              hostAddress.c_str(), port);
      }
      if (!connected) {
        result = true;
        Serial.println(
            "CheckIp failed, toggling relay...");
        delay(1000); // give time to send message
        setRelay(false, pulse == 0 ? 10000 : pulse);
      }
    lastIpCheck = millis();
  }
  return result;
}

// duration is ms
void MppRelay::setRelay(bool state, unsigned duration) {
	// if not specified used the default duration
	if (duration == 0)
		duration = pulse;
	if (duration > 0) {
		unsigned long now = millis();
		if (pulse > 0) // pulse always returns to initial state
			restoreState = baseState;
		else
			// else restore to previous state
			restoreState = !state;
		expires = now + duration;
	}
	if (duration)
		Serial.printf("setRelay: %s for %ums\n", state ? "ON" : "OFF",
				duration);
	else
		Serial.printf("setRelay: %s\n", state ? "ON" : "OFF");
	if (relayInvert)
		state = !state;
	digitalWrite(pin, state ? HIGH : LOW);
	reportRelayState();
}

void MppRelay::restoreRelay(bool state) {
	Serial.printf("restoreRelay to %s\n", state ? "ON" : "OFF");
	expires = 0;
	if (relayInvert)
		state = !state;
	digitalWrite(pin, state ? HIGH : LOW);
	reportRelayState();
}

void MppRelay::toggleRelay(unsigned duration) {
	bool state = digitalRead(pin) != HIGH;
	if (relayInvert)
		state = !state;
	setRelay(state, duration);
}

void MppRelay::cancelFlashing() {
	flashPeriod = 0;
}

void MppRelay::flashRelay(unsigned period_ms) {
	Serial.printf("flashRelay p=%dms\n", period_ms); // TODO
	if (period_ms == 0)
		// stop flashing
		cancelFlashing();
	else {
		flashPeriod = period_ms;
		flashNext = period_ms * 2 + millis();
		// start flashing
		toggleRelay(period_ms);
	}
}

bool MppRelay::handleAction(String action, MppParameters parms) {
	boolean handled = false;
	if (action == "state") {
		unsigned momentary = parms.getUnsignedParameter("momentary");
		if (parms.hasParameter("state")) {
			cancelFlashing();
			setRelay(parms.getBoolParameter("state"), momentary);
			handled = true;
		} else if (parms.hasParameter("toggle")) {
			cancelFlashing();
			toggleRelay(momentary);
			handled = true;
		} else if (parms.hasParameter("flash")) {
			flashRelay(parms.getUnsignedParameter("flash"));
			handled = true;
		}
	}
	return handled ? true : MppDevice::handleAction(action, parms);
}

void MppRelay::setFollower(unsigned followPin, bool invert) {
	this->follow = followPin;
	this->ledInvert = invert;
	if (follow != this->pin) {
		pinMode(follow, OUTPUT);
		Serial.printf("OUTPUT (follower) on pin %d\n", follow);
	}
}

void MppRelay::setRelayInvert(bool invert) {
	relayInvert = invert;
}

/******************************************************************************
 * MppPWM
 *****************************************************************************/

MppPWM::MppPWM(unsigned pin) {
	this->pin = pin;
	setState(false); // to match startup parms
}

bool MppPWM::handleAction(String action, MppParameters parms) {
	boolean handled = false;
	if (action == "state") {
		if (parms.hasParameter("state")) {
			setState(parms.getBoolParameter("state"));
			handled = true;
		} else if (parms.hasParameter("toggle")) {
			setState(!getState());
			handled = true;
		} else if (parms.hasParameter("level")) {
			setLevel(parms.getUnsignedParameter("level"));
			handled = true;
		}
	}
	return handled ? true : MppDevice::handleAction(action, parms);
}

void MppPWM::setLevel(unsigned level) {
	this->level = level;
	if (state)
		analogWrite(pin, state ? level : 0);
	notifyLevel();
}

void MppPWM::setState(bool state) {
	this->state = state;
	analogWrite(pin, state ? level : 0);
	notifyLevel();
}

void MppPWM::notifyLevel() {
	bool updated = false;
	updated |= update(STATE, getState() ? "on" : "off");
	updated |= update(VALUE, String(level).c_str());
	if (updated) {
		notifySubscribers();
		Serial.printf("Level=%s state=%s\n", get(VALUE).c_str(),
				get(STATE).c_str());
		if (stateHandler != nullptr)
			stateHandler(state);
	}
}

/******************************************************************************
 * MppTracker
 *****************************************************************************/

bool MppTracker::handleAction(String action, MppParameters parms) {
	boolean handled = false;
	if (action == "state") {
		if (parms.hasParameter("state")) {
			setState(parms.getBoolParameter("state"));
			handled = true;
		} else if (parms.hasParameter("toggle")) {
			setState(!getState());
			handled = true;
		} else if (parms.hasParameter("value")) {
			setValue(parms.getFloatParameter("value"));
			handled = true;
		}
	}
	return handled ? true : MppDevice::handleAction(action, parms);
}

void MppTracker::setValue(float value) {
	this->value = value;
	notifyValue();
}

void MppTracker::setState(bool state) {
	this->state = state;
	notifyValue();
}

void MppTracker::notifyValue() {
	bool updated = false;
	updated |= update(STATE, getState() ? "on" : "off");
	updated |= update(VALUE, String(value).c_str());
	if (updated) {
		notifySubscribers();
		Serial.printf("Value=%s state=%s\n", get(VALUE).c_str(),
				get(STATE).c_str());
	}
}
