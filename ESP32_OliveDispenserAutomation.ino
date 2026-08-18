#include <Arduino.h>
#include "nxjson.h" 
#include <Wire.h>            
// #include "Adafruit_SHT31.h"
  

#define BUFFER_SIZE 560
#define CSI_BUFFER_SIZE 8192
#define MAX_CSI_PACKETS 61
HardwareSerial SerialESP(2); 

uint8_t csiPacketCount = 0;
bool csiCaptureActive = false; 

char uartBuffer[BUFFER_SIZE];
uint16_t bufferIndex = 0;
bool checkCommand = false;

char csiBuffer[CSI_BUFFER_SIZE];
uint16_t csiIndex = 0;

uint32_t default_UPPER_Open = 10000;
uint32_t default_UPPER_Close = 6300;
uint32_t default_Lower_Open = 10000;
uint32_t default_Lower_Close = 10000;

const int Power_control_Pin = 15;

const int B_1A_Pin = 19;
const int UPPERMOTOR_IN1_Pin = 12; 
const int UPPERMOTOR_IN2_Pin = 14;
const int LOWERMOTOR_IN3_Pin = 26;
const int LOWERMOTOR_IN4_Pin = 27;

const int UPPERDOOR_OPEN_SW = 13;
const int UPPERDOOR_CLOSE_SW = 32;
const int LOWERDOOR_OPEN_SW = 35;
const int LOWERDOOR_CLOSE_SW = 18;
const int DETECT_SENSOR_IN1 = 25;
const int DETECT_SENSOR_IN2 = 36;

const int SCL_PIN = 22;
const int SDA_PIN = 21;
const uint8_t SHT31_ADDR = 0x44;


bool readSHT31_Raw(float &temperature)
{
  Wire.beginTransmission(SHT31_ADDR);
  Wire.write(0x24);
  Wire.write(0x00);  
  uint8_t writeResult = Wire.endTransmission();
  if (writeResult != 0)
  {
    Serial.printf("readSHT31_Raw: Write failed, code=%d\n", writeResult);
    return false;
  }
  delay(20); 
  uint8_t n = Wire.requestFrom((uint8_t)SHT31_ADDR, (uint8_t)6);
  if (n != 6)
  {
    Serial.printf("readSHT31_Raw: RequestFrom failed, got %d bytes\n", n);
    return false;
  }
  uint8_t data[6];
  for (int i = 0; i < 6; i++)
  {
    data[i] = Wire.read();
  }
  uint16_t rawTemp = (data[0] << 8) | data[1];
  temperature = -45.0 + 175.0 * ((float)rawTemp / 65535.0);
  return true;
}

void I2C_BusRecovery()
{
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT);

  for (int i = 0; i < 9; i++)
  {
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(10);
  }
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(10);

  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(SDA_PIN, HIGH);
  delayMicroseconds(10);

  Wire.begin(SDA_PIN, SCL_PIN);
}

void ReadandSend_TempValue() 
{
  float temp;
  bool success = readSHT31_Raw(temp);
  if (!success) 
  {
    Serial.println("Could nıt read, next try...");
    I2C_BusRecovery();
    delay(50);
    success = readSHT31_Raw(temp);
  }
  if (success) 
  {
    Serial.printf("{\"TEMP\":%.2f}\r\n", temp);
  } 
  else 
  {
    Serial.println("{\"TEMP\":\"ERROR\"}\r\n");
  }
}

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

bool LOWERDoor_Close()
{
  return digitalRead(LOWERDOOR_CLOSE_SW) == LOW;
}

bool oliveDetect()
{
  return (digitalRead(DETECT_SENSOR_IN1) == LOW || digitalRead(DETECT_SENSOR_IN2) == LOW);
}

bool InsertOlive_Motor(void)
{
  digitalWrite(B_1A_Pin, HIGH);
  // Serial.println("B_1A HIGH");
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
  delay(300);
  return sensor_answer;
}

