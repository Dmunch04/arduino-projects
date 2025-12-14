#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <RF24.h>

#define SDSelect 4

#define PASS_KEY "yeet"
#define MAX_PAYLOAD_SIZE 2048 // 255
#define COMMAND_ARGS_LIMIT 12
#define SERVICE_INDEX_OFFSET 2

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 50, 200 };

EthernetServer server(80);
StaticJsonDocument<MAX_PAYLOAD_SIZE> payloadData;

EthernetClient requestClient;
constexpr char statikaHost[] = "192.168.50.78";
constexpr char statikaReq[] = "GET /ddyz50b6/qxtdwmj5 HTTP/1.1";
constexpr int statikaPort = 3000;
StaticJsonDocument<MAX_PAYLOAD_SIZE> services;

typedef enum {
    UNKNOWN = -1,
    OK = 0,
    PAYLOAD_TOO_LARGE = 1,
} RESULT_CODE;

RESULT_CODE snipJSON(EthernetClient* client, char buffer[MAX_PAYLOAD_SIZE], const char openDelim, const char closeDelim) {
    if (client) {
        int bufferIndex = 0;
        bool isInPayload = false;
        byte depth = 0;

        while (client->connected()) {
            if (client->available()) {
                const char c = static_cast<char>(client->read());

                if (bufferIndex > MAX_PAYLOAD_SIZE - 1) {
                    return PAYLOAD_TOO_LARGE;
                }

                if (isInPayload) {
                    buffer[bufferIndex] = c;
                    bufferIndex++;

                    if (c == openDelim) {
                        depth++;
                    } else if (c == closeDelim) {
                        depth--;

                        if (depth <= 0) {
                            buffer[bufferIndex] = '\0';

                            delay(1);

                            return OK;
                        }
                    }
                } else {
                    if (c == openDelim) {
                        isInPayload = true;
                        depth++;

                        buffer[bufferIndex] = c;
                        bufferIndex++;
                    }
                }
            }
        }
    }

    return UNKNOWN;
}

bool initializeServiceDoc() {
    if (requestClient.connect(statikaHost, statikaPort)) {
        requestClient.println(statikaReq);
        requestClient.println(F("Host: statika.munchii.me"));
        requestClient.println(F("Connection: close"));
        requestClient.println();

        while (requestClient.connected() && requestClient.available() == 0) {
            delay(1);
        }

        int len = requestClient.available();
        if (len > 0) {
            char buffer[MAX_PAYLOAD_SIZE];
            int result = snipJSON(&requestClient, buffer, '[', ']');
            if (result == PAYLOAD_TOO_LARGE) {
                Serial.println(F("services payload too large"));
                return false;
            } else if (result == UNKNOWN) {
                Serial.println(F("services payload invalid"));
                return false;
            }

            deserializeJson(services, buffer);

            requestClient.stop();
            requestClient.flush();
            return true;
        }

        delay(1);
        requestClient.stop();
        requestClient.flush();

        return true;
    }

    Serial.println(F("could not connect to statika servers"));
    return false;
}

bool handleSelfCall(const char* action, char* res) {
    char* cmd[COMMAND_ARGS_LIMIT];
    byte index = 0;
    char* token = strtok(const_cast<char*>(action), " ");
    while (token != nullptr && index < COMMAND_ARGS_LIMIT) {
        cmd[index++] = token;
        token = strtok(nullptr, " ");
    }
    cmd[index] = nullptr;

    if (strcmp(cmd[0], "service") == 0) {
        if (strcmp(cmd[1], "list") == 0 || strcmp(cmd[1], "ls") == 0) {
            char buffer[MAX_PAYLOAD_SIZE];
            serializeJson(services, buffer);
            strcpy(res, buffer);
            return true;
        } else if (strcmp(cmd[1], "get") == 0) {

        } else if (strcmp(cmd[1], "refresh") == 0) {
            bool result = initializeServiceDoc();
            if (result == true) {
                strcpy(res, "successfully refreshed service list\0");
                return true;
            }

            strcpy(res, "failed to refresh service list\0");
            return false;
        } else {
            strcpy(res, "invalid service command\0");
            return false;
        }
    }

    strcpy(res, "invalid command\0");
    return false;
}

