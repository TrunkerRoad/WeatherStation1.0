// == revision history
// 20150313 update to command server command format 
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <Wire.h>
#include <math.h>
#include <LDateTime.h>
#include <HttpClient.h>
#include "\Adafruit_Sensor\Adafruit_Sensor.h"
#include "\Adafruit_BMP085_Unified\Adafruit_BMP085_U.h"
#include "Grove_LCD_RGB_Backlight\rgb_lcd.h"
#include <LFlash.h>

// ================================================
// Define
#undef _DBG_
//#define BMP085_ADDRESS 0x77  // I2C address of BMP085
#define sht21address B1000000
#define sht21temp  B11110011
#define sht21humid B11110101
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SITE_URL "api.mediatek.com"
#define per 5 // time interval in seconds to upload sensor data to MCS
#define per1 50 // time interval in seconds to initiate TCP heart beat
#define Drv LFlash
#define FAN_PIN 0

// ================================================
// Global variable

rgb_lcd lcd;
const unsigned char OSS = 0;  // Oversampling Setting
// Calibration values
int16_t ac1;
int16_t ac2;
int16_t ac3;
uint16_t ac4;
uint16_t ac5;
uint16_t ac6;
int16_t b1;
int16_t b2;
int16_t mb;
int16_t mc;
int16_t md;
// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5;
unsigned int rtc;
unsigned int lrtc;
unsigned int lrtc2;
unsigned int rtc2;
unsigned int rtc1;
unsigned int lrtc1;
char ssid[20];
char key[20];
char deviceid[20];
char devicekey[20];
LWiFiClient c;
LWiFiClient c2;
char port[5]="    ";
char connection_info[21]="                    ";
char ip[15]="              ";
int portnum;
int val = 0;
String tcpdata;
String tcpcmd_led_on;
String tcpcmd_led_off;
String upload_led;
HttpClient http(c2);
int state = 0;
int i = 0;


void fatal_error(const char* str)
{
  lcd.setRGB(255, 0, 0);
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print(str);
  while(1)
    delay(100);
}

// ==============================================================
// read file for Wifi and Device information

void readDeviceSetting()
{
  Drv.begin();

  LFile f = Drv.open("device.txt", FILE_READ);
  if(!f)
    fatal_error("Fail to open device.txt");

  char buf[100];
  int len = f.size();

  f.read(buf, len);
  f.close();
  buf[len] = '\0';

  int i = 0, j = 0;

  while(buf[i] && buf[i] != ',')
  {
    deviceid[j++]=buf[i++];
  }
  deviceid[j] = 0;
  if(buf[i] != ',')
    fatal_error("Fail to parse wifi.txt");
  i++;
  j=0;
  while(buf[i])
  {
    devicekey[j++]=buf[i++];
  }
  devicekey[j]=0;
}

void readWiFiSetting()
{
  Drv.begin();

  LFile f = Drv.open("wifi.txt", FILE_READ);
  if(!f)
    fatal_error("Fail to open wifi.txt");

  char buf[100];
  int len = f.size();

  f.read(buf, len);
  f.close();
  buf[len] = '\0';

  int i = 0, j = 0;

  while(buf[i] && buf[i] != ',')
  {
    ssid[j++]=buf[i++];
  }
  ssid[j] = 0;
  if(buf[i] != ',')
    fatal_error("Fail to parse wifi.txt");
  i++;
  j=0;
  while(buf[i])
  {
    key[j++]=buf[i++];
  }
  key[j]=0;
}

// ==============================================================
// Call MCS API to get TCP Socket connection information

void getconnectInfo(){
  //calling RESTful API to get TCP socket connection
  c2.print("GET /mcs/v2/devices/");
  c2.print(deviceid);
  c2.println("/connections.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(devicekey);
  c2.println("Connection: close");
  c2.println();
  
  delay(500);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.println("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  char c;
  int ipcount = 0;
  int count = 0;
  int separater = 0;
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      c = v;
      Serial.print(c);
      connection_info[ipcount]=c;
      if(c==',')
      separater=ipcount;
      ipcount++;    
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();

    }
    
  }
  Serial.print("The connection info: ");
  Serial.println(connection_info);
  int i;
  for(i=0;i<separater;i++)
  {  ip[i]=connection_info[i];
  }
  int j=0;
  separater++;
  for(i=separater;i<21 && j<4;i++)
  {  port[j]=connection_info[i];
     j++;
  }
  Serial.println("The TCP Socket connection instructions:");
  Serial.print("IP: ");
  Serial.println(ip);
  Serial.print("Port: ");
  Serial.println(port);
  portnum = atoi (port);

} //getconnectInfo

// ==============================================================
// TCP Socket server connection functions

