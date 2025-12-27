#include <Arduino.h>
#include "nxjson.h"    

#define BUFFER_SIZE 250

char uartBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool checkCommand = false;

uint32_t default_UPPER_Open = 10000;
uint32_t default_UPPER_Close = 6300;
uint32_t default_Lower_Open = 1200;
uint32_t default_Lower_Close = 723;

const int B_1A_Pin = 19;
const int UPPERMOTOR_IN1_Pin = 12;
const int UPPERMOTOR_IN2_Pin = 14;
const int LOWERMOTOR_IN3_Pin = 26;
const int LOWERMOTOR_IN4_Pin = 27;

const int UPPERDOOR_OPEN_SW = 13;
const int UPPERDOOR_CLOSE_SW = 32;
const int LOWERDOOR_OPEN_SW = 35;
const int DETECT_SENSOR_IN1 = 25;
const int DETECT_SENSOR_IN2 = 36;


bool UPPERDoor_Open()
{
  return digitalRead(UPPERDOOR_OPEN_SW) == HIGH;
}

bool UPPERDoor_Close()
{
  return digitalRead(UPPERDOOR_CLOSE_SW) == LOW;
}

bool LOWERDoor_Open()
{
  return digitalRead(LOWERDOOR_OPEN_SW) == HIGH;
}

bool oliveDetect()
{
  return (digitalRead(DETECT_SENSOR_IN1) == LOW || digitalRead(DETECT_SENSOR_IN2) == LOW);
}

bool InsertOlive_Motor(void)
{
  digitalWrite(B_1A_Pin, HIGH);
  uint32_t start = millis();
  uint32_t defaultTime = 5000;
  bool sensor_answer = false;

  while (millis() - start < defaultTime)
  {
    if (oliveDetect())
    {
      sensor_answer = true;
      break;
    }
  }
  digitalWrite(B_1A_Pin, LOW);
  return sensor_answer;
}

void MotorControl(uint8_t activeDirection, uint8_t passiveDirection, uint8_t sensorPin, int sensorStatus, uint32_t maxTime, const char* name )
{
  digitalWrite(activeDirection, HIGH);
  digitalWrite(passiveDirection, LOW);
  uint32_t start = millis();
  bool status = false;

  while (millis() - start < maxTime)
  {
    if (digitalRead(sensorPin) == sensorStatus)
    {
      status = true;
      break;
    }
  }
  digitalWrite(activeDirection, LOW);
  digitalWrite(passiveDirection, LOW);
  if (!status)
  {
    Serial.printf("WARNING: %s timeout\n", name);
  }
}

