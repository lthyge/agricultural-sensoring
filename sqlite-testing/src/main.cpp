/*
    This creates two empty databases, populates values, and retrieves them back
    from the SPIFFS file
*/
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"
#include <esp_now.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <Wifi_Secrets.h>
#include <Wifi_Credentials.h>

// SQLITE vars
sqlite3 *db1;
int rc;
typedef struct dataStruct
{
    String id;
    String content;
} dataStruct;

AsyncWebServer server(80);

// const char *ssid = SSID;
// const char *password = PASSWORD;

// Incoming readings
String incomingMsg;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message
{
    String msg;
} struct_message;
dataStruct firstRow;

// Create a struct_message to hold incoming sensor readings
struct_message incomingMessage;

// Not found
void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true

const char *data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    Serial.printf("%s: ", (const char *)data);
    firstRow.id = argv[0];
    firstRow.content = argv[1];

    for (i = 0; i < argc; i++)
    {
        Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    Serial.printf("\n");
    return 0;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql)
{
    Serial.println(sql);
    long start = micros();
    int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        Serial.printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else
    {
        Serial.printf("Operation done successfully\n");
    }
    Serial.print(F("Time taken:"));
    Serial.println(micros() - start);
    return rc;
}

bool dataReceived = false;

// Callback when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    Serial.println("Data received");
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
    // Serial.print("Bytes received: ");
    // Serial.println(len);
    incomingMsg = incomingMessage.msg;
    dataReceived = true;
    char buffer[200];
    sprintf(buffer, "INSERT INTO test1 (id, content) VALUES (%d, '%s');", millis(), incomingMessage);
    rc = db_exec(db1, buffer);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db1);
        return;
    }
    // Serial.println(incomingMsg);
}

int db_open(const char *filename, sqlite3 **db)
{
    int rc = sqlite3_open(filename, db);
    if (rc)
    {
        Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
        return rc;
    }
    else
    {
        Serial.printf("Opened database successfully\n");
    }
    return rc;
}

void setup()
{

    Serial.begin(115200);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("Failed to mount file system");
        return;
    }

    // list SPIFFS contents
    File root = SPIFFS.open("/");
    if (!root)
    {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }

    // remove existing file
    SPIFFS.remove("/test1.db");

    sqlite3_initialize();

    if (db_open("/spiffs/test1.db", &db1))
        return;

    rc = db_exec(db1, "CREATE TABLE test1 (id INTEGER, content);");
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db1);
        return;
    }

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
        request->send(200, "text/plain", incomingMsg); });

    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        Serial.println("Connection recieved on /test");
        request->send(200, "text/plain", "This is a test page!"); });
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        Serial.println("Connection recieved on /data");
        rc = db_exec(db1, "SELECT * FROM test1 ORDER BY id DESC LIMIT 1");
        char buffer[200];
        sprintf(buffer, "Id: %s, Content: %s", firstRow.id, firstRow.content);
        request->send(200, "text/plain", buffer); });

    server.onNotFound(notFound);
    server.begin();
}

void loop()
{
    // Set values to send
    // message.msg = "Hello, World!";

    // Send message via ESP-NOW
    // esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&message, sizeof(message));

    if (dataReceived)
    {
        Serial.println("dataReceived true");
        rc = db_exec(db1, "SELECT * FROM test1");
        if (rc != SQLITE_OK)
        {
            sqlite3_close(db1);
            return;
        }
        dataReceived = false;
    }
    delay(10000);
}