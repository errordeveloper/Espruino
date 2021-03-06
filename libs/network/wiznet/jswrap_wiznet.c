/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * This file is designed to be parsed during the build process
 *
 * Contains built-in functions for CC3000 WiFi Access
 * EXTREMELY BETA AND LIKELY TO CHANGE DRASTICALLY
 * ----------------------------------------------------------------------------
 */

#include "jswrap_wiznet.h"
#include "jshardware.h"
#include "jsinteractive.h"
#include "network.h"
// wiznet driver
#include "wizchip_conf.h"

#include "network_wiznet.h"
#include "DHCP/dhcp.h"

#define ETH_SPI          EV_SPI3
#define ETH_CS_PIN       (Pin)(JSH_PORTB_OFFSET + 2) // active low
#define ETH_CLK_PIN      (Pin)(JSH_PORTB_OFFSET + 3)
#define ETH_MISO_PIN     (Pin)(JSH_PORTB_OFFSET + 4)
#define ETH_MOSI_PIN     (Pin)(JSH_PORTB_OFFSET + 5)

void  wizchip_select(void)
{
  jshPinOutput(ETH_CS_PIN, 0); // active low
}

void  wizchip_deselect(void)
{
  jshPinOutput(ETH_CS_PIN, 1); // active low
}

static uint8_t wizchip_rw(uint8_t data) {
  int r = jshSPISend(ETH_SPI, data);
  if (r<0) r = jshSPISend(ETH_SPI, -1);
  return (uint8_t)r;
}

void  wizchip_write(uint8_t wb)
{
  wizchip_rw(wb);
}

uint8_t wizchip_read()
{
   return wizchip_rw(0xFF);
}


/*JSON{ "type":"library",
        "class" : "WIZnet",
        "description" : ""
}*/
/*JSON{ "type":"staticmethod", 
         "class" : "WIZnet", "name" : "connect",
         "generate" : "jswrap_wiznet_connect",
         "description" : "Initialise the WIZnet module and return an Ethernet object",
         "params" : [ ],
         "return" : ["JsVar", "A WLAN Object"]
}*/
JsVar *jswrap_wiznet_connect() {
  JsVar *ethObj = jspNewObject(0, "Ethernet");

  // SPI config
  JshSPIInfo inf;
  jshSPIInitInfo(&inf);
  inf.pinSCK =  ETH_CLK_PIN;
  inf.pinMISO = ETH_MISO_PIN;
  inf.pinMOSI = ETH_MOSI_PIN;
  inf.baudRate = 1000000;
  inf.spiMode = SPIF_SPI_MODE_0;
  jshSPISetup(ETH_SPI, &inf);

  // CS Configuration
  jshSetPinStateIsManual(ETH_CS_PIN, false);
  jshPinOutput(ETH_CS_PIN, 1); // de-assert CS

  // Wiznet
  reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
  reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

  /* wizchip initialize*/
  uint8_t tmp;
  uint8_t memsize[2][8] = { {2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};

  if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1)
  {
    jsiConsolePrint("WIZCHIP Initialized fail.\r\n");
    return 0;
  }

  /* PHY link status check */
  do {
    if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1) {
      jsiConsolePrint("Unknown PHY Link status.\r\n");
      return 0;
    }
  } while (tmp == PHY_LINK_OFF);

  JsNetwork net;
  networkCreate(&net, JSNETWORKTYPE_W5500);
  networkFree(&net);

  networkState = NETWORKSTATE_ONLINE;

  return ethObj;
}

/*JSON{ "type":"class",
        "class" : "Ethernet",
        "description" : "An instantiation of an Ethernet network adaptor"
}*/

/*JSON{ "type":"method",
         "class" : "Ethernet", "name" : "getIP",
         "generate" : "jswrap_ethernet_getIP",
         "description" : "Get the current IP address",
         "return" : ["JsVar", ""]
}*/
JsVar *jswrap_ethernet_getIP(JsVar *wlanObj) {
  NOT_USED(wlanObj);

  if (networkState != NETWORKSTATE_ONLINE) {
    jsError("Not connected to the internet");
    return 0;
  }

  wiz_NetInfo gWIZNETINFO;
  ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);

  /* If byte 1 is 0 we don't have a valid address */
  JsVar *data = jsvNewWithFlags(JSV_OBJECT);
  networkPutAddressAsString(data, "ip", &gWIZNETINFO.ip[0], 4, 10, '.');
  networkPutAddressAsString(data, "subnet", &gWIZNETINFO.sn[0], 4, 10, '.');
  networkPutAddressAsString(data, "gateway", &gWIZNETINFO.gw[0], 4, 10, '.');
  networkPutAddressAsString(data, "dns", &gWIZNETINFO.dns[0], 4, 10, '.');
  networkPutAddressAsString(data, "mac", &gWIZNETINFO.mac[0], 6, 16, 0);
  return data;
}


void _eth_getIP_set_address(JsVar *options, char *name, unsigned char *ptr) {
  JsVar *info = jsvObjectGetChild(options, name, 0);
  if (info) {
    char buf[64];
    jsvGetString(info, buf, sizeof(buf));
    *(unsigned long*)ptr = networkParseIPAddress(buf);
    jsvUnLock(info);
  }
}

/*JSON{ "type":"method",
         "class" : "Ethernet", "name" : "setIP",
         "generate" : "jswrap_ethernet_setIP",
         "description" : "Set the current IP address",
         "params" : [ [ "options", "JsVar", "Object containing IP address options `{ ip : '1,2,3,4', subnet, gateway, dns  }`"] ],
         "return" : ["bool", "True on success"]
}*/
bool jswrap_ethernet_setIP(JsVar *wlanObj, JsVar *options) {
  NOT_USED(wlanObj);

  if (networkState != NETWORKSTATE_ONLINE) {
    jsError("Not connected to the internet");
    return false;
  }

  bool success = false;
  wiz_NetInfo gWIZNETINFO;

  if (jsvIsObject(options)) {
    ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);
    _eth_getIP_set_address(options, "ip", &gWIZNETINFO.ip[0]);
    _eth_getIP_set_address(options, "subnet", &gWIZNETINFO.sn[0]);
    _eth_getIP_set_address(options, "gateway", &gWIZNETINFO.gw[0]);
    _eth_getIP_set_address(options, "dns", &gWIZNETINFO.dns[0]);
    gWIZNETINFO.dhcp = NETINFO_STATIC;
    success = true;
  } else {
    // DHCP
    uint8_t DHCPisSuccess = getIP_DHCPS(net_wiznet_getFreeSocket(), &gWIZNETINFO);
    if (DHCPisSuccess == 1) {
      // info in lease_time.lVal
      success = true;
    } else {
      jsWarn("DHCP failed");
      success = false;
    }
  }

  ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
  return success;
}

