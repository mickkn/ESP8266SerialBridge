/*
:File:        ESP8266SerialBridge.ino
:Details:     ESP8266 WiFi <-> UART Bridge using Software UART
:Date:        27-10-2024
:Author:      Mick K.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>  // Use SoftwareSerial, the UART pins on DevKit are used for flashing.

#define VERSION "1.00"

// Configuration
bool debug = true;         // Enable debug output on the hardware serial port
bool inverted = true;      // Invert the Software UART logic, if IrDA is used.
const char *ssid = "";     // WiFi SSID
const char *pw = "";       // WiFi password
const char *hostname = "ESP8266UART";

// COM Port Configuration
#define UART_BAUD 1200     // Baud rate for Software UART
#define UART_PORT 8880     // WiFi Port for Software UART (IrDA)
#define bufferSize 1024    // Buffer size for UART data
#define MAX_NMEA_CLIENTS 4 // Maximum number of clients

// SoftwareSerial definition: IrDA UART on GPIO14 (D5) and GPIO12 (D6)
SoftwareSerial irSerial(14, 12, inverted);  // RX = GPIO14 (D5), TX = GPIO12 (D6)

// WiFi server instance for the COM port
WiFiServer server(UART_PORT);

// TCP client array for the COM port
WiFiClient TCPClient[MAX_NMEA_CLIENTS];

// Buffers for reading and writing data
uint8_t buf1[bufferSize];
uint16_t i1 = 0;
uint8_t buf2[bufferSize];
uint16_t i2 = 0;

void setup() {
    // Delay for the Serial Monitor to start
    delay(500);

    // Begin SoftwareSerial for the IrDA communication
    irSerial.begin(UART_BAUD);

    // Initialize hardware serial for debugging
    Serial.begin(115200);
    if (debug) Serial.println("\n\nOpen ESP8266 WiFi <-> UART Bridge v" VERSION);
    if (debug) Serial.println("This is DEBUG on UART0 that will print all data flow.");

    // Initialize WiFi connection
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostname);
    WiFi.begin(ssid, pw);
    if (debug) Serial.println("Connecting to Wireless network: ");
    if (debug) Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (debug) Serial.print(".");
    }
    if (debug) Serial.println("\nWiFi connected");

    // Start TCP Server
    server.begin();
    server.setNoDelay(true);
}

void loop() {
    // Check for new clients and manage existing clients
    if (server.hasClient()) {
        for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
            if (!TCPClient[i] || !TCPClient[i].connected()) {
                if (TCPClient[i]) TCPClient[i].stop();
                TCPClient[i] = server.available();
                if (debug) {
                    Serial.print("New client for COM");
                    Serial.print('/');
                    Serial.println(i);
                }
                continue;
            }
        }
        // Reject client if no free/disconnected spot
        WiFiClient tmpClient = server.available();
        tmpClient.stop();
    }

    // Process UART data and send to WiFi clients
    if (irSerial) {
        for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
            if (TCPClient[cln]) {
                while (TCPClient[cln].available()) {
                    buf1[i1] = TCPClient[cln].read();
                    if (i1 < bufferSize - 1) i1++;
                }

                // Write to the UART
                irSerial.write(buf1, i1);

                // Send data to the debug channel if debug is enabled
                if (debug) {
                    Serial.write(buf1, i1);
                }

                i1 = 0;
            }
        }

        // Read from UART and send to WiFi clients
        if (irSerial.available()) {
            char myBuffer[bufferSize];
            int length = 0;

            while (irSerial.available() && length < bufferSize - 1) {
                myBuffer[length++] = (char)irSerial.read();
            }
            myBuffer[length] = '\0';

            // Send the buffer to each connected WiFi client
            for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
                if (TCPClient[cln] && TCPClient[cln].connected()) {
                    TCPClient[cln].write(myBuffer, length);
                }
            }

            // Send the buffer to the debug channel if debug is enabled
            if (debug) {
                Serial.write(myBuffer, length);
            }

            i2 = 0;
        }
    }
}