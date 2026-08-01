//Inclusion of libraries
#include <Wire.h>
#include "SparkFunCCS811.h"
#include <SparkFunTSL2561.h>
#include <SparkFun_APDS9960.h>
#include "BlinkM_funcs123.h"
#include "MAX30105.h"
#include "heartRate.h"
#include "SparkFun_Si7021_Breakout_Library.h"

//Address definitions
#define mag_addr 0x1E                 //Magnetometer - HMC5883L
#define i2cled_addr 0x09              //I2C Led - BlinkM
#define air_addr 0x5B                 //Air Quality Sensor - CCS811
#define gest_addr 0x39                //Gesture Sensor - APDS9960
#define pressure_addr 0x77            //Pressure Sensor - BMP180
#define th_addr 0x40                  //Temperature and Humidity Sensor - Si7021
#define lumi_addr 0x29                //Luminous Sensor - TSL2561
#define particle_addr 0x57            //Particle Sensor - Max30105
#define max_value 127                 //Maximum No. of I2c Address

//array of devices which require initialization. 0 --> don't require initialization, 1 --> require initialization.
int initDevices[max_value];

//array of currently connected devices. 0 --> Not connected, 1 --> connected
int devices[max_value] = {0};

//devices connected in previous iteration of loop.
int prevIter[max_value] = {0};




//ccs object
CCS811 mySensor(air_addr);
//sparkfun object
SparkFun_APDS9960 apds = SparkFun_APDS9960();
Weather sensor;
//apds
uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0;



uint8_t proximity_data = 0;


// Global variables:
MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

long samplesTaken = 0; //Counter for calculating the Hz or read rate
long unblockedValue; //Average IR at power up
long startTime; //Used to calculate measurement rate

int16_t  ac1, ac2, ac3, b1, b2, mb, mc, md; // Store sensor PROM values from BMP180
uint16_t ac4, ac5, ac6;                     // Store sensor PROM values from BMP180
// Ultra Low Power       OSS = 0, OSD =  5ms
// Standard              OSS = 1, OSD =  8ms
// High                  OSS = 2, OSD = 14ms
// Ultra High Resolution OSS = 3, OSD = 26ms
const uint8_t oss = 3;                      // Set oversampling setting
const uint8_t osd = 26;                     // with corresponding oversampling delay

float T, P;



void setup()
{
  // put your setup code here, to run once:
  Wire.begin();

  //initialize the initialization array
  initDevices[mag_addr] = 1;
  initDevices[air_addr] = 1;
  initDevices[gest_addr] = 1;
  initDevices[pressure_addr] = 1;
  initDevices[i2cled_addr] = 1;
  initDevices[lumi_addr] = 1;
  initDevices[particle_addr] = 1;

  Serial.begin(9600);
  while (!Serial);

}

int i = 0;
void loop()
{
  Serial.println("Aagam");
  // put your main code here, to run repeatedly:
  scan_Devices();

  //loop for initialisation of devices
  init_Devices();

  //loop for reading sensor data
  read_Data();

  //set prevIter for next iteration
  save_Data();

  delay(1000);

  //RGB LED
  if (Serial.available())
  {
    char in = Serial.read();
    Serial.println(in);
    if ( in == 'a' )
    {
      BlinkM_setRGB( i2cled_addr, 0xff, 0xff, 0xff ); // turn full white
      delay(100);
    }
  }
  else
  {
    BlinkM_setRGB( i2cled_addr, 0x00, 0x00, 0x00 );
    delay(100);
  }

}

//Scan I2C devices
void scan_Devices()
{
  byte error;
  int address;
  int nDevices = 0;               //No. of sensors/actuators connected

  //scan for i2c sensors/actuators
  for (int address = 1 ; address < max_value ; address++ )
  {
    //Check if I2C address is available
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)   //device is found
    {
      Serial.println("Device Found");
      Serial.println(address);

      devices[address] = 1;
      if (address < 16)                                                                   //Check with Siddharth
        nDevices++;
    }
    else if (error == 4)
    {
      if (address < 16)
        devices[address] = 0;
    }
    else
    {
      devices[address] = 0;
    }
  }
}

//Read Data
void read_Data()
{
  for (int i = 0; i < max_value; i++)
  {
    if (devices[i] == 1)
    {
      readFunction(i);
    }
  }
}

//Initialize all the devices
void init_Devices()
{
  for (int i = 0; i < max_value; i++)
  {
    // If the device is connected in this iteration
    if (devices[i] == 1)
    {
      //Initialize the sensor only if it wasn't connected in the previous iteration and requires initializaton
      if (prevIter[i] == 0 && initDevices[i] == 1)
      {
        initFunction(i);
      }
    }
  }
}

//Save the Data
void save_Data()
{
  for (int i = 0; i < max_value; i++)
  {
    prevIter[i] = devices[i];
  }
}

