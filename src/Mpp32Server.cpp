#include "config.h"
#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <NetworkUdp.h>
#include <StreamString.h>

#include "Mpp32Server.h"
#include "Mpp32HttpClient.h"

#define UDP_TX_PACKET_MAX_SIZE 2048


/*
 * Mpp32Server.cpp
  *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 *
 */

const char *VERSION = "MppArduino32Eth 1.2.0";


const char *P_BUTTON_PIN = "ButtonPin";
const char *P_Ethernet_RESTART="Ethernet wait to restart";
const char *P_NO_MULTICAST = "NoMulticast";
// const char *P_USE_STATIC_IP = "StaticIp";
const char *P_IP = "ip";
const char *P_GW = "gw";
const char *P_NM = "nm";
const char *P_SSID = "ssid";  //Keep that field for Ethernet

const char *P_NICKNAME = "Nickname";

const char *USERNAME = "admin";

extern char *mppindex;

// time how long not connected for reboot
unsigned EthConnect;
bool eth_connected = false;


static const char *Managed[] = { P_NICKNAME, //
    P_Ethernet_RESTART, // Times in sec waiting Ethernet connection to restart the chip
		P_NO_MULTICAST, // suppress multicast join
	//	P_USE_STATIC_IP, 
	  P_IP, P_GW, P_NM, // always static IP configuration
		P_PASSWORD // for secure devices
		};


bool isEthernetReady() {
	return eth_connected && ETH.localIP();
}

String methodToString(HTTPMethod method) {
	switch (method) {
	case HTTP_PUT:
		return "PUT";
	case HTTP_POST:
		return "POST";
	case HTTP_GET:
		return "GET";
	case HTTP_DELETE:
		return "DELETE";
	case HTTP_PATCH:
		return "PATCH";
//	case HTTP_HEAD:
//		return "HEAD";
	case HTTP_OPTIONS:
		return "OPTIONS";
	default:
		return "HTTP_ANY";
	}
}

static NetworkUDP ServerUdp;

static volatile unsigned long buttonInterruptStart = 0;
static volatile unsigned long buttonInterruptDone = 0;
static std::function<void(void)> buttonClickHandler = nullptr;
static unsigned long BlinkPeriod = 500;
static unsigned buttonPin = 0;

ICACHE_RAM_ATTR static void buttonHandler() {
	bool state = digitalRead(buttonPin);
	if (state) // active low
		buttonInterruptDone = millis();
	else {
		buttonInterruptDone = 0;
		if (buttonInterruptStart == 0)
			buttonInterruptStart = millis();
	}
}


const IPAddress MPP_ADDRESS = IPAddress (239,255,255,250);
const int MulticastPort = 8898;

static const char *CONTENT_LENGTH = "Content-Length";
static const char *APPL_JSON = "application/json";
static const char *TEXT_HTML = "text/html";
static const char *TEXT_PLAIN = "text/plain";
static String UID;


#define WEB_PORT 80

