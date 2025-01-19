#include <Arduino.h>
#include <Network.h>
#include <WebServer.h>
#include <NetworkUdp.h>
#include "Mpp32Properties.h"
#include "Mpp32Device.h"


/*
 * MppServer.h
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *

 REST requests sent to devices need to include the udn as the resource identifier.
 If the udn is not known to the device it will respond with "resource not found".

 *******************************************************************************
 **** These REST requests should be implemented in your custom sketch
 *******************************************************************************

 PUT http://ip:8898/state/udn?state=[true|false] - set the device relay state
 PUT http://ip:8898/state/udn?state=[true|false]&momentary={nnn} - set the device relay state for nnn milliseconds then set to the "not" state
 PUT http://ip:8898/state/udn?toggle=true - toggle the device relay state
 PUT http://ip:8898/state/udn?toggle=true&momentary={nnn} - toggle the device state for nnn milliseconds then toggle back
 PUT http://ip:8898/state/udn?level={nnn} - set a dimmer/level/stepper device value

 *******************************************************************************
 **** These REST and management requests are handled by the MppServer:
 *******************************************************************************

 GET http://ip:8898/restart{/[run|fast|load]} - reboots the device
 GET http://ip:8898/version - returns the base and device version
 GET http://ip:8898/survey - returns a json array of the available wifi signals
 GET http://ip:8898/ - returns a body of JSON array of device state (for multi-devices)
 PUT http://ip:8898/subscribe - body is the host address (IP).  Notifications are sent to this IP on port 8898 as UDP with a JSON body with the device state.
 Subscriptions are valid for 10m and can be renewed any time.
 PUT http://ip:8898/name/udn - set the friendly name of the device with a JSON body:  { "name":"new_device_name" }
 GET http://ip:8898/state/udn - where resource is the device UDN of a device. Returns the current device state as a JSON body.

 Discovery
 A UDP message to 239.255.255.250:8898 containing the string "discovery" will cause the device to respond to the sender with the device discovery information.

 Management calls:
 GET http://ip:8898/defaults - returns a JSON body containing the current configuration settings
 PUT http://ip:8898/defaults - change configuration with a JSON body containing configuration updates.  Returns the new configuration as JSON.

 *******************************************************************************
 **** JSON state and discovery response.  Implementation need only update state and value.
 *******************************************************************************

 udn - device udn
 location - device IP
 name - friendly name
 state - on, off, standby, unknown (default)
 value - analog value (analog devices only)


 */

#ifndef MPPSERVER_H_
#define MPPSERVER_H_


// extern bool eth_connected;
// MAC based device id, use with the UDN
extern const String& getUID();
extern String methodToString(HTTPMethod method);

extern const char* DeviceVersion; // define in version file

// optional parameters
extern const char* P_BUTTON_PIN;

// managed by the MppServer (no need to pass these in the MppServer constructor)

extern const char* P_NO_MULTICAST;
// extern const char* P_USE_STATIC_IP;
extern const char* P_IP;
extern const char* P_GW;
extern const char* P_NM;
// managed by the MppServer, optional (include in constructor)

extern const char* P_Ethernet_RESTART;

extern const char* P_GATEWAY_PW;
extern const char* P_NICKNAME;


// Button reset length in ms
#define ButtonReset 10000



//  static bool eth_connected;

class MppServer {
public:
	// identify any device unique properties here
	MppServer(const char* deviceVersion, const char* supportedProperties[], const char* extraHelp = NULL, unsigned baud = 115200); // end with NULL
	MppServer(const char* deviceVersion, const char* supportedProperties[],
			size_t supportedSize, const char* extraHelp = NULL, unsigned baud = 115200);
	virtual ~MppServer();

	// add each device with a unique type+udn
	void manageDevice(MppDevice* device, String udn);

