/*
:File:        ESP8266SerialBridge.ino
:Details:     ESP8266 WiFi <-> UART Bridge using Software UART
:Date:        27-10-2024
:Author:      Mick K.
*/

/*
Description:

This sketch creates a WiFi <-> UART bridge using the ESP8266 module. 
The sketch uses SoftwareSerial to create two COM ports, one for the 
debug output and one for the IrDA communication. The sketch creates 
two TCP servers, one for each COM port, and listens for incoming connections. 
The sketch reads data from the SoftwareSerial ports and sends it to the 
connected clients. The sketch also reads data from the connected clients 
and sends it to the SoftwareSerial ports.

The IrDA should be connected to GPIO14 (D5) and GPIO12 (D6) and 3V and GND.
When powered up, the IrDA Light can be checked with a iPhone front camera,
and should only light up when data is sent. If not the inverted flag should be toggled.

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>  // Use SoftwareSerial, the UART pins on DevKit are used for flashing.

#define VERSION "1.00"

// Configuration
bool debug = false;         // Enable debug output on the debug COM port RX = GPIO0 (D3), TX = GPIO2 (D4)
bool inverted = true;       // Invert the Software UART logic, if IrDA is used.
const char *ssid = "";      // WiFi SSID
const char *pw = "";        // WiFi password
const char *hostname = "ESP8266UART";

// COM Port Configuration
#define UART0_BAUD 1200  // Baud rate for Software UART0
#define UART1_BAUD 1200  // Baud rate for Software UART1
#define UART0_PORT 8880  // WiFi Port for Software UART0 (IrDA)
#define UART1_PORT 8881  // WiFi Port for Software UART1 (Debug)
#define bufferSize 1024  // Buffer size for UART data
#define MAX_NMEA_CLIENTS 4
#define NUM_COM 2
#define DEBUG_COM 1  // Debug COM port 1

// SoftwareSerial definitions: Debug UART on GPIO0 (D3) and GPIO2 (D4)
SoftwareSerial irSerial(14, 12, inverted);  // RX = GPIO14 (D5), TX = GPIO12 (D6)

// Secondary UART on GPIO14 (D5) and GPIO12 (D6)
SoftwareSerial debugSerial(0, 2);  // RX = GPIO0 (D3), TX = GPIO2 (D4) for debug

// Map the COM array to SoftwareSerial instances
SoftwareSerial *COM[NUM_COM] = {&irSerial, &debugSerial};

// WiFi server instances for each COM port
WiFiServer server_0(UART0_PORT);
WiFiServer server_1(UART1_PORT);
WiFiServer *server[NUM_COM] = {&server_0, &server_1};

// TCP client array for each COM port
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];

// Buffers for reading and writing data
uint8_t buf1[NUM_COM][bufferSize];
uint16_t i1[NUM_COM] = {0, 0};
uint8_t buf2[bufferSize];
uint16_t i2[NUM_COM] = {0, 0};

void setup() {

    // Delay for the Serial Monitor to start
    delay(500);

    // Begin SoftwareSerial for each COM port
    COM[0]->begin(UART0_BAUD);
    COM[1]->begin(UART1_BAUD);

    // Initialize WiFi connection
    if (debug) COM[DEBUG_COM]->println("\n\nOpen ESP8266 WiFi <-> UART Bridge v" VERSION);
    if (debug) COM[DEBUG_COM]->println("This is DEBUG on UART1 that will print all data flow.");
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostname);
    WiFi.begin(ssid, pw);
    if (debug) COM[DEBUG_COM]->println("Connecting to Wireless network: ");
    if (debug) COM[DEBUG_COM]->println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (debug) COM[DEBUG_COM]->print(".");
    }
    if (debug) COM[DEBUG_COM]->println("\nWiFi connected");

    // Start TCP Servers for each COM port
    server[0]->begin();
    server[0]->setNoDelay(true);
    server[1]->begin();
    server[1]->setNoDelay(true);
}

void loop() {

    // Check for new clients and manage existing clients for each COM port
    for (int num = 0; num < NUM_COM; num++) {
        if (server[num]->hasClient()) {
            for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
                if (!TCPClient[num][i] || !TCPClient[num][i].connected()) {
                    if (TCPClient[num][i]) TCPClient[num][i].stop();
                    TCPClient[num][i] = server[num]->available();
                    if (debug) {
                        COM[DEBUG_COM]->print("New client for COM");
                        COM[DEBUG_COM]->print(num);
                        COM[DEBUG_COM]->print('/');
                        COM[DEBUG_COM]->println(i);
                    }
                    continue;
                }
            }
            // Reject client if no free/disconnected spot
            WiFiClient tmpClient = server[num]->available();
            tmpClient.stop();
        }
    }

    // Process UART data and send to WiFi clients
    for (int num = 0; num < NUM_COM; num++) {
        if (COM[num]) {
            for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
                if (TCPClient[num][cln]) {
                    while (TCPClient[num][cln].available()) {
                        buf1[num][i1[num]] = TCPClient[num][cln].read();
                        if (i1[num] < bufferSize - 1) i1[num]++;
                    }

                    // Write to the UART
                    COM[num]->write(buf1[num], i1[num]);

                    // Send data to the debug channel if debug is enabled
                    if (debug) {
                        COM[DEBUG_COM]->write(buf1[num], i1[num]);
                    }

                    i1[num] = 0;
                }
            }

            // Read from UART and send to WiFi clients
            if (COM[num]->available()) {
                char myBuffer[bufferSize];
                int length = 0;

                while (COM[num]->available() && length < bufferSize - 1) {
                    myBuffer[length++] = (char)COM[num]->read();
                }
                myBuffer[length] = '\0';

                // Send the buffer to each connected WiFi client
                for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
                    if (TCPClient[num][cln] && TCPClient[num][cln].connected()) {
                        TCPClient[num][cln].write(myBuffer, length);
                    }
                }

                // Send the buffer to the debug channel if debug is enabled
                if (debug) {
                    COM[DEBUG_COM]->write(myBuffer, length);
                }

                i2[num] = 0;
            }
        }
    }
}