void processJsonCommand(char *jsonString)
{
  char reply[120];
  const nx_json *root = nx_json_parse(jsonString, 0);
  if (!root) 
  {
    Serial.println("Parse error!");
    return;
  }
  
  if (strstr(jsonString, "INSERT_OLIVE") != NULL)
  {
    const nx_json *insertOlive = nx_json_get(root, "INSERT_OLIVE");
    if (insertOlive && insertOlive->type == NX_JSON_STRING)
    {
      bool sensorStatus = InsertOlive_Motor();
      if (sensorStatus)
      {
        sprintf(reply, "{\"INSERT_OLIVE\":\"OK\"}\r\n");
      }
      else
      {
        sprintf(reply, "{\"INSERT_OLIVE\":\"NOT_OK\"}\r\n");
      }

      Serial.print(reply);
    }
  }
  else if (strstr(jsonString, "UPPER_DOOR") != NULL)
  {
    const nx_json *upperdoor = nx_json_get(root, "UPPER_DOOR");
    if (upperdoor && upperdoor->type == NX_JSON_STRING)
    {
      bool doorStatus = false; 
      if (strcmp(upperdoor->text_value, "OPEN") == 0)
      {
        MotorControl(UPPERMOTOR_IN1_Pin, UPPERMOTOR_IN2_Pin, UPPERDOOR_OPEN_SW, HIGH, default_UPPER_Open, "UpperDoor Open");
        doorStatus = UPPERDoor_Open();
        sprintf(reply, "{\"UPPER_DOOR\":\"%s\"}\r\n", doorStatus ? "OPEN" : "NOT_OK");
        Serial.print(reply);
      }
      else if (strcmp(upperdoor->text_value, "CLOSE") == 0)
      {
        MotorControl(UPPERMOTOR_IN2_Pin, UPPERMOTOR_IN1_Pin, UPPERDOOR_CLOSE_SW, LOW, default_UPPER_Close, "UpperDoor Close");
        doorStatus = UPPERDoor_Close();
        sprintf(reply, "{\"UPPER_DOOR\":\"%s\"}\r\n", doorStatus ? "CLOSE" : "NOT_OK");
        Serial.print(reply);
      }
    }
  }
  else if (strstr(jsonString, "LOWER_DOOR") != NULL)
  {
    const nx_json *lowerdoor = nx_json_get(root, "LOWER_DOOR");
    bool doorStatus = false;
    if (lowerdoor && lowerdoor->type == NX_JSON_STRING)
    {
      if (strcmp(lowerdoor->text_value, "OPEN") == 0)
      {
        MotorControl(LOWERMOTOR_IN3_Pin, LOWERMOTOR_IN4_Pin, LOWERDOOR_OPEN_SW,HIGH, default_Lower_Open, "LowerDoor Open");
        doorStatus = LOWERDoor_Open();
        sprintf(reply, "{\"LOWER_DOOR\":\"%s\"}\r\n", doorStatus ? "OPEN" : "NOT_OK");
        Serial.print(reply);
      }
      else if (strcmp(lowerdoor->text_value, "CLOSE") == 0)
      {
        digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
        digitalWrite(LOWERMOTOR_IN4_Pin, HIGH);          
        delay(default_Lower_Close);
        digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
        digitalWrite(LOWERMOTOR_IN4_Pin, LOW);
        doorStatus = true; 
        sprintf(reply, "{\"LOWER_DOOR\":\"%s\"}\r\n", doorStatus ? "CLOSE" : "NOT_OK");
        Serial.print(reply);
      }
    }
  }
  nx_json_free(root);
}


void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("ESP32 TEST\n");

  pinMode(B_1A_Pin, OUTPUT);
  pinMode(UPPERMOTOR_IN1_Pin, OUTPUT);
  pinMode(UPPERMOTOR_IN2_Pin, OUTPUT);
  pinMode(LOWERMOTOR_IN3_Pin, OUTPUT);
  pinMode(LOWERMOTOR_IN4_Pin, OUTPUT);
  pinMode(UPPERDOOR_OPEN_SW, INPUT);
  pinMode(UPPERDOOR_CLOSE_SW, INPUT);
  pinMode(LOWERDOOR_OPEN_SW, INPUT);
  pinMode(DETECT_SENSOR_IN1, INPUT);
  pinMode(DETECT_SENSOR_IN2, INPUT);

  digitalWrite(UPPERMOTOR_IN1_Pin, LOW);
  digitalWrite(UPPERMOTOR_IN2_Pin, LOW);
  digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
  digitalWrite(LOWERMOTOR_IN4_Pin, LOW);

}


void loop()
{
  if (Serial.available())
  {
    char c = Serial.read();
    uartBuffer[bufferIndex++] = c;
    if (bufferIndex >= 2)
    {
      if (uartBuffer[bufferIndex - 2] == 13 &&
          uartBuffer[bufferIndex - 1] == 10)
      {
        uartBuffer[bufferIndex - 2] = '\0';
        checkCommand = true;
      }
    }
    if (bufferIndex >= BUFFER_SIZE - 1)
    {
      memset(uartBuffer, 0, BUFFER_SIZE);
      bufferIndex = 0;
    }
  }
  if (checkCommand)
  {
    processJsonCommand(uartBuffer);
    memset(uartBuffer, 0, BUFFER_SIZE);
    bufferIndex = 0;
    checkCommand = false;
  }
}