//Initialize all Functions
void initFunction(int addr)
{
  //Initialise Magnetometer Sensor
  if (addr == mag_addr)
  {
    Wire.beginTransmission(mag_addr); //open communication with HMC5883
    Wire.write(0x02); //select mode register
    Wire.write(0x00); //continuous measurement mode
    Wire.endTransmission();
  }
  //Initialise Air Quality Sensor
  else if (addr == air_addr)
  {
    CCS811Core::status returnCode = mySensor.begin();
  }
  //Initialise Gesture Sensor
  else if (addr == gest_addr)
  {
    apds.init();
    apds.enableLightSensor(false);
    if ( !apds.setProximityGain(PGAIN_2X))
    {
      Serial.println(F("Something went wrong trying to set PGAIN"));
    }
    // Start running the APDS-9960 proximity sensor (no interrupts)
    if ( apds.enableProximitySensor(false))
    {
      Serial.println(F("Proximity sensor is now running"));
    }
    else
    {
      Serial.println(F("Something went wrong during sensor init!"));
    }
  }
  //Initialise Pressure Sensor
  else if (addr == pressure_addr)
  {
    init_SENSOR();
  }
  //Initialize I2C led
  else if (addr == i2cled_addr)
  {
    BlinkM_beginWithPower();
    BlinkM_stopScript( i2cled_addr );
  }
  //Initialize Luminous Sensor
  else if (addr == lumi_addr)
  {
    Wire.beginTransmission(addr);
    // Select control register
    Wire.write(0x00 | 0x80);
    // Power ON mode
    Wire.write(0x03);
    // Stop I2C Transmission
    Wire.endTransmission();

    // Starts I2C communication
    Wire.beginTransmission(addr);
    // Select timing register
    Wire.write(0x01 | 0x80);
    // Nominal integration time = 402ms
    Wire.write(0x02);
    // Stop I2C Transmission
    Wire.endTransmission();
  }
  else if (addr == particle_addr)
  {
    particleSensor.begin(Wire, I2C_SPEED_FAST);
    byte ledBrightness = 0xFF; //Options: 0=Off t;o 255=50mA
    byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
    byte sampleRate = 400; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    int pulseWidth = 411; //Options: 69, 118, 215, 411
    int adcRange = 2048; //Options: 2048, 4096, 8192, 16384

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

    particleSensor.setPulseAmplitudeRed(0x0A); //Turn off Red LED
    particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

    //Take an average of IR readings at power up
    unblockedValue = 0;
    for (byte x = 0 ; x < 32 ; x++)
    {
      unblockedValue += particleSensor.getIR(); //Read the IR value
    }
    unblockedValue /= 32;
    startTime = millis();
  }
}