void MppServer::webHandleRoot() {
	bool authenticated = (!hasProperty(P_PASSWORD)
			|| webServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
	if (!authenticated)
		return webServer.requestAuthentication();
	Serial.printf("HttpResponse: index.htm to %s\n",
			webServer.client().remoteIP().toString().c_str());
	String page = FPSTR(mppindex);
//	MppSerial.printf("Page: %d\n'%s'\n",page.length(),page.c_str()); // TODO
	webServer.sendHeader(CONTENT_LENGTH, String(page.length()));
	webServer.send(200, TEXT_HTML, page);
	//stayAwake(); // since root page accessed
}

void MppServer::webHandleProps() {
	sendProperties(webServer);
}

void MppServer::sendProperties(WebServer &server) {
	String result = properties.toString();
	server.sendHeader(CONTENT_LENGTH, String(result.length()));
	String filename = "attachment; filename=\"" + getUID() + ".props\"";
	server.sendHeader("Content-Disposition", filename);
	server.send(200, APPL_JSON, result);
}

void MppServer::mppHandleProps() {
	if (mppServer.method() == HTTP_GET) {
		sendProperties(mppServer);
	} else if (mppServer.method() == HTTP_PUT) {
		bool authenticated = (!hasProperty(P_PASSWORD)
				|| mppServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
		if (!authenticated)
			return mppServer.requestAuthentication();
		properties.update(mppServer.arg("plain"));
		sendProperties(mppServer);
	} else
		mppServer.send(501);
}

void MppServer::webSendBackForm(String title, int code) {
	String result = F("<html>"
			"<style>table, th, td { border: 1px solid black; }</style>"
			"<body style='width:90%;margin-left:auto;margin-right:auto;'>");
	result += title;
	result += F("<form action='./' method='get'>"
			"<p><input type='submit' value='Back' style='width: 100px;'/></p>"
			"</form></body></html>");
	webServer.sendHeader(CONTENT_LENGTH, String(result.length()));
	webServer.send(code, TEXT_HTML, result);
}

void MppServer::webHandleResetProperties() {
	bool authenticated = (!hasProperty(P_PASSWORD)
			|| webServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
	if (!authenticated)
		return webServer.requestAuthentication();
	Serial.println(F("Reset properties"));
	properties.clear();
	webSendBackForm(F("Properties reset.  Restart to continue."));
}

void MppServer::webHandleRestart() {
	Serial.println(F("Restarting..."));
	webSendBackForm(F("Restarting..."));
	delay(250);
	ESP.restart();
}

void MppServer::webHandleVersion() {
	webServer.send(200, TEXT_PLAIN, String(VERSION) + " / " + DeviceVersion);
}

void MppServer::mppHandleVersion() {
	mppServer.send(200, TEXT_PLAIN, String(VERSION) + " / " + DeviceVersion);
}

void MppServer::mppHandleSurvey() {
	Serial.println("mppHandleSurvey...");
	String result = "[{\"ssid\":\"Ethernet\",\"bssid\":\""+ETH.macAddress()+"\",\"channel\":0,\"rssi\":0,\"auth\":0}]";
	mppServer.sendHeader(CONTENT_LENGTH, String(result.length()));
	mppServer.send(200, APPL_JSON, result);
	Serial.println("mppHandleSurvey complete.");
}

void MppServer::webHandleSetProperty() {
	bool authenticated = (!hasProperty(P_PASSWORD)
			|| webServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
	if (!authenticated)
		return webServer.requestAuthentication();
	String key = webServer.arg("keyname");
	if (!key.length())
		key = webServer.arg("keyselect");
	String value = webServer.arg("value");
	Serial.printf("key='%s' value='%s'\n", key.c_str(), value.c_str());
	if (key.length()) {
		if (!value.length())
			properties.remove(key.c_str());
		else
			properties.put(key.c_str(), value.c_str());
	}
	if (properties.save())
		webSendBackForm(F("Properties updated, restart to apply."));
	else
		webSendBackForm(
				F("Properties updated failed (too long).  Restart to recover."),
				413);
}

int sendBroadcast(String message) {
	ServerUdp.beginMulticast(MPP_ADDRESS, MulticastPort);  // beginMulticast - joint to specific multicast group
  ServerUdp.beginMulticastPacket();  // beginMulticastPacket() - send multicast to multicast group
	int result = ServerUdp.write((const uint8_t *)message.c_str(),message.length());
	ServerUdp.endPacket();
	Serial.printf("Sent broadcast '%s' (%d bytes sent)\n", message.c_str(),result);
	return result;
}

void MppServer::broadcastMessage(String message) {
	sendBroadcast("OUT: " + message);
}

void MppServer::broadcastDiscovery() {
	sendBroadcast(getDiscovery());
}

String MppServer::getName() {
	if (hasProperty(P_NICKNAME))
		return String(getProperty(P_NICKNAME)) + "(" + getUID() + ")";
	else
		return getUID();
}

void MppServer::start() {

	webServer.begin(WEB_PORT);
	mppServer.begin(MPP_PORT);
	console.begin(MPP_PORT - 1);

	Serial.println();
	Serial.printf(" WEB, MPP and Console server started at IP Address: %s\n", ETH.localIP().toString().c_str());
  Serial.printf("Device MAC:%s\n",ETH.macAddress().c_str());
  
	String startupMessage = getName() + " " + String(DeviceVersion) + "/"
			+ VERSION + " on " + "Ethernet" + " rssi=" + "0";
	//		+ " restart=" + ESP.getResetInfo();  - todo rtc_get_reset_reason()

	broadcastMessage(startupMessage);
	if (!isProperty(P_NO_MULTICAST)) {
		// notify active
		broadcastDiscovery();

	}
}

void MppServer::handleClients() {

	unsigned long now = millis();

	if (!isEthernetReady()) {
   Serial.print(".");
		if (getUnsignedProperty(P_Ethernet_RESTART) > 0
				&& millis()
						> EthConnect
								+ getUnsignedProperty(P_Ethernet_RESTART) * 60
										* 1000) {
			Serial.println("\nNo Ip within timeout, restarting...\n");
			delay(1000);
			ESP.restart();
		}
	} else
		EthConnect = millis();

	webServer.handleClient();
	mppServer.handleClient();

	if (console.hasClient()) {
		Serial.println("Starting new console client!");
		if (consoleActive)
			client.stop();
		client = console.accept();
		Serial.printf("Console at %s is ready!\n", ETH.localIP().toString().c_str());
		if (hasProperty(P_PASSWORD)) {
			Serial.println("Enter password:");
			String password;
			while (password.length() == 0)
				password = client.readStringUntil('\n');
			Serial.printf("Password echo: '%s'\n",password.c_str());
			if (password != getProperty(P_PASSWORD))  {
				consoleActive = false;
				client.stop();
			} else {
				consoleActive = true;
				Serial.println("Authentication successful.");
			}
		} else
			consoleActive = true;
	}

	if (consoleActive) {
		if (client.connected()) {
			int length = client.available();
			if (length) {
				String command = client.readStringUntil('\n');
				if (command.length()) {
					Serial.printf("processing command len=%d '%s'...\n",
							command.length(), command.c_str()); // TODO
					processCommand(command);
					Serial.flush();
				}
			}
		} else {
			client.stop();
			consoleActive = false;
			Serial.println("Client stopped");
		}
	}

	int packetSize = ServerUdp.parsePacket();
	if (packetSize)
		handleIncomingUdp(ServerUdp, packetSize);

	for (unsigned i = 0; i < deviceCount; i++)
		devices[i]->handleDevice(now);
}

void MppServer::addDevice(MppDevice *device) {
	++deviceCount;
	devices = (MppDevice**) realloc(devices, deviceCount * sizeof(MppDevice*));
	devices[deviceCount - 1] = device;
}

void MppServer::manageDevice(MppDevice *device, String udn) {
	addDevice(device);
	String nameKey = F("Name");
	nameKey += udn;
	device->begin(udn, properties.get(nameKey.c_str()));
	String firmware = String(VERSION) + "/" + DeviceVersion;
	device->put(FIRMWARE, firmware.c_str());
}

String MppServer::getDiscovery() {
	String result = "[";
	for (unsigned i = 0; i < deviceCount; i++) {
		if (result.length() > 1)
			result += ",";
		result += devices[i]->getJson();
	}
	result += "]";
	return result;
}

void MppServer::mppHandleDiscovery() {
	mppServer.send(200, APPL_JSON, getDiscovery());
}

void MppServer::mppHandleState(String udnString) {
//	MppSerial.printf("mppHandleState processing %s\n", mppServer.uri().c_str());
	MppDevice *device = getDevice(udnString);
	if (device != NULL)
		mppServer.send(200, APPL_JSON, device->getJson());
}

void MppServer::mppHandleName(String udnString) {
	Serial.printf("mppHandleName processing %s\n", mppServer.uri().c_str());
	MppDevice *device = getDevice(udnString);
	if (device != NULL) {
		const String &newName = mppServer.arg("name");
		String nameKey = "Name";
		nameKey += device->getUdn();
		if (!newName.length()) {
			device->put("name", udnString.c_str());
			properties.remove(nameKey.c_str());
		} else {
			device->put("name", newName.c_str());
			properties.put(nameKey.c_str(), newName.c_str());
		}
		mppServer.send(properties.save() ? 200 : 413);
	}
}

void MppServer::mppHandleSetup(MppParameters parms) {
	Serial.printf("mppHandleSetup...\n");
	String body = parms.getParameter("plain");
	if (body.length() > 0) {
		MppJson properties;
		const char *error = properties.loadFrom(body);
		if (error) {
      mppServer.send(400, error);
			Serial.println("Properties NOT loaded!");
			delay(500); // allow response to send
		} else {
			mppServer.send(200);
      Serial.println("Properties loaded");
      delay(500); // allow response to send
		}
	} else
		mppServer.send(400);
delay(500); // allow response to send
}

void MppServer::mppHandleSubscribe() {
	String ip = mppServer.arg("plain");
	int port = MPP_PORT;
	if (ip.length() == 0)
		ip = mppServer.client().remoteIP().toString();
	else {
		int i = ip.indexOf(':');
		if (i > 0) {
			port = ip.substring(i + 1).toInt();
			ip = ip.substring(0, i);
		}
	}
Serial.printf("mppHandleSubscribe processing %s from %s\n",
			mppServer.uri().c_str(), ip.c_str());
	if (ip.length() > 0) {
		MppDevice::addSubscriber(ip, port);
		mppServer.send(200);
	} else
		mppServer.send(400);
}

void MppServer::mppHandleCommand() {
	bool authenticated = (!hasProperty(P_PASSWORD)
			|| mppServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
	if (!authenticated)
		return mppServer.requestAuthentication();
	String command = mppServer.arg("run");
	if (command.length() > 0) {
		if (processCommand(command))
			mppServer.send(200);
		else
			mppServer.send(405);
	} else
		mppServer.send(400);
}

MppServer::~MppServer() {
	free(devices);
}

MppDevice* MppServer::getDevice(String udnString) {
//	MppSerial.printf("mppGetDevice finding %s...\n", udnString.c_str());
	MppDevice *device = NULL;
	for (unsigned i = 0; i < deviceCount && device == NULL; i++) {
		if (strcmp(devices[i]->getUdn(), udnString.c_str()) == 0)
			device = devices[i];
	}
	if (device == NULL)
		mppServer.send(404);
//	MppSerial.printf("mppGetDevice %s %s found.\n", udnString.c_str(),
//			device == NULL ? "not" : "");
	return device;
}

bool MppServer::mppHandleAction(HTTPMethod method, String action,
		String resource, MppParameters parms) {
	(void) method;
//	MppSerial.printf("mppHandleAction: method=%d %s on %s\n", method,
//			action.c_str(), resource.c_str());
	bool handled = false;
	// MppSerial.printf("mppHandling: checking %s for %s...\n",
	//	action.c_str(), resource.c_str());
	if (action == "state" && mppServer.method() == HTTP_GET) {
		handled = true;
		mppHandleState(resource);
	} else if (action == "name") {
		handled = true;
		mppHandleName(resource);
	} else if (action == "subscribe") {
		handled = true;
		mppHandleSubscribe();
	} else if (action == "restart") {
		handled = true;
		mppHandleRestart();
	} else if (action == "defaults") {
		handled = true;
		mppHandleProps();
	} else if (action == "survey") {
		handled = true;
		mppHandleSurvey();
	} else if (action == "version") {
		handled = true;
		mppHandleVersion();
	} else if (action == "setup") {
		handled = true;
		mppHandleSetup(parms);
	}
	if (!handled) {
		MppDevice *device = getDevice(resource);
		if (device != NULL) {
			handled = true;
			mppServer.send(device->handleAction(action, parms) ? 200 : 400);
		}
	}
	return handled;
}

void MppServer::mppHandleNotFound() {
	String uri = mppServer.uri();
	NetworkClient client = mppServer.client();
	if (client && client.connected()) {
		const IPAddress &remoteIp = client.remoteIP();
		if (remoteIp) {
			Serial.printf("handling %s %s from %s:%d...\n",
					methodToString(mppServer.method()).c_str(), uri.c_str(),
					remoteIp.toString().c_str(), client.remotePort());
			String action = uri.substring(1);
			String resource;
			int x = uri.indexOf('/', 1);
			if (x > 1) {
				action = uri.substring(1, x);
				if (x + 1 < (int) uri.length())
					resource = uri.substring(x + 1);
			}
			if (action.length() > 0) {
				MppParameters parameters(&mppServer);
				if (!mppHandleAction(mppServer.method(), action, resource,
						parameters)) {
					Serial.printf(
							"mppHandleNotFound responding 404 to '%s' from %s:%d.\n",
							uri.c_str(), client.remoteIP().toString().c_str(),
							client.remotePort());
					mppServer.send(404);
				}
			} else
				mppServer.send(400);
		}
	}
}

void MppServer::mppHandleRestart() {
	Serial.println(F("Restarting..."));
	mppServer.send(200);
	delay(250);
	ESP.restart();
}

int MppServer::handleIncomingUdp(NetworkUDP &serverUdp, int packetSize) {
	(void) packetSize; // not used
	char incoming[16]; // only "discover" is accepted
// receive incoming UDP packets
//	MppSerial.printf("Received %d bytes from %s, port %d\n", packetSize,
//			ServerUdp.remoteIP().toString().c_str(), ServerUdp.remotePort());
	int len = serverUdp.read(incoming, sizeof(incoming) - 1);
	if (len > 0) {
		incoming[len] = 0;
//		MppSerial.printf("UDP packet contents: %s\n", incoming);
		if (String(incoming).startsWith("discover"))
			sendDiscoveryResponse(serverUdp.remoteIP(), ServerUdp.remotePort());
	}
	return OK;
}

void MppServer::sendDiscoveryResponse(IPAddress remoteIp, int remotePort) {
	Serial.printf("Responding to discovery request from %s:%d\n",
			remoteIp.toString().c_str(), remotePort);
	// send discovery reply
	ServerUdp.beginPacket(remoteIp, remotePort);
	int result = ServerUdp.write((const uint8_t *)getDiscovery().c_str(),getDiscovery().length());
	ServerUdp.endPacket();
	Serial.printf("Sent discovery response to %s:%d (%d bytes sent)\n",
			remoteIp.toString().c_str(), remotePort, result);
}

// bool MppServer::onGotIP(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
bool MppServer::onGotIP() {
 // eth_connected = true;
  
//  IPAddress ip(event.got_ip.ip_info.ip.addr);
  IPAddress ip(ETH.localIP());
  IPAddress gw(ETH.gatewayIP());
  IPAddress nm(ETH.subnetMask());

 
    noteProperty(P_IP, ip.toString().c_str());
    noteProperty(P_GW, gw.toString().c_str());
    noteProperty(P_NM, nm.toString().c_str());

  
  noteProperty(P_SSID, "Ethernet");
 // Serial.printf("\n ip=%s, gw=%s, nm=%s\n.", ip.toString().c_str(),gw.toString().c_str(), nm.toString().c_str());
 Serial.println("Connected successfully to Ethernet!");
  start();
  return true;
}


void MppServer::onEventStatic(arduino_event_id_t event) { 
  mppserver.onEvent(event); 
  } 

 // void MppServer::onEvent(arduino_event_id_t event, arduino_event_info_t info) { 
 
  void MppServer::onEvent(arduino_event_id_t event) { 
      if (event == ARDUINO_EVENT_ETH_GOT_IP) { 
      Serial.println("Got IP address!");
      if (ETH.fullDuplex())
        {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
      eth_connected=true;
      onGotIP(); // Call the handler for GOT_IP 
      }else if(event == ARDUINO_EVENT_ETH_DISCONNECTED) {
        Serial.println("Ethernet disconnected!");
        eth_connected=false;
      }else if(event == ARDUINO_EVENT_ETH_STOP) {
        Serial.println("Ethernet stopped!");
        eth_connected=false;
      }else if(event == ARDUINO_EVENT_ETH_CONNECTED) {
        Serial.println("Ethernet link connected!");
        eth_connected=false;
      }else if(event == ARDUINO_EVENT_ETH_START) {
        Serial.println("Ethernet activated!");
        eth_connected=false;
    } else if(event == ARDUINO_EVENT_ETH_LOST_IP) {
        Serial.println("Device lost IP ADDRESS!");
        eth_connected=false;
    } 
  }

void MppServer::begin() {
  if(ETH.macAddress()=="00:00:00:00:00:00") {
  if( ETH.begin(ETH_PHY_TYPE,ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE))
    {
      Serial.println(ETH.localIP());
        Serial.println(ETH.macAddress());
    }else { Serial.println("ETH Service not started!"); return;}
    
  } 
    Serial.print("Waiting for IP address.. ");
    
	EthConnect = millis();


	BlinkPeriod = 500;
 networkEvents.onEvent(MppServer::onEventStatic);

    noteProperty("uid", getUID().c_str());
	for (unsigned i = 0; i < deviceCount; i++)
		devices[i]->begin();

}

const char* MppServer::getProperty(const char *property) {
	return hasProperty(property) ? properties.get(property) : NULL;
}

void MppServer::removeProperty(const char *property) {
	if (properties.contains(property)) {
		properties.remove(property);
		properties.save();
	}
}

void MppServer::putProperty(const char *property, const char *value) {
// Serial.printf("putProperty 1 pointer:%p, value:%p\n",property,value);
	bool changed = false;
	if (properties.contains(property)) {
		const char *current = properties.get(property);
		if (current == NULL && value == NULL)
			changed = false;
		else
			changed = (current == NULL && value != NULL)
					|| (current != NULL && value == NULL)
					|| strcmp(current, value) != 0;
	} else
		changed = true;
	if (changed) {
// Serial.printf("putProperty 2 pointer:%p, value:%p changed:%d \n",property,value,changed);
		properties.put(property, value);
		properties.save();
	}
}

void MppServer::noteProperty(const char *property, const char *value) {
	properties.put(property, value);
	// noted properties are not persisted
}

bool MppServer::usesProperty(const char *property) {
	return properties.contains(property);
}

void MppServer::setPropertyDefault(const char *property, const char *value) {
	if (!hasProperty(property))
		properties.put(property, value);
	// property defaults don't need to be persisted
}

int MppServer::getIntProperty(const char *property) {
	return hasProperty(property) ? properties.getInt(property) : 0;
}

unsigned MppServer::getUnsignedProperty(const char *property) {
	return hasProperty(property) ? properties.getUnsigned(property) : 0;
}

float MppServer::getFloatProperty(const char *property) {
	return hasProperty(property) ? properties.getFloat(property) : 0;
}

bool MppServer::isProperty(const char *property) {
	return hasProperty(property) ? properties.is(property) : false;
}

bool MppServer::hasProperty(const char *property) {
	return properties.has(property);
}

MppServer::MppServer(const char *deviceVersion, const char *supported[],
		const char *extraHelp, unsigned baud) :
		console(8897) {
	int count = 0;
	if (supported)
		while (supported[count] != NULL)
			++count;
	setup(deviceVersion, supported, count, baud);
	this->extraHelp = extraHelp;
}

MppServer::MppServer(const char *deviceVersion, const char *supported[],
		size_t supportedSize, const char *extraHelp, unsigned baud) :
		console(8897) {
	setup(deviceVersion, supported, supportedSize / sizeof(P_BUTTON_PIN), baud);
	this->extraHelp = extraHelp;
}

static void assignProperties(const char *keys[], unsigned count,
		MppProperties *properties) {
 	if (keys != NULL && count > 0 && properties != NULL)
		for (unsigned int i = 0; i < count && keys[i]; i++)
			if (!properties->has(keys[i]))
				properties->put(keys[i], NULL);
}

void MppServer::setup(const char *deviceVersion, const char *supported[],
		unsigned count, unsigned baud) {

	if (baud > 0)
		Serial.begin(baud);
  else Serial.begin(115200);

	// MPP_ADDRESS.fromString(MPP_ADDRESS_IP);

	devices = (MppDevice**) calloc(0, sizeof(MppDevice*));

 
	 properties.begin();
	assignProperties(supported, count, &properties);
	assignProperties(Managed, sizeof(Managed) / sizeof(char*), &properties);

// device versions
	noteProperty("Version", VERSION);
	noteProperty("MppVersion", deviceVersion);
//	noteProperty("uid", getUID().c_str());

	// setup servers
	// setup WEB server
	webServer.on(String(F("/")), std::bind(&MppServer::webHandleRoot, this));
	webServer.on(String(F("/props")),
			std::bind(&MppServer::webHandleProps, this));
	webServer.on(String(F("/setprops")),
			std::bind(&MppServer::webHandleSetProperty, this));
	webServer.on(String(F("/downloadprops")),
			std::bind(&MppServer::webHandleDownloadProps, this));
	webServer.on(String(F("/uploadprops")), HTTP_POST,
			std::bind(&MppServer::webHandleUploadedProps, this),
			std::bind(&MppServer::webHandleUploadProps, this));
	webServer.on(String(F("/reset")),
			std::bind(&MppServer::webHandleResetProperties, this));
	webServer.on(String(F("/version")),
			std::bind(&MppServer::webHandleVersion, this));
	webServer.on(String(F("/restart")),
			std::bind(&MppServer::webHandleRestart, this));
	webServer.on(String(F("/upload")), HTTP_POST,
			std::bind(&MppServer::webHandleUploadedUpdate, this),
			std::bind(&MppServer::webHandleUploadUpdate, this));

	// setup MPP server
	mppServer.on(String(F("/version")),
			std::bind(&MppServer::mppHandleVersion, this));
	mppServer.on(String(F("/defaults")),
			std::bind(&MppServer::mppHandleProps, this));
	mppServer.on(String(F("/")),
			std::bind(&MppServer::mppHandleDiscovery, this));
	mppServer.on(String(F("/restart")),
			std::bind(&MppServer::mppHandleRestart, this));
	mppServer.on(String(F("/subscribe")),
			std::bind(&MppServer::mppHandleSubscribe, this));
	mppServer.on(String(F("/check")),
			std::bind(&MppServer::mppHandleVersion, this));
	mppServer.on(String(F("/survey")),
			std::bind(&MppServer::mppHandleSurvey, this));
	mppServer.on(String(F("/command")),
			std::bind(&MppServer::mppHandleCommand, this));
	mppServer.onNotFound(std::bind(&MppServer::mppHandleNotFound, this));

}


void MppServer::handleCommand() {
	if (Serial.available())
		processCommand(Serial.readStringUntil('\n'));
}

bool MppServer::processCommand(String input) {
	if (input.length() > 0) {
		input.trim();

		 if (input.startsWith("restart")) {
			Serial.println("Restarting...");
			delay(500);
			ESP.restart();
		} else if (input.startsWith("remove ")) {
			char string[input.length() + 1];
			input.toCharArray(string, sizeof(string));
			strtok(string, " "); // strip off prop
			String key = strtok(NULL, " ");
			removeProperty(key.c_str());
			Serial.printf("%s removed\n", key.c_str());
		} else if (input.startsWith("set ")) {
			char string[input.length() + 1];
			input.toCharArray(string, sizeof(string));
			strtok(string, " "); // strip off prop
			String key = strtok(NULL, " ");
			const char *value = strtok(NULL, "");
			if (value == NULL)
				removeProperty(key.c_str());
			else if (!(key.equals(P_PASSWORD) || key.equals(P_GATEWAY_PW))
					|| strcmp(value, "********") != 0)
				putProperty(key.c_str(), value);
			Serial.printf("%s is now '%s'\n", key.c_str(),
					value == NULL ? "<null>" : value);
		} else if (input.startsWith("props")
				|| input.startsWith("properties")) {
			Serial.println(properties.toString());
		} else if (input.startsWith("save ")) {
			if (input.length() > 6) {
				properties.update(input.substring(5));
				Serial.println(properties.toString());
			}
		} else if (input.startsWith("clear")) {
			properties.clear();
			Serial.println(properties.toString());
		} 
		else if (input.startsWith("mppinfo")) {
			Serial.println((String(VERSION) + " / " + DeviceVersion).c_str());
		} else if (input.startsWith("devices")) {
			for (unsigned i = 0; i < deviceCount; i++)
				Serial.println(devices[i]->getJson());
		} 
		else if (input.startsWith("gpio ")) {
			char string[input.length() + 1];
			input.toCharArray(string, sizeof(string));
			strtok(string, " "); // strip off gpio
			String pinString = strtok(NULL, " ");
			int pin = pinString.toInt();
			if (pin >= 0 && pin <= 16) {
				String action = strtok(NULL, " ");
				if (action.length() == 0)
					Serial.println(digitalRead(pin) ? "HIGH" : "LOW");
				else {
					action.toLowerCase();
					if (action == "0" || action == "low")
						digitalWrite(pin, LOW);
					else if (action == "1" || action == "high")
						digitalWrite(pin, HIGH);
					else if (action == "out")
						pinMode(pin, OUTPUT);
					else if (action == "in") {
						String pullup = strtok(NULL, " ");
						pullup.toLowerCase();
						pinMode(pin, pullup == "pullup" ? INPUT_PULLUP : INPUT);
					} else {
						printf("Unknown command '%s'.\n", input.c_str());
						return false;
					}
					Serial.println("Ok");
				}
			}
		} else if (input.startsWith("analog")) {
			Serial.printf("A0: %d\n", analogRead(36)); // Here we read from one 36 ADC only 
		} else if (input.startsWith("memory")) {
      uint32_t chipId = 0;
        for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
	//		uint32_t realSize = ESP.getFlashChipRealSize();
			uint32_t ideSize = ESP.getFlashChipSize();
			FlashMode_t ideMode = ESP.getFlashChipMode();
//			Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
//			Serial.printf("Flash real size: %u bytes\n\n", realSize);
  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
  Serial.print("Chip ID(EMAC): ");
  Serial.println(chipId);
			Serial.printf("Flash ide  size: %lu bytes\n", ideSize);
			Serial.printf("Flash ide speed: %lu MHz\n",
					ESP.getFlashChipSpeed() / 1000000);
			Serial.printf("Flash ide mode:  %s\n",
					(ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" :
						ideMode == FM_DIO ? "DIO" :
						ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
	//		if (ideSize != realSize)
	//			Serial.println("Warning: ideSize != realSize\n");
		} 
		else if (input.startsWith("help") || input.startsWith("?")) {
			Serial.println(F("Commands:"));
			if (extraHelp != NULL)
				Serial.println(extraHelp);
			Serial.println(
					F(
							"\n mppinfo - show mpp version info"
									"\n memory - show esp memory info"
									"\n save [json] - update all properties"
									"\n set {key} {value} - update property"
									"\n remove {key} - remove property"
									"\n properties - show properties"
									"\n clear - clear properties"
									"\n devices - show device information"
									"\n eraseConfig - erase WiFi config"
									"\n restart - restart device"
									"\n gpio {n} - read gpio pin"
									"\n gpio {n} {0|1|low|high} - write gpio pin"
									"\n gpio {n} in [float|pullup] - set input mode"
									"\n gpio {n} out - set output mode"
									"\n analog - read analog input"
									"\n forceException - WDT (testing)"
									"\n"));
		} 
		else {
			printf("Unknown command '%s'.\n", input.c_str());
			return false;
		}
	} else
		return false;
	return true;
}


void MppServer::webHandleUploadUpdate() {
	HTTPUpload &upload = webServer.upload();

	if (upload.status == UPLOAD_FILE_START) {
		updateError = String();
		authenticated = (!hasProperty(P_PASSWORD)
				|| webServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
		if (!authenticated) {
			Serial.printf("Unauthenticated Update\n");
			updateError = F("Firmware update: not authenticated");
			return webServer.requestAuthentication();
		}
		Serial.printf("Update from: %s\n", upload.filename.c_str());
		uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000)
				& 0xFFFFF000;
		Update.clearError();
		if (!Update.begin(maxSketchSpace)) //start with max available size
			updateError = "Insufficient space for " + upload.filename;
	} else if (updateError.length() == 0 && !Update.hasError()) {
		if (upload.status == UPLOAD_FILE_WRITE) {
			Serial.printf(".");
			if (Update.write(upload.buf, upload.currentSize)
					!= upload.currentSize)
				updateError = "Unable to write " + upload.filename;
		} else if (upload.status == UPLOAD_FILE_END) {
			if (Update.end(true)) { //true to set the size to the current progress
				Serial.printf("\nUpdate Success: %u bytes. Rebooting...\n",
						upload.totalSize);
			} else
				updateError = "Unable to update with " + upload.filename;
		} else if (upload.status == UPLOAD_FILE_ABORTED) {
			Update.end();
			updateError = "Update aborted.";
		}
	}
	if (!authenticated)
		return webServer.requestAuthentication();
	else if (Update.hasError() || updateError.length() > 0)
		webHandleUpdateError();
}

void MppServer::webHandleUploadedUpdate() {
	if (!authenticated)
		return webServer.requestAuthentication();
	else if (Update.hasError() || updateError.length() > 0)
		webHandleUpdateError();
	else {
		webSendBackForm(F("Update success, rebooting..."));
		delay(500);
		ESP.restart();
	}
}

void MppServer::webHandleUpdateError() {
	StreamString local;
	if (updateError.length() > 0)
		local.print((updateError + " ").c_str());
	if (Update.getError() != UPDATE_ERROR_OK)
		Update.printError(local);
Serial.println();
Serial.println(local.c_str());
	webSendBackForm(local, 400);
}

void MppServer::manageButton(void (*buttonClick)(void)) {
	if (hasProperty(P_BUTTON_PIN)) {
		buttonPin = getUnsignedProperty(P_BUTTON_PIN);
		buttonClickHandler = buttonClick;
		Serial.printf("Managing button on pin %d\n", buttonPin);
		pinMode(buttonPin, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(buttonPin), &buttonHandler,
		CHANGE);
	}
}

void MppServer::webHandleUploadProps() {
	HTTPUpload &upload = webServer.upload();

	if (upload.status == UPLOAD_FILE_START) {
		propsError = String();
		propsUpdate = String();
		authenticated = (!hasProperty(P_PASSWORD)
				|| webServer.authenticate(USERNAME, getProperty(P_PASSWORD)));
		if (!authenticated) {
			Serial.printf("Unauthenticated Update\n");
			propsError = F("Properties update: not authenticated");
			return;
		}
		Serial.printf("Properties update from: %s\n", upload.filename.c_str());
	} else if (propsError.length() == 0) {
		if (upload.status == UPLOAD_FILE_WRITE) {
			Serial.printf(".");
			char data[upload.currentSize + 1];
			memcpy(data, upload.buf, upload.currentSize);
			data[upload.currentSize] = 0;
			propsUpdate += data;
		} else if (upload.status == UPLOAD_FILE_END) {
		} else if (upload.status == UPLOAD_FILE_ABORTED) {
			propsError = "Update aborted.";
		}
	}
	if (propsError.length() > 0)
		webHandlePropsError();
}

void MppServer::webHandleUploadedProps() {
	if (!authenticated)
		return webServer.requestAuthentication();
	else if (propsError.length() > 0)
		webHandlePropsError();
	else {
		properties.update(propsUpdate);
		webSendBackForm(F("Properties uploaded."));
	}
}

void MppServer::webHandlePropsError() {
	Serial.println();
	Serial.println(propsError.c_str());
	webSendBackForm(propsError, 400);
}

void MppServer::webHandleDownloadProps() {
	sendProperties(webServer);
}

void MppServer::sendUdpEvent(const char *targetIp, const char *event) {
	String message = "notify ";
	message += event;
	sendUdp(targetIp, message.c_str());
}

void MppServer::sendUdp(const char *targetIp, String message) {
	
		Serial.printf("Notifying %s with %s...\n", targetIp, message.c_str());
		NetworkUDP deviceUdp;
		deviceUdp.beginPacket(targetIp, MPP_PORT);
		deviceUdp.write((const uint8_t *)message.c_str(),message.length());
		deviceUdp.endPacket();

}

void MppServer::sendHttp(String url, String type, String body,
bool &successFlag, unsigned retry, unsigned timeout) {
	Serial.printf("sendHttp to %s %s\n", url.c_str(), body.c_str());
	if (eth_connected) {
		MppHTTPClient *http = MppHTTPClient::allocateClient(
				[this, url, type, body, &successFlag, timeout, retry](
						int httpCode, MppHTTPClient *http) {
					(void) http;
					if (httpCode == 200)
						successFlag = true;
					else if (httpCode < 0 && retry > 0) {
						Serial.printf(
								"sendHttp to %s failed with %d retry %d\n",
								url.c_str(), httpCode, retry);
						sendHttp(url, type, body, successFlag, retry - 1,
								timeout + 250);
					} else {
						// it ain't gonna work, give it up
						Serial.printf("sendHttp to %s failed with %d\n",
								url.c_str(), httpCode);
						successFlag = false;
					}
				});
		http->setTimeout(timeout);
		if (http->begin(url))
			http->sendRequest(type.c_str(), body);
		else {
			Serial.printf("sendHttp begin to '%s' failed\n", url.c_str());
			successFlag = false;
		}
	} else {
		Serial.printf("sendHttp to '%s' failed - no wifi\n", url.c_str());
		successFlag = false;
	}
}

void MppServer::sendHttpEvent(const char *targetIp, const char *event,
bool &successFlag, unsigned retry, unsigned timeout) {
	String url = String("http://") + targetIp + ":" + 4030 + "/events/" + event;
	sendHttp(url, "PUT", "", successFlag, retry, timeout);
}
