#ifndef WIRELESS_H
#define WIRELESS_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WebServer.h>


// BLE setup
void setupBLE();

// WiFi AP and HTTP server setup
void setupWiFiAPAndServer();

// HTTP server handler
void handleWiFiServer();

#endif // WIRELESS_H