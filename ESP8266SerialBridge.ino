/*
:File:        ESP8266SerialBridge.ino

:Details:     ESP8266 WiFi <-> UART Bridge

:Date:        14-10-2024
:Author:      Mick K.
*/

#include <ESP8266WiFi.h>

//##define MODE_AP              // Access Point mode (phone connects directly to ESP) (no router)
#define MODE_ST                 // Station mode (connect to router)

#define PROTOCOL_TCP
//#define PROTOCOL_UDP
bool debug = true;

#define VERSION "1.00"
#ifdef MODE_AP
// For Access Point mode:
const char *ssid = "SerialWifi";        // This is the SSID
const char *pw = "123456789";           // and this is the PASSWORD
const char *hostname = "ESP8266UART";   // This is the hostname of the ESP
IPAddress ip(192, 168, 4, 1);           // IP address of the ESP
IPAddress netmask(255, 255, 255, 0);    // Netmask of the ESP
#endif

#ifdef MODE_ST
// For Station mode:
const char *ssid = "";                  // This is the SSID of the WiFi network
const char *pw = "";                    // and this is the PASSWORD
const char *hostname = "ESP8266UART";   // This is the hostname of the ESP
#endif

/*************************  COM Port 0 *******************************/
#define UART0_BAUD 1200                 // Baudrate UART0 (default 1200)
#define UART0_PARAM SERIAL_8N1          // Data/Parity/Stop UART0
#define UART0_PORT 8880             // Wifi Port UART0
/*************************  COM Port 1 *******************************/
#define UART1_BAUD 1200                 // Baudrate UART1 (default 1200)
#define UART1_PARAM SERIAL_8N1          // Data/Parity/Stop UART1
#define UART1_PORT 8881             // Wifi Port UART1
/*********************************************************************/
#define bufferSize 1024                 // Buffer size for UART data
#define MAX_NMEA_CLIENTS 4              // Maximum number of clients
#define NUM_COM 2                       // Number of COM ports
#define DEBUG_COM 0                     // Debug COM port
/*********************************************************************/

#ifdef PROTOCOL_TCP
#include <WiFiClient.h>                                 // include the TCP library
WiFiServer server_0(UART0_PORT);                  // create an instance of the server
WiFiServer server_1(UART1_PORT);                  // create an instance of the server
WiFiServer *server[NUM_COM] = {&server_0, &server_1};   // create an instance of the server
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];        // create an array to store the clients
#endif

#ifdef PROTOCOL_UDP
#include <WiFiUdp.h>        // include the UDP library     
WiFiUDP udp;                // create an instance of the UDP library
IPAddress remoteIp;         // create an instance of the IPAddress class
#endif

// Define the COM ports
HardwareSerial* COM[NUM_COM] = {&Serial, &Serial1};

uint8_t buf1[NUM_COM][bufferSize];
uint16_t i1[NUM_COM] = {0,0};

uint8_t buf2[bufferSize];
uint16_t i2[NUM_COM] = {0,0};

void setup() {

    delay(500);   // Delay to allow the serial port to initialize

    COM[0]->begin(UART0_BAUD, UART0_PARAM, SERIAL_FULL);
    COM[1]->begin(UART1_BAUD, UART1_PARAM, SERIAL_FULL);

    if (debug) COM[DEBUG_COM]->println("\n\nESP8266 WiFi <-> UART Bridge v" VERSION);
    
    #ifdef MODE_AP
        if (debug) COM[DEBUG_COM]->println("Open ESP Access Point mode");
        WiFi.mode(WIFI_AP);
        WiFi.hostname(hostname);
        WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP
        WiFi.softAP(ssid, pw); // configure ssid and password for softAP
    #endif


    #ifdef MODE_ST
        if (debug) COM[DEBUG_COM]->println("Open ESP Station mode");
        WiFi.mode(WIFI_STA);
        WiFi.hostname(hostname);
        WiFi.begin(ssid, pw);
        if (debug) COM[DEBUG_COM]->println("try to Connect to Wireless network: ");
        if (debug) COM[DEBUG_COM]->println(ssid);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            if (debug) COM[DEBUG_COM]->print(".");
        }
        if (debug) COM[DEBUG_COM]->println("\nWiFi connected");
    #endif


    #ifdef PROTOCOL_TCP
        Serial.println("Starting TCP Server 0");
        if (debug) COM[DEBUG_COM]->println("Starting TCP Server 0");
        server[0]->begin();
        server[0]->setNoDelay(true);
        COM[1]->println("Starting TCP Server 1");
        if (debug) COM[DEBUG_COM]->println("Starting TCP Server 1");
        server[1]->begin();
        server[1]->setNoDelay(true);
    #endif

    #ifdef PROTOCOL_UDP
        if (debug) COM[DEBUG_COM]->println("Starting UDP Server 0");
        udp.begin(UART0_PORT);

        if (debug) COM[DEBUG_COM]->println("Starting UDP Server 1");
        udp.begin(UART1_PORT);
    #endif
}


void loop() {

    #ifdef PROTOCOL_TCP
    for (int num = 0; num < NUM_COM ; num++) {

        if (server[num]->hasClient()) {

            for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {

                    // Find free/disconnected spot.
                    if (!TCPClient[num][i] || !TCPClient[num][i].connected()) {
                    if (TCPClient[num][i]) TCPClient[num][i].stop();
                    TCPClient[num][i] = server[num]->available();
                    if (debug) COM[DEBUG_COM]->print("New client for COM");
                    if (debug) COM[DEBUG_COM]->print(num);
                    if (debug) COM[DEBUG_COM]->print('/');
                    if (debug) COM[DEBUG_COM]->println(i);
                    continue;
                }
            }

            // No free/disconnected spot so reject
            WiFiClient TmpserverClient = server[num]->available();
            TmpserverClient.stop();
        }
    }
    #endif

    for (int num = 0; num < NUM_COM ; num++) {

        if (COM[num] != NULL) {

            for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {

                if (TCPClient[num][cln]) {

                    while (TCPClient[num][cln].available()) {

                        buf1[num][i1[num]] = TCPClient[num][cln].read(); // Read char from WiFi client
                        if (i1[num] < bufferSize - 1) i1[num]++; // Increment the index
                    }

                    COM[num]->write(buf1[num], i1[num]); // Write the buffer to the UART
                    i1[num] = 0;
                }
            }

            if (COM[num]->available()) {

                char myBuffer[bufferSize];   // Fixed-size buffer for UART data
                int length = 0;              // Keep track of the length of data

                // Read available data from the UART
                while (COM[num]->available() && length < bufferSize - 1) {

                    myBuffer[length++] = (char)COM[num]->read();  // Read char from UART(num)
                }
                myBuffer[length] = '\0';  // Null-terminate the string

                // Now send the received data to the WiFi clients
                for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {

                    if (TCPClient[num][cln] && TCPClient[num][cln].connected()) {

                        TCPClient[num][cln].write(myBuffer, length);  // Send the buffer to the client
                    }
                }

                i2[num] = 0;  // Reset the index
            }
        } 
    }
}

