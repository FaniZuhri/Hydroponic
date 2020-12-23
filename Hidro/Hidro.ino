/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/

#include "Arduino.h"
#include "Adafruit_ADS1015.h"
#include "DFRobot_ESP_EC.h"
#include <EEPROM.h>
#include <WiFi.h> // Include the Wi-Fi library
#include <PubSubClient.h>
#include "GravityTDS.h"
//#include "DFRobot_PH.h"
#include "DFRobot_ESP_PH_WITH_ADC.h"
#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <cString>
#include <SHT2x.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
BH1750 lightMeter(0x23);

// Update these with values suitable for your network.

//const char* ssid = "PT HAB Green House";
//const char* password = "pthab007";
const char *ssid = "monik";
const char *password = "*Hydr0p0n1c#";
//const char *ssid = "Mie Goyeng";
//const char *password = "digodogsek";
const char *mqtt_server = "192.168.1.111";
const char *mqtt_topic = "hidroHAB";
String sn = "2020110001", tanggal = "20165-165-165", waktu = "45:165:85", tanggal_ordered, waktu_ordered;
char c;

/************************* EC  and PH Initialization **************************/
DFRobot_ESP_EC ec;
DFRobot_ESP_PH_WITH_ADC ph;
Adafruit_ADS1115 ads;
float ecValue;
float voltage, voltage1, temperature, temp = 25;
/************************* End Initialization *************************/

/********************** JSN Initialization *****************************/
#include <NewPing.h>
#define trigPin 2
#define echoPin 4
// Define maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500 cm:
#define MAX_DISTANCE 400
// NewPing setup of pins and maximum distance.
NewPing sonar = NewPing(trigPin, echoPin, MAX_DISTANCE);
/**************************** End Initialization *******************************/

#define VREF 3.3 // analog reference voltage(Volt) of the ADC

SHT2x SHT2x;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (100)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to...");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

#define TdsSensorPin 32

// -------------------- PH Sensor initialization ----------------- //
//DFRobot_ESP_PH ph;
//#define ESPADC 4096.0   //the esp Analog Digital Convertion value
//#define ESPVOLTAGE 3300 //the esp voltage supply value
//#define PH_PIN 39    //the esp gpio data pin number
//float voltage, temperature=25;
// -------------------- End PH Initialization ------------------- //

#define SCOUNT 30         // sum of sample point
int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue, hum;

float pH_start, pH_end, pH_val, phValue = 0, reservoir_temp;
float vol;
int lux;
int i = 1, t, TDS_coef, dis_sum, firstLoop = 1, n, gy, a = 1, b = 1, d = 1, e = 1;

String from_nodeMCU, pHstatus, TDSstatus;
int pHrelaypin = 33, TDSrelaypin = 25, samplingrelay = 26, pomparelay = 33, h;

//Temperature chip i/o
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//rtc & lcd
#define SDA_PIN 21
#define SCL_PIN 22
#define DS3231_I2C_ADDRESS 0x68
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

//TIME DELAY
unsigned long upload_previous;
unsigned long currentTime;
unsigned long upload_period = 5000;
long int dataCount = 0;

//variabel untuk looping 30 detik
unsigned long startTime = 0;
unsigned long durasiRelay = 30000;
unsigned long durasiSampling = 8000;

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1')
  {
  }
  else
  {
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Connecting to MQTT..");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      lcd.clear();
      lcd.setCursor(0, 2);
      lcd.print("   MQTT Connected   ");
      delay(2000);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      lcd.clear();
      lcd.setCursor(0, 2);
      lcd.print("failed with state");
      lcd.print(client.state());
      lcd.setCursor(0, 3);
      lcd.print("try again in 3 seconds");
      delay(3000);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  lcd.init(); // initialize the lcd
  lcd.backlight();

  setup_wifi();
  client.setServer(mqtt_server, 1883); //IP raspi
  client.setCallback(callback);

  pinMode(pHrelaypin, OUTPUT);
  pinMode(TDSrelaypin, OUTPUT);
  pinMode(samplingrelay, OUTPUT);
  pinMode(pomparelay, OUTPUT);
  //turn off all relays
  relay(1, 1, 1);
  delay(1000);
  waktu1();
  pinMode(TdsSensorPin, INPUT);
  EEPROM.begin(32); //needed EEPROM.begin to store calibration k in eeprom
                    //  EEPROM.begin(34);
                    //  delay(250);
  ph.begin();
  ec.begin();
  //  delay(250);
  //by default lib store calibration k since 10 change it by set ec.begin(30); to start from 30
  ads.setGain(GAIN_ONE);
  ads.begin();

  sensors.begin(); //DS18B20 start

  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else
  {
    Serial.println(F("Error initialising BH1750"));
  }

  SHT2x.begin();
}

