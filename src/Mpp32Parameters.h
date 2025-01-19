#include <Arduino.h>
#include <WebServer.h>
/*
 * MppParameters.h ESP32 
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 */

#ifndef MPPPARAMETERS_H_
#define MPPPARAMETERS_H_

class MppParameters {
public:
	MppParameters(WebServer* webServer);
	String getParameter(const char *name);
	bool hasParameter(const char *name);
	String getParameterName(int i);
	String getParameter(int i);
	int getParameterCount();
	unsigned getUnsignedParameter(const char* name);
	float getFloatParameter(const char* name);
	int getIntParameter(const char* name);
	bool getBoolParameter(const char* name);
	String getAsQuery();
	HTTPUpload& getUpload() { return webRequest->upload(); }
protected:
private:
	WebServer* webRequest;

};

#endif /* MPPPARAMETERS_H_ */
