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
 * Contains functions for handling JsNetwork and doing common networking tasks
 * ----------------------------------------------------------------------------
 */
#include "network.h"
#include "jsparse.h"

#if defined(USE_CC3000)
  #include "network_cc3000.h"
#endif
#if defined(USE_WIZNET)
  #include "network_wiznet.h"
#endif
#if defined(LINUX)
  #include "network_linux.h"
#endif

JsNetworkState networkState =
#ifdef LINUX
    NETWORKSTATE_ONLINE
#else
    NETWORKSTATE_OFFLINE
#endif
    ;

unsigned long networkParseIPAddress(const char *ip) {
  int n = 0;
  unsigned long addr = 0;
  while (*ip) {
    if (*ip>='0' && *ip<='9') {
      n = n*10 + (*ip-'0');
    } else if (*ip>='.') {
      addr = (addr>>8) | (unsigned long)(n<<24);
      n=0;
    } else {
      return 0; // not an ip address
    }
    ip++;
  }
  addr = (addr>>8) | (unsigned long)(n<<24);
  return addr;
}

void networkPutAddressAsString(JsVar *object, const char *name,  unsigned char *ip, int nBytes, unsigned int base, char separator) {
  char data[64] = "";
  int i = 0, dir = 1, l = 0;
  if (nBytes<0) {
    i = (-nBytes)-1;
    nBytes = -1;
    dir=-1;
  }
  for (;i!=nBytes;i+=dir) {
    if (base==16) {
      data[l++] = itoch(ip[i]>>4);
      data[l++] = itoch(ip[i]&15);
    } else {
      itoa((int)ip[i], &data[l], base);
    }
    l = (int)strlen(data);
    if (i+dir!=nBytes && separator) {
      data[l++] = separator;
      data[l] = 0;
    }
  }

  JsVar *dataVar = jsvNewFromString(data);
  jsvUnLock(jsvObjectSetChild(object, name, dataVar));
}

/** Some devices (CC3000) store the IP address with the first element last, so we must flip it */
unsigned long networkFlipIPAddress(unsigned long addr) {
  return
      ((addr&0xFF)<<24) |
      ((addr&0xFF00)<<8) |
      ((addr&0xFF0000)>>8) |
      ((addr&0xFF000000)>>24);
}

/// Get an IP address from a name. Sets out_ip_addr to 0 on failure
void networkGetHostByName(JsNetwork *net, char * hostName, unsigned long* out_ip_addr) {
  assert(out_ip_addr);
  *out_ip_addr = 0;

  *out_ip_addr = networkParseIPAddress(hostName); // first try and simply parse the IP address
  if (!*out_ip_addr)
    net->gethostbyname(net, hostName, out_ip_addr);
}



void networkCreate(JsNetwork *net, JsNetworkType type) {
  net->networkVar = jsvNewStringOfLength(sizeof(JsNetworkData));
  net->data.type = type;
  jsvUnLock(jsvObjectSetChild(execInfo.root, NETWORK_VAR_NAME, net->networkVar));
  networkSet(net);
  networkGetFromVar(net);
}

bool networkGetFromVar(JsNetwork *net) {
  net->networkVar = jsvObjectGetChild(execInfo.root, NETWORK_VAR_NAME, 0);
  if (!net->networkVar) {
#ifdef LINUX
    networkCreate(net, JSNETWORKTYPE_SOCKET);
    return true;
#else
    return false;
#endif
  }
  jsvGetString(net->networkVar, (char *)&net->data, sizeof(JsNetworkData)+1/*trailing zero*/);

  switch (net->data.type) {
#if defined(USE_CC3000)
  case JSNETWORKTYPE_CC3000 : netSetCallbacks_cc3000(net); break;
#endif
#if defined(USE_WIZNET)
  case JSNETWORKTYPE_W5500 : netSetCallbacks_wiznet(net); break;
#endif
#if defined(LINUX)
  case JSNETWORKTYPE_SOCKET : netSetCallbacks_linux(net); break;
#endif
  default:
    jsError("Unknown network device %d", net->data.type);
    networkFree(net);
    return false;
  }
  return true;
}

bool networkGetFromVarIfOnline(JsNetwork *net) {
  bool found = networkGetFromVar(net);
  if (!found || networkState != NETWORKSTATE_ONLINE) {
    jsError("Not connected to the internet");
    if (found) networkFree(net);
    return false;
  }
  return true;
}

void networkSet(JsNetwork *net) {
  jsvSetString(net->networkVar, (char *)&net->data, sizeof(JsNetworkData));
}

void networkFree(JsNetwork *net) {
  jsvUnLock(net->networkVar);
}