	// access the device configuration properties
	const char* getProperty(const char* property); // null if not set
	int getIntProperty(const char* property); // defaults to 0 if not set
	unsigned getUnsignedProperty(const char* property); // defaults to 0 if not set
	float getFloatProperty(const char* property); // defaults to 0 if not set
	bool isProperty(const char* property); // false if the value is not set or not true
	bool hasProperty(const char* property); // false if the value is not set
	void setPropertyDefault(const char* property, const char* value); // set a default if not already set
	void putProperty(const char* property, const char* value); // set/override a property value
	void removeProperty(const char* property); // from the property set
	bool usesProperty(const char* property); // property applicable to this device
	void noteProperty(const char* property, const char* value); // visible but not persisted

	// Attach wifi handler to ButtonPin (if defined)
	// 	 MppServer will report when the button is clicked (released).
	//   If the button is held for longer than ButtonReset the device goes into SoftAP mode
	// use nullptr if no need to notify your sketch
	void manageButton(void (*buttonClick)(void));


	virtual void begin();

	virtual void handleClients(); // call from sketch in each loop
	// handles Serial input commands, use ? or help in serial port
	//   - restart, config wifi, etc, checked each loop
	void handleCommand(); // call from sketch in each loop to handle input

	void sendUdp(const char* ip, String message);

	void sendHttp(String url, String type, String body,	bool &successFlag, unsigned retry = 3, unsigned timeout = 2000);
	void sendHttpEvent(const char *targetIp, const char *event,	bool &successFlag, unsigned retry = 3, unsigned timeout = 2000);

	// send a UDP event to AM/MppDevices (port 8898)
	void sendUdpEvent(const char* ip, const char* event);

	// announce device
	void broadcastDiscovery();

	// send general message (e.g. status or debug)
	void broadcastMessage(String message);

	// for battery devices, called when index (root) page accessed
	// or command issued
//	bool isNoSleep() { return noSleep; }
//	void stayAwake() { noSleep = true; }

	String getName();
//	bool onGotIP();
  NetworkUDP Udp;
  void onEvent(arduino_event_id_t event);
protected:

	MppDevice* getDevice(String udnString);
	virtual String getDiscovery();
	void addDevice(MppDevice* device);
	MppDevice** getDevices() { return devices; }
	unsigned getDeviceCount() { return deviceCount; }
	virtual bool mppHandleAction(HTTPMethod method, String action, String resource, MppParameters parms);
	virtual bool processCommand(String input);

	void sendDiscoveryResponse(IPAddress remoteIp, int remotePort);
	virtual int handleIncomingUdp(NetworkUDP &serverUdp, int packetSize);

	void mppHttpRespond(int code, const String& type = "", const String& response = "") { mppServer.send(code,type,response); }


private:

	WebServer webServer;
	void webHandleRoot();
	void webHandleProps();
	void webHandleSetProperty();
	void webHandleResetProperties();
	void webHandleVersion();
	void webHandleRestart();
	void webHandleDownloadProps();
	void webHandleUploadProps();
	void webHandleUploadedProps();
	void webHandlePropsError();
	void webHandleUploadUpdate();
	void webHandleUploadedUpdate();
	void webHandleUpdateError();
	void webSendBackForm(String title, int code = 200);

	WebServer mppServer;
	void mppHandleVersion();
	void mppHandleProps();
	void mppHandleDiscovery();
	void mppHandleSubscribe();
	void mppHandleState(String udnString);
	void mppHandleName(String udnString);
	void mppHandleRestart();
	void mppHandleSurvey();
	void mppHandleSetup(MppParameters parms);
	// use [ip]:8898/command?run=...
	void mppHandleCommand();
	void mppHandleNotFound();

	NetworkServer console; // TCP command console
	NetworkClient client; // for the console
	boolean consoleActive = false;

NetworkEvents networkEvents;

static void onEventStatic(arduino_event_id_t event); 

bool onGotIP(); // Handler for GOT_IP event NetworkEvents networkEvents;


	void start();

	void sendProperties(WebServer& server);

	void setup(const char* deviceVersion, const char* supportedProperties[],
			unsigned count, unsigned baud);

	unsigned deviceCount = 0;
	MppDevice** devices;
	MppProperties properties;
	bool authenticated = false;
	String updateError;
	String propsError;
	String propsUpdate;
	const char* extraHelp;

};
extern MppServer mppserver;
#endif /* MPPSERVER_H_ */
