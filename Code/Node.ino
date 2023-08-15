#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <SensirionI2CSht4x.h>
#include <Wire.h>
#include "HUSKYLENS.h"
#include "SoftwareSerial.h"

HUSKYLENS huskylens;
SoftwareSerial mySerial(1, 2); // RX, TX

int relayPin = 9;

SensirionI2CSht4x sht4x;

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
  Serial1.printf(p_cmd, args);
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
    while (Serial1.available() > 0)
    {
      ch = Serial1.read();
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
  while (Serial1.available() > 0)
  {
    ch = Serial1.read();
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

    p_start = strstr(recv_buf, "+TEST: RX \"05345454544");
    if (p_start)
    {
      p_start = strstr(recv_buf, "05345454544");
      if (p_start && (1 == sscanf(p_start, "05345454544%s", data)))
      {
        data[1] = 0;
        Serial.print("Motor State:");
        Serial.print(data);
        if (data[0]=='1'){
          digitalWrite(relayPin, HIGH);
        }
        else{
          digitalWrite(relayPin, LOW);
        }
        Serial.print("\r\n");
      }

      p_start = strstr(recv_buf, "RSSI:");
      if (p_start && (1 == sscanf(p_start, "RSSI:%d,", &rssi)))
      {
        Serial.print("rssi:");
        Serial.print(rssi);
      }
      p_start = strstr(recv_buf, "SNR:");
      if (p_start && (1 == sscanf(p_start, "SNR:%d", &snr)))
      {

        Serial.print("snr :");
        Serial.print(snr);
      }
      return 1;
    }
  }
  return 0;
}

void findAndReplace(char *str, char findChar, char replaceChar) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == findChar) {
            str[i] = replaceChar;
        }
    }
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
   //Lora
    static uint16_t count = 0;
    int ret = 0;
    char data[64];
    char cmd[256];
   
    // SHT-40
    uint16_t error;
    char errorMessage[256];
    delay(10);

    float temperature;
    float humidity;
    float moisture;
    
    error = sht4x.measureHighPrecision(temperature, humidity);
    if (error) 
    {
        Serial.print("Error trying to execute measureHighPrecision(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else 
    {
        Serial.print("Temperature:");
        Serial.print(temperature);
        Serial.print("\t");
        Serial.print("Humidity:");
        Serial.println(humidity);
    }
    
    //Moisture Sensor
    int sensorPin = A0;
    int analogValue = analogRead(sensorPin);
    moisture = map(analogValue,0,600,0.0,100.0);
    Serial.print("Moisture = " );
    Serial.println(moisture);

    int huskyStatus = husky();
    //delay(1000);
    Serial.print("HUSKY: ");
    Serial.print(huskyStatus);
    Serial.print("\r\n");
    char humStr[10];
    char temStr[10];
    char moiStr[10];
    char husStr[10];
    
    snprintf(humStr, sizeof(humStr), "%05.2f", humidity);
    snprintf(temStr, sizeof(temStr), "%05.2f", temperature);
    snprintf(moiStr, sizeof(moiStr), "%05.2f", moisture);

    findAndReplace(humStr, '.', 'A');
    findAndReplace(temStr, '.', 'A');
    findAndReplace(moiStr, '.', 'A');
    
    //Sending data to the Gateway
    memset(data, 0, sizeof(data));
    sprintf(data, "%s%s%s%d", temStr, humStr, moiStr,huskyStatus);
    sprintf(cmd, "AT+TEST=TXLRPKT,\"5345454544%s\"\r\n", data);
    Serial.println(data);
    Serial.println(cmd);
    

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

int husky()
{
    if (!huskylens.request()) Serial.println(F("Fail to request data from HUSKYLENS, recheck the connection!"));
    else if(!huskylens.available()) return 0;
    else
    {
        while (huskylens.available())
        {
            HUSKYLENSResult result = huskylens.read();
            Serial.print("ID :");
            Serial.print(result.ID);
            Serial.println();
            if (result.ID ==1 || result.ID ==2 || result.ID == 3  )
            {
              Serial.println("Elephant detected");
              return 2;
            }
            else if(result.ID ==4 || result.ID == 5 || result.ID == 6)
            {
              Serial.println("Boar detected");
              return 1;
            }
            else
            {
              Serial.println("Nothing detected");
              return 0;
            }
        }    
    }
}

void setup()
{   
    pinMode(relayPin, OUTPUT);
    Serial.begin(115200);
    mySerial.begin(9600);
    // Configure Serial1 on pins TX=6 and RX=7 (-1, -1 means use the default)
    Serial1.begin(9600);
    while (!huskylens.begin(mySerial))
    {
        Serial.println(F("Begin failed!"));
        Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>Serial 9600)"));
        Serial.println(F("2.Please recheck the connection."));
        delay(100);
    }
    
    // Settung up the SHT-40 sensor
    
    while (!Serial) 
    {
          delay(100);
    }

    Wire.begin();

    uint16_t error;
    char errorMessage[256];

    sht4x.begin(Wire);
    

    uint32_t serialNumber;
    error = sht4x.serialNumber(serialNumber);
    if (error) 
    {
        Serial.print("Error trying to execute serialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } 
    else 
    {
        Serial.print("Serial Number: ");
        Serial.println(serialNumber);
    }


   // Checking Lora Module
   Serial.print("LoRa P2P Communication!\r\n");

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

static void node_send_then_recv(uint32_t timeout)
{   
    int ret = 0;
    ret = node_send();
    if (!ret)
    {
        Serial.print("\r\n");
        return;
    }
    
    int ret2 = 0;
    ret2 = node_recv(timeout);
    delay(10);
    if (!ret2)
    {
        Serial.print("\r\n");
        return;
    }
    
}

void loop()
{
  node_send_then_recv(2000);
}
