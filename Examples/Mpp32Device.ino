#include "config.h"
#include <Arduino.h>
#include "Mpp32Devices.h"
#include "Mpp32Server.h"
#include <DallasTemperature.h>
#include <OneWire.h>

/*
#undef ETH_CLK_MODE
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN      // WT01 version
// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_PHY_POWER 16
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
 #include <ETH.h> // important to set up after !*/


const char* DeviceVersion = "Mpp32Setup 1.2.0"; // Handled Events in MppServer 
 
static const char* Properties[] = { //
        P_RELAY_PIN, //
        P_MOMENTARY, // ms,default pulse period
        P_IP_CHECK, //
        P_IP_ADDRESS, //
        P_IP_PORT, //
        P_INITIAL,
        P_PASSWORD,//
        NULL };


extern bool eth_connected;

//String  _IP;
//String  _MAC;
unsigned int AnalogPin =15;
unsigned int SensPin =39;

OneWire oneWire(AnalogPin);
DallasTemperature sensors(&oneWire);


class MppServer mppserver(DeviceVersion, Properties);

class MppRelay* relay;   
class MppSensor* sensor;
MppDevice temp; //Temperature sensor



//The setup function is called once at startup of the sketch
void setup() {
	Serial.begin(115200); 
	Serial.println("ready...");
 ETH.begin(ETH_PHY_TYPE,ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);


mppserver.setPropertyDefault(P_RELAY_PIN, "14");
 relay = new class MppRelay(mppserver.getUnsignedProperty(P_RELAY_PIN), mppserver.getUnsignedProperty(P_MOMENTARY), mppserver.isProperty(P_INITIAL));
 sensor = new class MppSensor(SensPin,false,true);
 mppserver.manageDevice(relay, getDefaultUDN(MppMomentary));
 mppserver.manageDevice(&temp, getDefaultUDN(MppAnalog) + "_T");
 mppserver.manageDevice(sensor, getDefaultUDN(MppSensor));
if (mppserver.hasProperty(P_INITIAL))
    relay->setRelay(mppserver.isProperty(P_INITIAL), 0);
 mppserver.begin();
  sensors.begin();

 sensors.requestTemperatures(); 
 delay(200);
 float avalue = sensors.getTempCByIndex(0);
  Serial.printf("Current Temperature : %.2f\n",avalue);

// Serial.printf("DEvice:%s is in state: %s\n",relay->getJson().c_str() , relay->getRelay() ? "true" : "false"); 

}

#define checkin 10000
unsigned long next = millis();

// The loop function is called in an endless loop
void loop() {
	unsigned long now = millis();

 mppserver.handleClients();
 mppserver.handleCommand();

 if (relay->doIpCheck(mppserver.getUnsignedProperty(P_IP_CHECK),
     mppserver.getProperty(P_IP_ADDRESS),
      mppserver.getUnsignedProperty(P_IP_PORT)))
    mppserver.broadcastMessage(
        String("Check to ") + mppserver.getProperty(P_IP_ADDRESS) + ":"
            + mppserver.getUnsignedProperty(P_IP_PORT)
            + " failed, relay toggled.");
            
	if (now > next) {
		Serial.printf("heap=%lus at %lus\n", ESP.getFreeHeap(), now / 1000);
//    Serial.printf("DEvice:%s is in state: %s\n",relay->getJson().c_str() , relay->getRelay() ? "true" : "false"); 
		next = now + checkin;
   sensors.requestTemperatures();
   float tempC = sensors.getTempCByIndex(0);
   temp.put(VALUE, String(tempC));
    temp.put(STATE, tempC == 0 ? "off" : "on");
     Serial.printf("Temperature: %.2f C\n", tempC);
	}
}