//Read Function for sensor
void readFunction(int addr)
{
  //Reading Magnetometer sensor data
  if (addr == mag_addr)
  {
    int x, y, z;
    Wire.beginTransmission(mag_addr);
    Wire.write(0x03); //select register 3, X MSB register
    Wire.endTransmission();


    //Read data from each axis, 2 registers per axis
    Wire.requestFrom(mag_addr, 6);
    if (6 <= Wire.available())
    {
      x = Wire.read() << 8; //X msb
      x |= Wire.read();     //X lsb
      z = Wire.read() << 8; //Z msb
      z |= Wire.read();     //Z lsb
      y = Wire.read() << 8; //Y msb
      y |= Wire.read();     //Y lsb
    }

    //Print out values of each axis
    Serial.print("x: ");
    Serial.print(x);
    Serial.print("  y: ");
    Serial.print(y);
    Serial.print("  z: ");
    Serial.println(z);

  }
  //Reading Air Quality sensor data
  else if (addr == air_addr)
  {
    int itr = 0;
    while (!mySensor.dataAvailable() && itr < 13)             //Ask Siddharth
      itr = itr + 1;
    if (itr == 13)                                            //Ask Siddharth
    {
      //If so, have the sensor read and calculate the results.
      //Get them later
      mySensor.readAlgorithmResults();

      Serial.print("CO2[");
      //Returns calculated CO2 reading
      Serial.print(mySensor.getCO2());
      Serial.print("] tVOC[");
      //Returns calculated TVOC reading
      Serial.print(mySensor.getTVOC());
      Serial.print("] millis[");
      //Simply the time since program start
      Serial.print(millis());
      Serial.print("]");
      Serial.println();
    }
  }
  //Reading Gesture Sensor data
  else if (addr == gest_addr)
  {
    //Reading RGB values from gesture sensor.
    if (  !apds.readAmbientLight(ambient_light) || !apds.readRedLight(red_light) || !apds.readGreenLight(green_light) || !apds.readBlueLight(blue_light) )
    {
      Serial.println("Error reading light values");
    }
    else
    {
      Serial.print("Ambient: ");
      Serial.print(ambient_light);
      Serial.print(" Red: ");
      Serial.print(red_light);
      Serial.print(" Green: ");
      Serial.print(green_light);
      Serial.print(" Blue: ");
      Serial.println(blue_light);
    }
    //Proximity value from Gesture sensor
    if ( !apds.readProximity(proximity_data) )
    {
      Serial.println("Error reading proximity value");
    }
    else
    {
      Serial.print("Proximity: ");
      Serial.println(proximity_data);
    }
  }
  //Reading Pressure sensor
  else if (addr == pressure_addr)
  {
    int32_t b5;
    b5 = temperature();                       // Lit et calcule la température (T)
    Serial.print("Temperature: ");
    Serial.print(T, 2);
    Serial.print("*C, ");
    P = pressure(b5);                         // Lit et calcule la pressure (P)
    Serial.print("Pressure: ");
    Serial.print(P, 2);
    Serial.print(" mbar, ");
    Serial.print(P * 0.75006375541921, 2);
    Serial.print(" mmHg, ");
    Serial.print(P * 0.75006375541921 * 133.322387415);
    Serial.println(" Pascal");    
  }
  //Reading Temperature and Humidity Sensor
  else if (addr == th_addr)
  {
    // Measure Relative Humidity from the HTU21D or Si7021
    int  humidity = sensor.getRH();
    // Measure Temperature from the HTU21D or Si7021
    int tempf = sensor.getTempF();
    Serial.print("Humidity:");
    Serial.print(humidity);
    Serial.print("temperature");
    Serial.print(tempf);
    Serial.println("F");
  }
  //Readig Luminous Sensor
  else if (addr == lumi_addr)
  {
    unsigned int data[4];
    for (int i = 0; i < 4; i++)
    {
      // Starts I2C communication
      Wire.beginTransmission(lumi_addr);
      // Select data register
      Wire.write((140 + i));
      // Stop I2C Transmission
      Wire.endTransmission();

      // Request 1 byte of data
      Wire.requestFrom(lumi_addr, 1);

      // Read 1 bytes of data
      if (Wire.available() == 1)
      {
        data[i] = Wire.read();
      }
      delay(500);
    }
    // Convert the data
    double ch0 = ((data[1] & 0xFF) * 256) + (data[0] & 0xFF);
    double ch1 = ((data[3] & 0xFF) * 256) + (data[2] & 0xFF);
    double Lux_value = 0;
    if (0 < ch1 / ch0 <= 0.52)
    {
      Lux_value = 0.0315 * ch0 - 0.0593 * ch0 * pow((ch1 / ch0), 0.25);
    }
    else if (0.52 < ch1 / ch0 <= 0.65)
    {
      Lux_value = 0.0229 * ch0 - 0.0291 * ch1;
    }
    else if (0.65 < ch1 / ch0 <= 0.80)
    {
      Lux_value = 0.0157 * ch0 - 0.0180 * ch1;
    }
    else if (0.80 < ch1 / ch0 <= 1.30)
    {
      Lux_value = 0.00338 * ch0 - 0.00260 * ch1;
    }
    else if (ch1 / ch0 > 1.30)
    {
      Lux_value = 0;
    }
    // Output data to serial monitor
    Serial.println(); Serial.println();
    Serial.println("Data from Light Intensity Sensor");
    Serial.println();
    Serial.print("Infrared Value : ");
    Serial.println(ch1);
    Serial.print("Visible Value : ");
    Serial.println(ch0 - ch1);
    Serial.print("LUX Value : ");
    Serial.println(Lux_value);
    Serial.println("---------------------------------");
  }
  //Reading Particle Sensor
  else if (addr = particle_addr)
  {
    Serial.print("IR[");
    Serial.print(particleSensor.getIR());
    Serial.print("] Hz[");
    Serial.print((float)samplesTaken / ((millis() - startTime) / 1000.0), 2);
    Serial.print("]");

    long currentDelta = particleSensor.getIR() - unblockedValue;

    Serial.print(" delta[");
    Serial.print(currentDelta);
    Serial.print("]");

    if (currentDelta > (long)1000)
    {
      Serial.print(" Something is there!");
    }
  }

}

/**********************************************
  Initialise les variables du capteur
 **********************************************/
