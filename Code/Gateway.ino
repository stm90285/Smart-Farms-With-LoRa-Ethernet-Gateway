#include <HardwareSerial.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // Replace with your MAC address.
const char* server = "industrial.api.ubidots.com";
int port = 80; // HTTP port.

EthernetClient client;
HardwareSerial Serial2(PA3,PD5);

StaticJsonDocument<512> jsonResponse;


int motorState = 0;

static char recv_buf[512];
static bool is_exist = false;

static int at_send_check_response(char *p_ack, int timeout_ms, char*p_cmd, ...)
{
    int ch = 0;
    int index = 0;
    int startMillis = 0;
    va_list args;
    memset(recv_buf, 0, sizeof(recv_buf));
    va_start(args, p_cmd);
    Serial2.printf(p_cmd, args);
    Serial.printf(p_cmd, args);
    va_end(args);
    delay(200);
    startMillis = millis();

    if (p_ack == NULL)
    {
        return 0;
    }

    do
    {
        while (Serial2.available() > 0)
        {
            ch = Serial2.read();
            recv_buf[index++] = ch;
            Serial.print((char)ch);
            delay(2);
        }

        if (strstr(recv_buf, p_ack) != NULL)
        {
            return 1;
        }

    } while (millis() - startMillis < timeout_ms);
    return 0;
}

static int recv_prase(void)
{
    char ch;
    int index = 0;
    memset(recv_buf, 0, sizeof(recv_buf));
    while (Serial2.available() > 0)
    {
        ch = Serial2.read();
        recv_buf[index++] = ch;
        Serial.print((char)ch);
        delay(2);
    }

    if (index)
    {
        char *p_start = NULL;
        char data[32] = {
            0,
        };
        int rssi = 0;
        int snr = 0;
        int husky;
        char tem[10],hum[10],moi[10];
        float moisture;
        
        p_start = strstr(recv_buf, "+TEST: RX \"5345454544");
        if (p_start)
        {
            p_start = strstr(recv_buf, "5345454544");
            if (p_start && (1 == sscanf(p_start, "5345454544%s", data)))
            {
                sscanf(p_start,"5345454544%5s%5s%5s%d",&tem,&hum,&moi,&husky);
                findAndReplace(tem, 'A', '.');
                findAndReplace(hum, 'A', '.');
                findAndReplace(moi, 'A', '.');
                moisture = atof(moi);
                Serial.print("Temp: ");
                Serial.print(tem);
                Serial.print("\r\n");
                Serial.print("Humidity: ");
                Serial.print(hum);
                Serial.print("\r\n");
                Serial.print("Moisture: ");
                Serial.print(moi);
                Serial.print("\r\n");
                Serial.print("HuskyStatus: ");
                Serial.print(husky);
                Serial.print("\r\n");
                PostHttpRequest(tem, hum, String(moisture), String(husky));
            }

            p_start = strstr(recv_buf, "RSSI:");
            if (p_start && (1 == sscanf(p_start, "RSSI:%d,", &rssi)))
            {
                Serial.print("rssi:");
                Serial.print(rssi);
                Serial.print("\r\n");
            }
            p_start = strstr(recv_buf, "SNR:");
            if (p_start && (1 == sscanf(p_start, "SNR:%d", &snr)))
            {
                Serial.print("snr :");
                Serial.print(snr);
                Serial.print("\r\n");
            }
            return 1;
        }
    }
    return 0;
}

static int node_recv(uint32_t timeout_ms)
{
    at_send_check_response("+TEST: RXLRPKT", 1000, "AT+TEST=RXLRPKT\r\n");
    int startMillis = millis();
    do
    {
        if (recv_prase())
        {
            return 1;
        }
    } while (millis() - startMillis < timeout_ms);
    return 0;
}

static int node_send(void)
{   
    Serial2.begin(9600);
    while (!Serial2);
    
    static uint16_t count = 0;
    int ret = 0;
    char data[64];
    char cmd[256];

    memset(data, 0, sizeof(data));
    sprintf(data, "%d", motorState);
    sprintf(cmd, "AT+TEST=TXLRPKT,\"5345454544%s\"\r\n", data);

    ret = at_send_check_response("TX DONE", 2000, cmd);
    if (ret == 1)
    {

        Serial.print("Sent successfully!\r\n");
    }
    else
    {
        Serial.print("Send failed!\r\n");
    }
    return ret;
}

