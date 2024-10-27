/*
:File:        ESP32SerialBridge.ino
:Details:     ESP32 WiFi <-> UART Bridge using Hardware UART2
:Date:        27-10-2024
:Author:      Mick K.
*/

#include <WiFi.h>          // Use ESP32 WiFi library
#include <WiFiClient.h>    // Use ESP32 WiFiClient

#define VERSION "1.00"

// Configuration
bool debug = true;         // Enable debug output on the hardware serial port
const char *ssid = "";     // WiFi SSID
const char *pw = "";       // WiFi password
const char *hostname = "ESP32UART";

// COM Port Configuration
#define UART_BAUD 1200     // Baud rate for UART2
#define UART_PORT 8880     // WiFi Port for UART
#define bufferSize 1024    // Buffer size for UART data
#define MAX_NMEA_CLIENTS 4 // Maximum number of clients

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

    // Initialize hardware Serial2 for the IrDA communication on default pins RX2 (GPIO16) and TX2 (GPIO17)
    Serial2.begin(UART_BAUD);

    // Initialize hardware serial for debugging
    Serial.begin(115200);
    if (debug) Serial.println("\n\nOpen ESP32 WiFi <-> UART Bridge v" VERSION);
    if (debug) Serial.println("This is DEBUG on UART0 that will print all data flow.");

    // Initialize WiFi connection
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.begin(ssid, pw);
    if (debug) Serial.println("Connecting to Wireless network: ");
    if (debug) Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (debug) Serial.print(".");
    }
    if (debug) {
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }

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
                    Serial.print("New client for COM ");
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
    if (Serial2) {
        for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
            if (TCPClient[cln]) {
                while (TCPClient[cln].available()) {
                    buf1[i1] = TCPClient[cln].read();
                    if (i1 < bufferSize - 1) i1++;
                }

                // Write to the UART
                Serial2.write(buf1, i1);

                // Send data to the debug channel if debug is enabled
                if (debug) {
                    Serial.write(buf1, i1);
                }

                i1 = 0;
            }
        }

        // Read from UART and send to WiFi clients
        if (Serial2.available()) {
            char myBuffer[bufferSize];
            int length = 0;

            while (Serial2.available() && length < bufferSize - 1) {
                myBuffer[length++] = (char)Serial2.read();
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
