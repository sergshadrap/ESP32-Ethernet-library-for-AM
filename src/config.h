#ifndef CONFIG_H 
#define CONFIG_H

 /*
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 */

#undef ETH_CLK_MODE
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN      // WT01 version
// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_PHY_POWER 16
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#include <ETH.h> // important to set up after !*/


#endif // CONFIG_H