void loop()
{
  delay(500);
  if (!client.connected())
  {
    reconnect();
    lcd.clear();
    delay(1000);
    timestamp();
    displayLcd1();
  }
  client.loop();

  delay(1000);

  unsigned long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;
    //    ++value;
    Serial.println("Send data monitoring to MQTT...");
    snprintf(msg, MSG_BUFFER_SIZE, "%s,%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%i,%.1f", sn.c_str(), tanggal.c_str(), waktu.c_str(), temperature, reservoir_temp, phValue, ecValue, hum, lux, vol);
    Serial.print("Publish message: ");
    Serial.println(msg);
    Serial.println(" ");
    client.publish("hidroHAB", msg);
    delay(1000);
  }
  if (i == 1)
  {
    Serial.println("first lcd");
    lcd.clear();
    displayLcd();
    i++;
    delay(2000);
  }
  if (i > 2 && i < 10)
  {
    Serial.println("print lcd");
    displayLcd();
    i++;
    delay(2000);
  }
  if (i > 9)
  {
    Serial.println("clear lcd");
    lcd.clear();
    displayLcd();
    i = 1;
    delay(2000);
  }
  delay(500);
  for (gy = 1; gy < 5; gy++)
  {
    read_BH();
    delay(250);
  }

  readSHT();
  lcd.setCursor(0, 1);
  lcd.print("T/H:");
  lcd.print(temperature, 0);
  lcd.print("/");
  lcd.print(hum, 0);
  delay(1000);

  for (e = 1; e < 9; e++)
  {
    read_JSN();
    delay(250);
  }
  lcd.setCursor(10, 2);
  lcd.print("V :");
  lcd.print(vol, 0);
  delay(1000);

  sampling();
  delay(1000);

  read_temp();
  lcd.setCursor(10, 3);
  lcd.print("WT:");
  lcd.print(reservoir_temp);
  delay(1000);
  
  for (d = 1; d < 21; d++)
  {
    relay(1, 0, 1);
    delay(1000);
    static unsigned long timepoint = millis();
    if (millis() - timepoint > 1000U) //time interval: 1s
    {

      timepoint = millis();
      voltage1 = ads.readADC_SingleEnded(0) / 10;
      Serial.print("voltage:");
      Serial.println(voltage1, 0);

      //temperature = readTemperature();  // read your temperature sensor to execute temperature compensation
      Serial.print("temperature:");
      Serial.print(temp, 1);
      Serial.println("^C");

      ecValue = ec.readEC(voltage1, temp); // convert voltage to EC with temperature compensation
      ecValue = ecValue * 1000;
      Serial.print("EC:");
      Serial.print(ecValue, 4);
      Serial.println("us/cm");
    }

    ec.calibration(voltage1, temp); // calibration process by Serail CMD
    delay(1000);
  }

  delay(1000);
  relay(1, 1, 1);
  delay(1000);
  lcd.setCursor(0, 3);
  lcd.print("TDS:");
  lcd.print(ecValue, 0);
  delay(1000);

for (b = 1; b < 21; b++)
  {
    relay(0, 1, 1);
    delay(1000);
    static unsigned long timepoint = millis();
    if (millis() - timepoint > 1000U) //time interval: 1s
    {
      timepoint = millis();
      /**
       * index 0 for adc's pin A0
       * index 1 for adc's pin A1
       * index 2 for adc's pin A2
       * index 3 for adc's pin A3
      */
      voltage = ads.readADC_SingleEnded(1) / 10; // read the voltage
      Serial.print("voltage:");
      Serial.println(voltage, 0);

      //    reservoir_temp = readTemperature(); // read your temperature sensor to execute temperature compensation
      //    Serial.print("temperature:");
      //    Serial.print(reservoir_temp, 1);
      //    Serial.println("^C");

      phValue = ph.readPH(voltage, temp); // convert voltage to pH with temperature compensation
      Serial.print("pH:");
      Serial.println(phValue, 4);
    }
    ph.calibration(voltage, temp); // calibration process by Serail CMD
    delay(1000);
  }

  delay(1000);
  relay(1, 1, 1);
  delay(1000);
  lcd.setCursor(0, 2);
  lcd.print("PH :");
  lcd.print(phValue, 1);
  delay(500);
  
  timestamp();
  delay(500);
  displayLcd1();
  delay(5000);
}