bool handleServiceCall(byte serviceId, const char* action, char* res) {
    const char* serviceName = services[serviceId - SERVICE_INDEX_OFFSET][F("name")].as<const char *>();
    const char* serviceIdS = services[serviceId - SERVICE_INDEX_OFFSET][F("serviceId")].as<const char*>();

    char one[50] = "Connecting to service ";
    char two[50] = " with action ";
    strcpy(res, strcat(strcat(one, serviceName), strcat(two, action)));

    return true;
}

void setup() {
    pinMode(SDSelect, OUTPUT);
    digitalWrite(SDSelect, HIGH);

    delay(1000);
    Serial.begin(115200);
    while (!Serial) { }
    Serial.println("starting...");

    EthernetClass::begin(mac);

    if (EthernetClass::hardwareStatus() == EthernetNoHardware) {
        Serial.println(F("missing ethernet shield"));
        while (EthernetClass::hardwareStatus() == EthernetNoHardware) {
            delay(1);
        }
        Serial.println(F("ethernet shield found"));
    }

    if (EthernetClass::linkStatus() == LinkOFF) {
        Serial.println(F("ethernet cable not connected"));
        while (EthernetClass::linkStatus() == LinkOFF) {
            delay(1);
        }
        Serial.println(F("ethernet cable connected"));
    }

    server.begin();
    Serial.print(F("server started on "));
    Serial.println(EthernetClass::localIP());

    if (initializeServiceDoc() == false) {
        Serial.println(F("failed to initialize service doc"));
        while (initializeServiceDoc() == false) {
            delay(1);
        }
        Serial.println(F("service doc initialized"));
    }

    // radio
}

void loop() {
    EthernetClient client = server.available();

    if (client) {
        char buffer[MAX_PAYLOAD_SIZE];
        int result = snipJSON(&client, buffer, '{', '}');

        if (result == PAYLOAD_TOO_LARGE) {
            client.println(F("HTTP/1.1 400 Bad Request"));
            client.println(F("Content-Type: text/plain"));
            client.println(F("Connection: close"));
            client.println();
            client.println(F("payload too large"));

            client.stop();
            client.flush();
            return;
        } else if (result == UNKNOWN) {
            client.println(F("HTTP/1.1 400 Bad Request"));
            client.println(F("Content-Type: text/plain"));
            client.println(F("Connection: close"));
            client.println();
            client.println(F("invalid payload"));

            client.stop();
            client.flush();
            return;
        } else if (result == OK) {
            deserializeJson(payloadData, buffer);

            if (strcmp(payloadData[F("key")].as<const char*>(), PASS_KEY) == 0) {
                const byte serviceId = payloadData[F("service")].as<byte>();
                bool actionResult;
                char response[MAX_PAYLOAD_SIZE] = "no response\0";

                if (serviceId == 1) {
                    actionResult = handleSelfCall(payloadData[F("action")].as<const char*>(), response);
                } else {
                    actionResult = handleServiceCall(serviceId, payloadData[F("action")].as<const char*>(), response);
                }

                if (actionResult == true) {
                    client.println(F("HTTP/1.1 200 OK"));
                    client.println(F("Content-Type: text/plain"));
                    client.println(F("Connection: close"));
                    client.println();
                    client.println(F("ok"));
                    client.println(response);

                    client.stop();
                    client.flush();
                } else {
                    client.println(F("HTTP/1.1 400 Bad Request"));
                    client.println(F("Content-Type: text/plain"));
                    client.println(F("Connection: close"));
                    client.println();
                    client.println(F("something went wrong with the action"));
                    client.println(response);

                    client.stop();
                    client.flush();
                }
            } else {
                client.println(F("HTTP/1.1 401 Unauthorized "));
                client.println(F("Content-Type: text/plain"));
                client.println(F("Connection: close"));
                client.println();
                client.println(F("invalid key"));
            }

            payloadData.clear();

            delay(1);

            client.stop();
            client.flush();
            return;
        }


        if (client) {
            client.stop();
            client.flush();
        }
    }
}