void connectTCP(){
  //establish TCP connection with TCP Server with designate IP and Port
  c.stop();
  Serial.println("Connecting to TCP");
  while (0 == c.connect(ip, portnum))
  {
    Serial.println("Re-Connecting to TCP");    
    delay(1000);
  }  
  Serial.println("send TCP connect");
  c.println(tcpdata);
  c.println();
  Serial.println("waiting TCP response:");
  lcd.clear();
  lcd.print("Connected..");
} //connectTCP

void heartBeat(){
  Serial.println("send TCP heartBeat");
  c.println(tcpdata);
  c.println();
    
} //heartBeat

// ==============================================================
// MCS API call functions

void senddata() {
  Serial.println("Begin of senddata");
  float temp = readsht21temp();
  float hum = readsht21humidity();
  float temperature = bmp085GetTemperature(bmp085ReadUT()); //MUST be called first

  float pressure = 0;
  while (pressure < 90000) {
    bmp085GetTemperature(bmp085ReadUT());
    pressure  = bmp085GetPressure(bmp085ReadUP());
  }
  Serial.println("1");
  char buffert[5];
  char bufferh[5];
  char bufferp[10];
  sprintf(buffert, "%.2f", temp);
  sprintf(bufferh, "%.2f", hum);
  sprintf(bufferp, "%6.1f", pressure);
  Serial.println("2");
  // display status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(buffert);
  lcd.setCursor(8, 0);
  lcd.print("H:");
  lcd.print(bufferh);
  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print(bufferp);


  String data = "temperature,," + String(buffert) + "\nhumidity,," + String(bufferh) + "\npressure,," + String(bufferp);
  Serial.println(data);

  //   int light = analogRead(A0);
  //   String data = "1000000013,,"+String(light);
  int thisLength = data.length();
  //Serial.println(data);
  LWiFiClient c2;
  unsigned long t1 = millis();
  while (0 == c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }

  delay(500);
  Serial.println("send POST request");

  c2.print("POST /mcs/v2/devices/");
  c2.print(deviceid);
  c2.println("/datapoints.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(devicekey);
  c2.print("Content-Length: ");
  c2.println(thisLength);
  c2.println("Content-Type: text/csv");
  c2.println("Connection: close");
  c2.println();
  c2.println(data);

  String dataget = "";
  Serial.println("waiting HTTP response:");

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.println("waiting HTTP response:");
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      Serial.print(char(v));

      delay(1);
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Data uploaded");
  lcd.setCursor(0, 1);
  lcd.print("to MCS");
  delay(300);
  unsigned long t2 = millis();
}

// ==============================================================
// Sensor functions

float readsht21temp() {
  Wire.flush();
  Wire.beginTransmission(sht21address);
  Wire.write(sht21temp);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(sht21address, 3);

  byte msb = Wire.read();
  byte lsb = Wire.read();
  int t = (msb << 8) + lsb;
  int crc = Wire.read();

  float temperature = -46.85 + 175.72 * t / 65536.0 ;
  return temperature;


}
float readsht21humidity() {
  Wire.flush();
  Wire.beginTransmission(sht21address);
  Wire.write(sht21humid);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(sht21address, 3);

  byte msb = Wire.read();
  byte lsb = Wire.read();
  int h = (msb << 8) + lsb;
  int crc = Wire.read();

  float humidity = -6 + 125 * h / 65536.0 ;
  return humidity;
}


void bmp085Calibration()
{
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);



}

// Calculate temperature in deg C
float bmp085GetTemperature(unsigned int ut) {
  long x1, x2;

  x1 = (((long)ut - (long)ac6) * (long)ac5) >> 15;
  long a = (long)mc << 11;
  long b = x1 + md;
  x2 = a / b;
  b5 = x1 + x2;
  float temp = ((b5 + 8) >> 4);
  temp = temp / 10;

  return temp;
}

// Calculate pressure given up
// calibration values must be known
// b5 is also required so bmp085GetTemperature(...) must be called first.
// Value returned will be pressure in units of Pa.
long bmp085GetPressure(unsigned long up) {
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;

  b6 = b5 - 4000;
  // Calculate B3
  x1 = (b2 * (b6 * b6) >> 12) >> 11;
  x2 = (ac2 * b6) >> 11;
  x3 = x1 + x2;
  b3 = (((((long)ac1) * 4 + x3) << OSS) + 2) >> 2;

  // Calculate B4
  x1 = (ac3 * b6) >> 13;
  x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  b4 = (ac4 * (unsigned long)(x3 + 32768)) >> 15;

  b7 = ((unsigned long)(up - b3) * (50000 >> OSS));
  if (b7 < 0x80000000)
    p = (b7 << 1) / b4;
  else
    p = (b7 / b4) << 1;

  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  p += (x1 + x2 + 3791) >> 4;

  long temp = p;
  return temp;
}

// Read 1 byte from the BMP085 at 'address'
char bmp085Read(unsigned char address)
{
  unsigned char data;

  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(BMP085_ADDRESS, 1);
  while (!Wire.available());

  return Wire.read();
}