/***************** sampling state ****************************************/
void sampling()
{
  relay(1, 1, 0);
  //    Serial.print("Pick sample water....");
  //    Serial.println(n);
  delay(10000);
  relay(1, 1, 1);
  delay(1000);
}

//----------------------start of RTC DS3231 function----------------------//
// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ((val / 10 * 16) + (val % 10));
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ((val / 16 * 10) + (val % 16));
}
void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);                    // set next input to start at the seconds register
  Wire.write(decToBcd(second));     // set seconds
  Wire.write(decToBcd(minute));     // set minutes
  Wire.write(decToBcd(hour));       // set hours
  Wire.write(decToBcd(dayOfWeek));  // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month));      // set month
  Wire.write(decToBcd(year));       // set year (0 to 99)
  Wire.endTransmission();
}
void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}
//----------------------end of RTC DS3231 function----------------------//

//-----------------------RTC--------------------------//
void waktu1()
{
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  Serial.print(dayOfMonth, DEC);
  Serial.print("-");
  Serial.print(month, DEC);
  Serial.print("-");
  Serial.print("20");
  Serial.print(year, DEC);
  Serial.print(" ");

  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute < 10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second < 10)
  {
    Serial.print("0");
  }
  Serial.println(second, DEC);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      PT. HAB      ");
  lcd.setCursor(0, 1);
  lcd.print("   Mon. Hidroponik  ");
  lcd.setCursor(0, 2);
  lcd.print("   ");
  lcd.print(dayOfMonth, DEC);
  lcd.print("-");
  lcd.print(month, DEC);
  lcd.print("-");
  lcd.print("20");
  lcd.print(year, DEC);
  lcd.print(" ");
  lcd.print(hour, DEC);
  lcd.print(":");
  if (minute < 10)
    lcd.print("0");
  lcd.print(minute, DEC);
  lcd.print("  ");
}
//------------------END of RTC----------------------//


/******************** Read SHT value ***********************/
void readSHT()
{

  hum = SHT2x.GetHumidity();
  temperature = SHT2x.GetTemperature();

  Serial.print("Humidity(%RH): ");
  Serial.print(hum);
  Serial.print("\tTemperature(C): ");
  Serial.println(temperature);
}
/********************* End SHT Rading **************************/

/********************** Relay Value ****************************/
void relay(int pH_state, int TDS_state, int sampling_state)
{
  digitalWrite(pHrelaypin, pH_state);
  digitalWrite(TDSrelaypin, TDS_state);
  digitalWrite(samplingrelay, sampling_state);
}
/********************** End Relay **********************/

// --------------- Reservoir Temperature --------------- //
void read_temp()
{
  sensors.requestTemperatures();
  delay(500);
  reservoir_temp = sensors.getTempCByIndex(0);
  Serial.print("reservoir temperature: ");
  Serial.println(reservoir_temp);
  delay(1000);
}
// ----- End Of Measurement ------ //

/******************************* BH1750 Reading *******************************/
void read_BH()
{
  lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  lcd.setCursor(10, 1);
  lcd.print("L :");
  lcd.print(lux);
  delay(1000);
}
/************************** End BH Reading ********************************/

/****************************** Read JSN *********************************/
void read_JSN()
{
  // Measure distance and print to the Serial Monitor:
  Serial.print("Distance = ");
  // Send ping, get distance in cm and print result (0 = outside set distance range):
  vol = sonar.ping_cm();
  Serial.print(vol);
  Serial.println(" cm");
  delay(500);
}
/***************************** End JSN Reading ******************************/