bool MotorControl(uint8_t activeDirection, uint8_t passiveDirection, uint8_t sensorPin, int sensorStatus, uint32_t maxTime, const char* name )
{
  digitalWrite(activeDirection, HIGH);
  digitalWrite(passiveDirection, LOW);
  uint32_t start = millis();
  bool status = false;

  while (millis() - start < maxTime)
  {
    if (digitalRead(sensorPin) == sensorStatus)
    {
      digitalWrite(activeDirection, LOW);
      digitalWrite(passiveDirection, LOW);
      delay(300);
      if (digitalRead(sensorPin) == sensorStatus)
      {
        status = true;
        break;
      }
      else
      {
        //Serial.printf("%s false trigger, resuming...\n", name);
        digitalWrite(activeDirection, HIGH);
        digitalWrite(passiveDirection, LOW);
      }
    }
  }
  digitalWrite(activeDirection, LOW);
  digitalWrite(passiveDirection, LOW);
  if (!status)
  {
    Serial.printf("WARNING: %s timeout\n", name);
  }
  return status;
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
  
  // if (strstr(jsonString, "INSERT_OLIVE") != NULL)
  // {
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
  // }
  // else if (strstr(jsonString, "UPPER_DOOR") != NULL)
  // {
  const nx_json *upperdoor = nx_json_get(root, "UPPER_DOOR");
  if (upperdoor && upperdoor->type == NX_JSON_STRING)
  {
    bool doorStatus = false; 
    if (strcmp(upperdoor->text_value, "OPEN") == 0)
    {
      doorStatus = MotorControl(UPPERMOTOR_IN1_Pin, UPPERMOTOR_IN2_Pin, UPPERDOOR_OPEN_SW, HIGH, default_UPPER_Open, "UpperDoor Open");
      sprintf(reply, "{\"UPPER_DOOR\":\"%s\"}\r\n", doorStatus ? "OPEN" : "NOT_OK");
      Serial.print(reply);
    }
    else if (strcmp(upperdoor->text_value, "CLOSE") == 0)
    {
      doorStatus = MotorControl(UPPERMOTOR_IN2_Pin, UPPERMOTOR_IN1_Pin, UPPERDOOR_CLOSE_SW, LOW, default_UPPER_Close, "UpperDoor Close");
      sprintf(reply, "{\"UPPER_DOOR\":\"%s\"}\r\n", doorStatus ? "CLOSE" : "NOT_OK");
      Serial.print(reply);
    }
  }
  // }
  // else if (strstr(jsonString, "LOWER_DOOR") != NULL)
  // {
  const nx_json *lowerdoor = nx_json_get(root, "LOWER_DOOR");
  bool doorStatus = false;
  if (lowerdoor && lowerdoor->type == NX_JSON_STRING)
  {
    if (strcmp(lowerdoor->text_value, "OPEN") == 0)
    {
      doorStatus = MotorControl(LOWERMOTOR_IN3_Pin, LOWERMOTOR_IN4_Pin, LOWERDOOR_OPEN_SW,HIGH, default_Lower_Open, "LowerDoor Open");
      sprintf(reply, "{\"LOWER_DOOR\":\"%s\"}\r\n", doorStatus ? "OPEN" : "NOT_OK");
      Serial.print(reply);
    }
    else if (strcmp(lowerdoor->text_value, "CLOSE") == 0)
    {
      // digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
      // digitalWrite(LOWERMOTOR_IN4_Pin, HIGH);          
      // delay(default_Lower_Close);
      // digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
      // digitalWrite(LOWERMOTOR_IN4_Pin, LOW);
      // doorStatus = true; 
      doorStatus = MotorControl(LOWERMOTOR_IN4_Pin, LOWERMOTOR_IN3_Pin, LOWERDOOR_CLOSE_SW,LOW, default_Lower_Close, "LowerDoor Close");
      sprintf(reply, "{\"LOWER_DOOR\":\"%s\"}\r\n", doorStatus ? "CLOSE" : "NOT_OK");
      Serial.print(reply);
    }
  }
  // }
  // else if (strstr(jsonString, "CSI_CAPTURE") != NULL)
  // {
  const nx_json *csiCmd = nx_json_get(root, "CSI_CAPTURE");
  if (csiCmd && csiCmd->type == NX_JSON_STRING)
  {
    if (strcmp(csiCmd->text_value, "START") == 0)
    {
      csiCaptureActive = true;
      csiPacketCount = 0;
      Serial.println("{\"CSI_CAPTURE\":\"STARTED\"}");
    }
  }
  // }
  // else if (strstr(jsonString, "Power") != NULL)
  // {
  const nx_json *pwCmd = nx_json_get(root, "Power");
  if (pwCmd && pwCmd->type == NX_JSON_STRING)
  {
    if (strcmp(pwCmd->text_value, "CLOSE") == 0)
    {
      digitalWrite(Power_control_Pin, LOW);
      Serial.println("{\"Power\":\"OFF\"}");
    }
    else if (strcmp(pwCmd->text_value, "OPEN") == 0)
    {
      digitalWrite(Power_control_Pin, HIGH);
      Serial.println("{\"Power\":\"ON\"}");
    }
  }
  // }
  const nx_json *cmdCmd = nx_json_get(root, "CMD");
  if (cmdCmd && cmdCmd->type == NX_JSON_STRING)
  {
      if (strcmp(cmdCmd->text_value, "NOTHING") == 0)
      {
          Serial.println("{\"CMD\":\"NOTHING\"}");
      }
  }
  const nx_json *tempCmd = nx_json_get(root, "TEMP");
  if (tempCmd && tempCmd->type == NX_JSON_STRING)
  {
    if (strcmp(tempCmd->text_value, "READ") == 0)
    {
      ReadandSend_TempValue();
    }
  }
  nx_json_free(root);
}