static void node_recv_then_send(uint32_t timeout)
{
    int ret = 0;
    ret = node_recv(timeout);
    delay(100);
    if (!ret)
    {
        Serial.print("\r\n");
        return;
    }
    node_send();
    Serial.print("\r\n");
}

static void node_send_then_recv(uint32_t timeout)
{   
    GetHttpRequest();
    int ret = 0;
    ret = node_send();
    if (!ret)
    {
        Serial.print("\r\n");
        return;
    }
    /*if (!node_recv(timeout))
    {
        Serial.print("recv timeout!\r\n");
    }
    Serial.print("\r\n");*/

    int ret2 = 0;
    ret2 = node_recv(timeout);
    delay(10);
    if (!ret2)
    {
        Serial.print("\r\n");
        return;
    }
    
}

void findAndReplace(char *str, char findChar, char replaceChar) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == findChar) {
            str[i] = replaceChar;
        }
    }
}

void PostHttpRequest(String temperature, String humidity, String moisture, String husky){
  Ethernet.begin(mac);
  Serial.println("Start Sending Data");
  if (client.connect(server, port)) 
  {
    Serial.println("Connected to Ubidots");
    // Create the JSON payload.
    String payload = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity)+ ",\"moisture\":" + String(moisture)+",\"husky\":" + String(husky)+"}";
    Serial.println("Payload: "+payload);
    // Create the HTTP request headers.
    String headers = "POST /api/v1.6/devices/w5300/ HTTP/1.1\r\n";
    headers += "Host: industrial.api.ubidots.com\r\n";
    headers += "Content-Type: application/json\r\n";
    headers += "X-Auth-Token: BBFF-fR1e87CgVqFhTR7RilQ8r9GPxAPKYC\r\n";
    headers += "Content-Length: ";
    headers += payload.length();
    headers += "\r\n\r\n";
    
    // Send the HTTP request to Ubidots using TCP.
    client.print(headers);
    client.print(payload);
    
    // Print the server's response to the serial monitor.
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }
    
    // Close the connection.
    client.stop();
    Serial.println("\nDisconnected from server");
  } else {
    Serial.println("Connection to Ubidots failed");
  }

}

int GetHttpRequest() {
  int value;
  Ethernet.begin(mac);
  Serial.println("Getting Data From Ubidots");
  
  if (client.connect(server, port)) {
    Serial.println("Connected to Ubidots");
    String httpResponse;
    String headers = "GET /api/v1.6/variables/64d9ae5f30002d000e0d40db/values/?page_size=1 HTTP/1.1\r\n";
    headers += "Host: industrial.api.ubidots.com\r\n";
    headers += "X-Auth-Token: BBFF-fR1e87CgVqFhTR7RilQ8r9GPxAPKYC\r\n";
    headers += "Connection: close\r\n\r\n";
    
    client.print(headers);
    
    while (client.connected()) {
      if (client.available()) {
        String httpResponse = client.readStringUntil('\n');
        if (httpResponse.startsWith("{\"count\":")){
            Serial.println(httpResponse);
            deserializeJson(jsonResponse,httpResponse);
            JsonObject root = jsonResponse.as<JsonObject>();
            if (root.containsKey("results") && root["results"][0].containsKey("value")) {
              value = root["results"][0]["value"];
              Serial.print("Motor State: ");
              Serial.print(value);
              Serial.print("\r\n");
              motorState = value;
            }
        }
      }
    }
    client.stop();
    Serial.println("\nDisconnected from server");
    return value;
  } else {
    Serial.println("Connection to Ubidots failed");
  }
}

void setup(void)
{

    Serial.begin(115200);
    while (!Serial);

    Serial2.begin(9600);
    while (!Serial2);
    
    Serial.print("LoRa P2P communication\r\n");

    while(!is_exist){
      if (at_send_check_response("+AT: OK", 100, "AT\r\n"))
      {
          is_exist = true;
          at_send_check_response("+MODE: TEST", 1000, "AT+MODE=TEST\r\n");
          at_send_check_response("+TEST: RFCFG", 1000, "AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF,OFF\r\n");
          delay(200);
      }
      else
      {
          is_exist = false;
          Serial.print("No E5 module found.\r\n");
      }
    }
}

void loop(void)
{
    node_send_then_recv(2000);
}