//------------------- Time Stamps -------------------//
void timestamp()
{
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  String hari = String(dayOfMonth, DEC);
  String bulan = String(month, DEC);
  String tahun = String(year, DEC);
  String jam = String(hour, DEC);
  String menit = String(minute, DEC);
  String detik = String(second, DEC);

  if (month < 10)
  {
    tanggal = "20" + tahun + "-" + "0" + bulan + "-" + hari;
    tanggal_ordered = hari + "-" + "0" + bulan + "-" + "20" + tahun;
  }
  else if (month >= 10)
  {
    tanggal = "20" + tahun + "-" + bulan + "-" + hari;
    tanggal_ordered = hari + "-" + bulan + "-" + "20" + tahun;
  }

  if (hour < 10 && minute < 10 && second < 10)
  {
    waktu = "0" + jam + ":" + "0" + menit + ":" + "0" + detik;
    waktu_ordered = "0" + jam + ":" + "0" + menit;
  }
  else if (hour < 10 && minute < 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + "0" + menit + ":" + detik;
    waktu_ordered = "0" + jam + ":" + "0" + menit;
  }
  else if (hour < 10 && minute >= 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + detik;
    waktu_ordered = "0" + jam + ":" + menit;
  }
  else if (hour < 10 && minute >= 10 && second < 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + "0" + detik;
    waktu_ordered = "0" + jam + ":" + menit;
    
  }
  else if (hour < 10 && minute >= 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + detik;
    waktu_ordered = "0" + jam + ":" + menit;
  }
  else if (hour >= 10 && minute < 10 && second < 10)
  {
    waktu = jam + ":" + "0" + menit + ":" + "0" + detik;
    waktu_ordered = jam + ":" + "0" + menit;
  }
  else if (hour >= 10 && minute >= 10 && second < 10)
  {
    waktu = jam + ":" + menit + ":" + "0" + detik;
    waktu_ordered = jam + ":" + menit;
  }
  else if (hour >= 10 && minute < 10 && second >= 10)
  {
    waktu = jam + ":" + "0" + menit + ":" + detik;
    waktu_ordered = jam + ":" + "0" + menit;
  }
  else
    waktu = jam + ":" + menit + ":" + detik;
    waktu_ordered = jam + ":" + "0" + menit;

  Serial.print(tanggal);
  Serial.print("   ");
  Serial.print(waktu);
  Serial.print("   ");
  Serial.println(waktu_ordered);
}
// ------------- End Of Timestamps --------------- //

/********************* Sensor Display to LCD ***************************/
void displayLcd()
{
  lcd.setCursor(0, 0);
  lcd.print("  ");
  lcd.print(tanggal_ordered);
  lcd.print(" ");
  lcd.print(waktu_ordered);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("T/H:");
  lcd.print(temperature, 0);
  lcd.print("/");
  lcd.print(hum, 0);
  lcd.setCursor(0, 2);
  lcd.print("PH :");
  lcd.print(phValue, 1);
  lcd.setCursor(0, 3);
  lcd.print("TDS:");
  lcd.print(ecValue, 0);
  lcd.setCursor(10, 1);
  lcd.print("L :");
  lcd.print(lux);
  lcd.setCursor(10, 2);
  lcd.print("V :");
  lcd.print(vol, 0);
  lcd.setCursor(10, 3);
  lcd.print("WT:");
  lcd.print(reservoir_temp);
  delay(5000);
  //  lcd.clear();
}
/************************ End of Display ***************************/

/********************* Sensor Display to LCD ***************************/
void displayLcd1()
{
  lcd.setCursor(0, 0);
  lcd.print("  ");
  lcd.print(tanggal_ordered);
  lcd.print(" ");
  lcd.print(waktu_ordered);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("T/H:");
  lcd.print(temperature, 0);
  lcd.print("/");
  lcd.print(hum, 0);
  lcd.setCursor(0, 2);
  lcd.print("PH :");
  lcd.print(phValue, 1);
  lcd.setCursor(0, 3);
  lcd.print("TDS:");
  lcd.print(ecValue, 0);
  lcd.setCursor(10, 1);
  lcd.print("L :");
  lcd.print(lux);
  lcd.setCursor(10, 2);
  lcd.print("V :");
  lcd.print(vol, 0);
  lcd.setCursor(10, 3);
  lcd.print("WT:");
  lcd.print(reservoir_temp);
  delay(5000);
  //  lcd.clear();
}
/************************ End of Display ***************************/