void parseAndSendCSI(char* rawData) 
{
  char* startBracket = strchr(rawData, '[');
  char* endBracket = strrchr(rawData, ']');

  if (startBracket == NULL || endBracket == NULL) 
  {
    return;
  }
  char rssiStr[8] = {0};
  {
    char* p = rawData;
    char* fieldStart = rawData;
    int fieldIndex = 0;

    while (*p && p < startBracket) 
    {
      if (*p == ',') 
      {
        if (fieldIndex == 3)   
        {
          int len = p - fieldStart;
          if (len > 0 && len < (int)sizeof(rssiStr)) 
          {
            memcpy(rssiStr, fieldStart, len);
            rssiStr[len] = '\0';
          }
          break;
        }
        fieldIndex++;
        fieldStart = p + 1;
      }
      p++;
    }
  }

  // if (startBracket != NULL && endBracket != NULL) 
  // {
    *(endBracket + 1) = '\0'; 
    // Serial.print("{\"CSI_Data\":\"");
    char* content = startBracket + 1;
    *(endBracket) = '\0'; 
  //   Serial.print(content); 
  //   Serial.println("\"}");
  // }

  Serial.print("\"RSSI\":");
  Serial.print(rssiStr[0] ? rssiStr : "null");
  Serial.print(",\"CSI_Data\":\"");
  Serial.print(content); 
  Serial.println("\"}");
}


void setup()
{
  Serial.begin(115200);//500000
  Serial.setRxBufferSize(1024);
  SerialESP.begin(115200, SERIAL_8N1, 16, 17);//500000, SERIAL_8N1, 16, 17); 

  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.beginTransmission(SHT31_ADDR);
  if (Wire.endTransmission() != 0)
  {
    Serial.println("{\"TEMP_SENSOR\":\"NOT_FOUND\"}\r\n");
  }

  pinMode(Power_control_Pin, OUTPUT);
  pinMode(B_1A_Pin, OUTPUT);
  pinMode(UPPERMOTOR_IN1_Pin, OUTPUT);
  pinMode(UPPERMOTOR_IN2_Pin, OUTPUT);
  pinMode(LOWERMOTOR_IN3_Pin, OUTPUT);
  pinMode(LOWERMOTOR_IN4_Pin, OUTPUT);
  pinMode(UPPERDOOR_OPEN_SW, INPUT);
  pinMode(UPPERDOOR_CLOSE_SW, INPUT);
  pinMode(LOWERDOOR_OPEN_SW, INPUT);
  pinMode(LOWERDOOR_CLOSE_SW, INPUT);
  pinMode(DETECT_SENSOR_IN1, INPUT);
  pinMode(DETECT_SENSOR_IN2, INPUT);

  digitalWrite(UPPERMOTOR_IN1_Pin, LOW);
  digitalWrite(UPPERMOTOR_IN2_Pin, LOW);
  digitalWrite(LOWERMOTOR_IN3_Pin, LOW);
  digitalWrite(LOWERMOTOR_IN4_Pin, LOW);
  digitalWrite(Power_control_Pin, HIGH);

}


void loop()
{
  while (SerialESP.available() && csiCaptureActive) 
  {
    char c = SerialESP.read();
    Serial.print(c);

    if (csiIndex < CSI_BUFFER_SIZE - 1) 
    {
      csiBuffer[csiIndex++] = c;
    }
    if (c == '\n') 
    {
      csiBuffer[csiIndex - 1] = '\0';
      parseAndSendCSI(csiBuffer);
      memset(csiBuffer, 0, CSI_BUFFER_SIZE);
      csiIndex = 0;

      csiPacketCount++;
      if (csiPacketCount >= MAX_CSI_PACKETS) 
      {
        csiCaptureActive = false;
        Serial.println("{\"CSI_CAPTURE\":\"DONE\"}");
      }
    }
  }
  // int csiReadCount = 0;
  // while (SerialESP.available() && csiCaptureActive && csiReadCount < 256) 
  // {
  //   char c = SerialESP.read();
  //   csiReadCount++;
  //   if (csiIndex < CSI_BUFFER_SIZE - 1) 
  //   {
  //     csiBuffer[csiIndex++] = c;
  //   }
  //   if (c == '\n') 
  //   {
  //     csiBuffer[csiIndex - 1] = '\0';
  //     parseAndSendCSI(csiBuffer);
  //     memset(csiBuffer, 0, CSI_BUFFER_SIZE);
  //     csiIndex = 0;

  //     csiPacketCount++;
  //     if (csiPacketCount >= MAX_CSI_PACKETS) 
  //     {
  //       csiCaptureActive = false;
  //       Serial.println("{\"CSI_CAPTURE\":\"DONE\"}");
  //     }
  //     break; 
  //   }
  // }

  while (Serial.available())
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
