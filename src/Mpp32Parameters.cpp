/*
 * MppParameters.cpp FOR 32 !!
 * 
 *  
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 *
 */

#include "Mpp32Parameters.h"

String MppParameters::getParameter(const char* name) {
	 return webRequest->arg(name);
}

bool MppParameters::hasParameter(const char* name) {
	return webRequest->arg(name).length() > 0;
}

String MppParameters::getParameter(int i) {
	return webRequest->arg(i);
}

String MppParameters::getParameterName(int i) {
	return webRequest->argName(i);
}

int MppParameters::getParameterCount() {
	return webRequest->args();
}

MppParameters::MppParameters(WebServer* webServer) {
	webRequest = webServer;
}

float MppParameters::getFloatParameter(const char* name) {
	String value = getParameter(name);
	return value.length() == 0 ? 0 : atof(value.c_str());
}

unsigned MppParameters::getUnsignedParameter(const char* name) {
	String value = getParameter(name);
	return value.length() == 0 ? 0 : atoi(value.c_str());
}

int MppParameters::getIntParameter(const char* name) {
	String value = getParameter(name);
	return value.length() == 0 ? 0 : atoi(value.c_str());
}

bool MppParameters::getBoolParameter(const char* name) {
	String value = getParameter(name);
	value.toLowerCase();
	return value == "true";
}

String MppParameters::getAsQuery() {
	String result = "";
	for (int i = 0; i < getParameterCount(); i++) {
		String parm = getParameter(i);
		parm.replace(" ","%20"); // only replacing spaces
		if (parm.length() > 0)
			result += (result.length() == 0 ? "?" : "&") + getParameterName(i)
					+ "=" + parm;
	}
	return result;
}