// Read 2 bytes from the BMP085
// First byte will be from 'address'
// Second byte will be from 'address'+1
int bmp085ReadInt(unsigned char address)
{
  unsigned char msb, lsb;

  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(BMP085_ADDRESS, 2);
  while (Wire.available() < 2)
    ;
  msb = Wire.read();
  lsb = Wire.read();

  return (int) msb << 8 | lsb;
}

// Read the uncompensated temperature value
unsigned int bmp085ReadUT() {
  unsigned int ut;

  // Write 0x2E into Register 0xF4
  // This requests a temperature reading
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();

  // Wait at least 4.5ms
  delay(5);

  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);

  return ut;
}

// Read the uncompensated pressure value
unsigned long bmp085ReadUP() {

  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;

  // Write 0x34+(OSS<<6) into register 0xF4
  // Request a pressure reading w/ oversampling setting
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (OSS << 6));
  Wire.endTransmission();

  // Wait for conversion, delay time dependent on OSS
  delay(2 + (3 << OSS));

  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  msb = bmp085Read(0xF6);
  lsb = bmp085Read(0xF7);
  xlsb = bmp085Read(0xF8);

  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8 - OSS);

  return up;
}

void writeRegister(int deviceAddress, byte address, byte val) {
  Wire.beginTransmission(deviceAddress); // start transmission to device
  Wire.write(address);       // send register address
  Wire.write(val);         // send value to write
  Wire.endTransmission();     // end transmission
}

int readRegister(int deviceAddress, byte address) {

  int v;
  Wire.beginTransmission(deviceAddress);
  Wire.write(address); // register to read
  Wire.endTransmission();

  Wire.requestFrom(deviceAddress, 1); // read a byte

  while (!Wire.available()) {
    // waiting
  }

  v = Wire.read();
  return v;
}

float calcAltitude(float pressure) {

  float A = pressure / 101325;
  float B = 1 / 5.25588;
  float C = pow(A, B);
  C = 1 - C;
  C = C / 0.0000225577;

  return C;
}

// ==============================================================
// Main program
void setup() {
  Wire.begin();
  
  //LCD setup
  lcd.begin(16, 2);
  lcd.setRGB(0, 125, 0);
  
  //Sensor setup
  bmp085Calibration();
  
  //Wifi & Device info Setup
  readWiFiSetting();
  readDeviceSetting();
  LTask.begin();
  LWiFi.begin();
  Serial.begin(115200);
  //while(!Serial) delay(1000); //mark this demo as standalone
  Serial.println("Connecting to AP");
  Serial.println(deviceid);
  Serial.println(devicekey);
  while (0 == LWiFi.connect(ssid, LWiFiLoginInfo(WIFI_AUTH, key))){
    delay(1000);
  }
  lcd.clear();
  lcd.print("AP Connected");
  delay(100);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  Serial.print("LED is set to LOW");
  LDateTime.getRtc(&lrtc);
  LDateTime.getRtc(&lrtc2);
  while (!c2.connect(SITE_URL, 80)){
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);
  tcpdata = String(deviceid) + "," + String(devicekey) + ",0";
  tcpcmd_led_on = String(deviceid) + "," + String(devicekey) + ",0,fan_control,on";
  tcpcmd_led_off = String(deviceid) + "," + String(devicekey) + ",0,fan_control,off";
  getconnectInfo();
  connectTCP();
}


void loop() {
  if (LWiFi.status() == LWIFI_STATUS_DISCONNECTED) {
    Serial.println("Disconnected from AP");
    while (0 == LWiFi.connect(ssid, LWiFiLoginInfo(WIFI_AUTH, key))){
      Serial.println("Connecting to AP");
      delay(1000);
    }

  }


  LDateTime.getRtc(&rtc);
  if ((rtc - lrtc) >= per) {
    Serial.println("Senddata");
     senddata();
    lrtc = rtc;
  }

  //Check for TCP socket command from MCS Server 
  String tcpcmd="";
  while (c.available())
   {
      int v = c.read();
      if (v != -1)
      {
        Serial.print((char)v);
        tcpcmd += (char)v;
        if (tcpcmd.substring(52).equals("1")){
          digitalWrite(FAN_PIN, HIGH);
          lcd.clear();
          lcd.print("Switch FAN ON ");
          tcpcmd="";
        }else if(tcpcmd.substring(52).equals("0")){  
          digitalWrite(FAN_PIN, LOW);
          lcd.clear();
          lcd.print("Switch FAN OFF");
          tcpcmd="";
        }
      }
   }
  //Check for hearbeat interval 
  LDateTime.getRtc(&rtc2);
  if ((rtc2 - lrtc2) >= per1) {
    heartBeat();
    lrtc2 = rtc2;
  }  
}
