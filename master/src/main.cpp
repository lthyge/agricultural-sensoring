/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp-now-two-way-communication-esp32/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <esp_now.h>
#include <WiFi.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wifi_Secrets.h>
#include <Wifi_Credentials.h>

AsyncWebServer server(80);

const char *ssid = SSID;
const char *password = PASSWORD;

// Incoming readings
String incomingMsg;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message
{
    String msg;
} struct_message;

// Create a struct_message to hold incoming sensor readings
struct_message incomingMessage;

// Not found
void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

// Callback when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
    Serial.print("Bytes received: ");
    Serial.println(len);
    incomingMsg = incomingMessage.msg;
    Serial.println(incomingMsg);
}

void setup()
{
    // Init Serial Monitor
    Serial.begin(115200);

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PWD, 6);
    Serial.print("Mac Address: ");
    Serial.println(WiFi.macAddress());

    // Init WiFi
    // WiFi.begin(ssid, password);
    // if (WiFi.waitForConnectResult() != WL_CONNECTED)
    // {
    //     Serial.printf("WiFi Failed!\n");
    //     return;
    // }

    Serial.print("IP Address: ");
    // Serial.println(WiFi.localIP());
    Serial.println(WiFi.softAPIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    else
    {
        Serial.println("ESP-NOW initialized successfully!");
    }

    // Once ESPNow
    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        Serial.println("Incoming connection on /");
        request->send(200, "text/plain", incomingMsg); 
    });

    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        Serial.println("Connection recieved on /test");
        request->send(200, "text/plain", "This is a test page!");
    });

    server.onNotFound(notFound);
    server.begin();
}

void loop()
{
}