void init_SENSOR()
{
  ac1 = read_2_bytes(0xAA);
  ac2 = read_2_bytes(0xAC);
  ac3 = read_2_bytes(0xAE);
  ac4 = read_2_bytes(0xB0);
  ac5 = read_2_bytes(0xB2);
  ac6 = read_2_bytes(0xB4);
  b1  = read_2_bytes(0xB6);
  b2  = read_2_bytes(0xB8);
  mb  = read_2_bytes(0xBA);
  mc  = read_2_bytes(0xBC);
  md  = read_2_bytes(0xBE);

  Serial.println("");
  Serial.println("Données de calibration du capteur :");
  Serial.print(F("AC1 = ")); Serial.println(ac1);
  Serial.print(F("AC2 = ")); Serial.println(ac2);
  Serial.print(F("AC3 = ")); Serial.println(ac3);
  Serial.print(F("AC4 = ")); Serial.println(ac4);
  Serial.print(F("AC5 = ")); Serial.println(ac5);
  Serial.print(F("AC6 = ")); Serial.println(ac6);
  Serial.print(F("B1 = "));  Serial.println(b1);
  Serial.print(F("B2 = "));  Serial.println(b2);
  Serial.print(F("MB = "));  Serial.println(mb);
  Serial.print(F("MC = "));  Serial.println(mc);
  Serial.print(F("MD = "));  Serial.println(md);
  Serial.println("");
}

/**********************************************
  Calcul de la pressure
 **********************************************/
float pressure(int32_t b5)
{
  int32_t x1, x2, x3, b3, b6, p, UP;
  uint32_t b4, b7;

  UP = read_pressure();                         // Lecture de la pression renvoyée par le capteur

  b6 = b5 - 4000;
  x1 = (b2 * (b6 * b6 >> 12)) >> 11;
  x2 = ac2 * b6 >> 11;
  x3 = x1 + x2;
  b3 = (((ac1 * 4 + x3) << oss) + 2) >> 2;
  x1 = ac3 * b6 >> 13;
  x2 = (b1 * (b6 * b6 >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  b4 = (ac4 * (uint32_t)(x3 + 32768)) >> 15;
  b7 = ((uint32_t)UP - b3) * (50000 >> oss);
  if (b7 < 0x80000000) {
    p = (b7 << 1) / b4;  // ou p = b7 < 0x80000000 ? (b7 * 2) / b4 : (b7 / b4) * 2;
  } else {
    p = (b7 / b4) << 1;
  }
  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  return (p + ((x1 + x2 + 3791) >> 4)) / 100.0f; // Retourne la pression en mbar
}

/**********************************************
  Lecture de la température (non compensée)
 **********************************************/
int32_t temperature()
{
  int32_t x1, x2, b5, UT;

  Wire.beginTransmission(pressure_addr); // Début de transmission avec l'Arduino
  Wire.write(0xf4);                       // Envoi l'adresse de registre
  Wire.write(0x2e);                       // Ecrit la donnée
  Wire.endTransmission();                 // Fin de transmission
  delay(5);

  UT = read_2_bytes(0xf6);                // Lecture de la valeur de la TEMPERATURE

  // Calcule la vrai température
  x1 = (UT - (int32_t)ac6) * (int32_t)ac5 >> 15;
  x2 = ((int32_t)mc << 11) / (x1 + (int32_t)md);
  b5 = x1 + x2;
  T  = (b5 + 8) >> 4;
  T = T / 10.0;                           // Retourne la température in celsius
  return b5;
}

/**********************************************
  Lecture de la pression
 **********************************************/
int32_t read_pressure()
{
  int32_t value;
  Wire.beginTransmission(pressure_addr);   // Début de transmission avec l'Arduino
  Wire.write(0xf4);                         // Envoi l'adresse de registre
  Wire.write(0x34 + (oss << 6));            // Ecrit la donnée
  Wire.endTransmission();                   // Fin de transmission
  delay(osd);
  Wire.beginTransmission(pressure_addr);
  Wire.write(0xf6);
  Wire.endTransmission();
  Wire.requestFrom(pressure_addr, 3);
  if (Wire.available() >= 3)
  {
    value = (((int32_t)Wire.read() << 16) | ((int32_t)Wire.read() << 8) | ((int32_t)Wire.read())) >> (8 - oss);
  }
  return value;                             // Renvoie la valeur
}

/**********************************************
  Lecture d'un byte sur la capteur BMP
 **********************************************/
uint8_t read_1_byte(uint8_t code)
{
  uint8_t value;
  Wire.beginTransmission(pressure_addr);
  Wire.write(code);
  Wire.endTransmission();
  Wire.requestFrom(pressure_addr, 1);
  if (Wire.available() >= 1)
  {
    value = Wire.read();
  }
  return value;
}

/**********************************************
  Lecture de 2 bytes sur la capteur BMP
 **********************************************/
uint16_t read_2_bytes(uint8_t code)
{
  uint16_t value;
  Wire.beginTransmission(pressure_addr);
  Wire.write(code);
  Wire.endTransmission();
  Wire.requestFrom(pressure_addr, 2);
  if (Wire.available() >= 2)
  {
    value = (Wire.read() << 8) | Wire.read();     // Récupère 2 bytes de données
  }
  return value;                                   // Renvoie la valeur
}
