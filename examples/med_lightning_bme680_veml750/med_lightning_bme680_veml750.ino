// -------- USER INPUTS
#define     DEBUG true                                          // true setup and use serial print
int     fitzpatrickScore            = 13;                       // Fitzpatrick Skin Score       0-32
int     sunProtectFactor            = 30;                       // SPF Applied                  0-100
int     onWater                     = 0;                        // On Water                     bool
int     onSnow                      = 0;                        // Snow Present                 bool
double  altitudeM                   = 3;                        // Altitude (meters)            0-11000
#define SEALEVELPRESSURE_HPA (1013.25)

// -------- Clock Access
#include <RTCZero.h>
RTCZero chrono;
#include "RTClib.h"
RTC_Millis rtc;

#include <notes.h>
#define TONE_PIN                    A0


// -------- SENSORS
#include <Adafruit_VEML6075.h>
Adafruit_VEML6075 uv = Adafruit_VEML6075();

#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
Adafruit_BME680 bme;


#include <SparkFun_AS3935.h>
#define AS3935_CS_PIN               10                          // AS3935 chip select pin
#define AS3935_STRIKE_PIN           6                           // strike interrupt pin (HIGH)

#define AS3935_INT_LIGHTNING        0x08
#define AS3935_INT_DISTURBER        0x04
#define AS3935_INT_NOISE            0x01

#define AS3935_MASK_DISTURBER       false                       // mask disturber events false[d]
#define AS3935_INDOOR_OUTDOOR       0xE                         // attenuator outside 0xE or inside 0x12[d]
#define AS3935_NOISE_LEVEL          2                           // noise floor 1-7 (2 default)
#define AS3935_WATCHDOG_THRESHOLD   2                           // watchdog threshold 1-10 (2 default)
#define AS3935_SPIKE_REJECTION      2                           // spike rejection 1-11 (2 default)
#define AS3935_LIGHTENING_THRESHOLD 1                           // strikes to trip interrupt 1,5,9, or 26
char indoorOutdoor[2][20] =         {"INDOOR 0xE", "OUTDOOR 0x12"};
int  inOut;
char falseTrue[2][5] =              {"NO", "YES"};
SparkFun_AS3935 strike;


// -------- SENSOR INPUTS, OUTPUTS, FUNCTIONS
double  uvIndex;                                                // UV Index                     0.00-26.00
double  uvBReading;                                             // UV B reading

double  temperatureC;                                           // Temperature (° Celsius)
double  temperatureF;                                           // Temperature (° Fahrenheit)
double  humidityPercent;                                        // Relative humididty (%)       0-100%
double  dewpointF;                                              // Dewpoint (°Fahrenheit)
double  heatIndexF;                                             // Heat Index (°Fahrenheit)

double  barameterKPA;                                           // Barometer (kPa)              0-1086
double  barameterHG;                                            // Barometer (in. mercury)

int     minutes2MED;                                            // Minutes to Minimal Erythemal Dose (MED)

double celsuisFahrenheit(double celsius) {                      // convert Celcius to Fahrenheit
    return ((celsius * 9) / 5) + 32; 
}
double pascalsHG(float pascals) {                               // convert Pascals to in. mercury
    return (pascals * 0.000295333727); 
}
double getDewpoint(double cel, int hum) {                       // calc dew point
    double Td = (237.7 * ((17.271 * cel) / 
                (237.7 + cel) + 
                log(hum * 0.01))) / 
                (17.271 - ((17.271 * cel) / 
                (237.7 + cel) + 
                log(hum * 0.01)));
    return celsuisFahrenheit(Td);
}
double getHeatIndex(double tempF, double humd_l) {              // calc Heat Index
    if (tempF < 80 || humd_l < 40) {
        double pass = tempF; 
        return pass;
    }
    double c1=-42.38, c2=2.049;
    double c3=10.14, c4=-0.2248;
    double c5=-6.838e-3, c6=-5.482e-2;
    double c7=1.228e-3, c8=8.528e-4;
    double c9=-1.99e-6;
    double t = tempF;
    double r = humd_l;
    double a = ((c5 * t) + c2) * t + c1;
    double b = ((c7 * t) + c4) * t + c3;
    double c = ((c9 * t) + c8) * t + c6;
    double rv = (c * r + b) * r + a;
    return rv;
}
char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  // convert floats to fixed string
  // val      Your float variable;
  // width    Length of the string that will be created INCLUDING decimal point;
  // prec     Number of digits after the deimal point to print;
  // sout     Destination of output buffer
    char fmt[20];
    sprintf(fmt, "%%%d.%df", width, prec);
    sprintf(sout, fmt, val);
    return sout;
}
double med() {                                                  // mins to Minimal Erythemal Dose (MED)
    double uvi_f = (uvIndex * (altitudeM * 1.2)) +              // alt factor (1.2 x meters above sea level)
                   (uvIndex * (onWater * 1.5)) +                // on water UVindex factor (1.5x)
                   (uvIndex * (onSnow * 1.85));                 // on snow UVindex factor (1.85x)
    double s2med_b = (-3.209E-5 * pow(fitzpatrickScore, 5)) +   // Fitzpatrick score @ 1 UV idx secs to MED
                     (2.959E-3 * pow(fitzpatrickScore, 4)) -    // 5th order polynomial plot
                     (0.103 * pow(fitzpatrickScore, 3)) +
                     (1.664* pow(fitzpatrickScore, 2)) +
                     (3.82 * fitzpatrickScore) + 
                     34.755;
    double s2med = ((s2med_b / uvi_f) * sunProtectFactor);      // combine factors
    double m2med = s2med / 60;                                  // convert secs to mins
    if (m2med > 480) m2med = 480;                               // max at 6 hours
    return m2med;
}


#include <U8g2lib.h>
U8G2_SSD1327_MIDAS_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define     CHAR_W                  6                           // display size 1 character width
#define     CHAR_H                  8                           // display size 1 character height
#define     LINE_HEIGHT             14
#define     MARGIN_W                3
#define     COLUMN_2                70

#define     TITLE_LINE              14
#define     MIN2MED_LINE            28
#define     UVI_LINE                42
#define     UVB_LINE                56
#define     HEATINDEX_LINE          70
#define     DEWPOINT_LINE           84
#define     TEMPERATURE_LINE        98
#define     HUMIDITY_LINE           112
#define     BAROMETER_LINE          126
#define     STRIKE_LINE             120
#define     SPRITE0_X               123
#define     SPRITE0_Y               123
#define     SPRITE1_X               123
#define     SPRITE1_Y               0


#include <Adafruit_NeoPixel.h>
#define LED_PIN                 3
#define LED_COUNT               8
bool heartBeat =                true;                          // for heartbeat
unsigned long                   lastMillis;
#define NEO_RED                 0xFF0000
#define NEO_ORANGE              0xFF8000
#define NEO_YELLOW              0xFFFF00
#define NEO_GREEN               0x00FF00
#define NEO_CYAN                0x00FFFF
#define NEO_LIGHTBLUE           0x00BFFF
#define NEO_BLUE                0x0000FF
#define NEO_PURPLE              0x8000FF
#define NEO_PINK                0xFF00FF
#define NEO_WHITE               0xFFFFFF
#define NEO_OFF                 0x000000
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


void setup() {
    asm(".global _printf_float");                               // printf renders floats

#if DEBUG
    Serial.begin(115200);
    while (!Serial);                                            // wait for serial port to open
    Serial.println("\n\n ------------------- setup() ");
#endif

    alarm();

//    ssd1351OLEDSetup();

    veml6075Setup();

    bme680Setup();

    aS3935Setup();
    
    strip.begin();
    
    chrono.begin();
    chrono.enableAlarm(chrono.MATCH_SS);                        // fire every minute
    chrono.attachInterrupt(getReadings);

    delay(1000);
    
#if DEBUG
    Serial.print("end setup at ");
    DateTime now = rtc.now();
    Serial.print(now.hour(), DEC);      Serial.print("hr ");    
    Serial.print(now.minute(), DEC);    Serial.print("min ");
    Serial.print(now.second(), DEC);    Serial.println("sec");
#endif
}

void ssd1351OLEDSetup() {
    Serial.println(" ------------------- ssd1351OLEDSetup()");
    
    u8g2.setI2CAddress(0x7a);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_9x18_tf);                            // [t]ransparent [f]ull 256bit set

    char title[] = "WX Readings";
    u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getStrWidth(title))/2, TITLE_LINE);
    u8g2.print(title);

    u8g2.drawUTF8(MARGIN_W, MIN2MED_LINE,       "MED in");
    u8g2.drawUTF8(MARGIN_W, UVI_LINE,           "UVIndx");
    u8g2.drawUTF8(MARGIN_W, UVB_LINE,           "UVb");
    u8g2.drawUTF8(MARGIN_W, HEATINDEX_LINE,     "HtIdx");
    u8g2.drawUTF8(MARGIN_W, DEWPOINT_LINE,      "Dewpt");
    u8g2.drawUTF8(MARGIN_W, TEMPERATURE_LINE,   "Temp");
    u8g2.drawUTF8(MARGIN_W, HUMIDITY_LINE,      "Humid");
    u8g2.drawUTF8(MARGIN_W, BAROMETER_LINE,     "Baro");
    
    u8g2.sendBuffer();
}

void veml6075Setup() {

    Serial.println(" ------------------- veml6075Setup()");
    
    if (! uv.begin()) {
#if DEBUG
        Serial.println("VEML6075 sensor fail");
#endif
    }
    
    uv.setIntegrationTime(VEML6075_100MS);
    uv.setHighDynamic(true);
    uv.setForcedMode(false);                                    // Set the mode
    uv.setCoefficients( 2.22, 1.33,                             // UVA_A and UVA_B coefficients
                        2.95, 1.74,                             // UVB_C and UVB_D coefficients
                        0.001461, 0.002591);                    // UVA and UVB responses

#if DEBUG
    Serial.println("VEML6075 sensor connect");
    Serial.print("Integration time set to: ");
    switch (uv.getIntegrationTime()) {
        case VEML6075_50MS:  Serial.print("50");  break;
        case VEML6075_100MS: Serial.print("100"); break;
        case VEML6075_200MS: Serial.print("200"); break;
        case VEML6075_400MS: Serial.print("400"); break;
        case VEML6075_800MS: Serial.print("800"); break;
    }
    Serial.println("ms");

    delay(500);
    
    if (uv.getHighDynamic()) Serial.println("Dynamic reading mode: High");
    else Serial.println("Dynamic reading mode: Normal");
    if (uv.getForcedMode()) Serial.println("Reading mode: Forced");
    else Serial.println("Reading mode: Continuous");
    Serial.println("VEML6075 rest readings");

    delay(500);
    Serial.print("Raw UVA reading:  "); Serial.println(uv.readUVA());
    Serial.print("Raw UVB reading:  "); Serial.println(uv.readUVB());
    Serial.print("UV Index reading: "); Serial.println(uv.readUVI());
#endif
}

void bme680Setup() {

    if (!bme.begin(0x76)) {
    
#if DEBUG
        Serial.println("VEML6075 sensor fail");
#endif
    }

    bme.setTemperatureOversampling(BME680_OS_8X);               // Set up oversampling and filter initialization
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);                                 // 320*C for 150 ms

#if DEBUG
    unsigned long endTime = bme.beginReading();
    if (endTime == 0) {
        Serial.println("Failed to begin :(");
        return;
    }
    Serial.print("Reading started at ");
    Serial.print(millis());
    Serial.print(" and finishes at ");
    Serial.println(endTime);

    Serial.println("delay(50) wait for readings");
    delay(50);

    if (!bme.endReading()) {
        Serial.println("Failed to complete reading :(");
        return;
    }
    Serial.print("Reading completed at ");
    Serial.println(millis());

    Serial.print("Temperature = ");
    Serial.print(bme.temperature);
    Serial.println("°C");

    Serial.print("Pressure = ");
    Serial.print(bme.pressure / 100.0);
    Serial.println(" hPa");

    Serial.print("Humidity = ");
    Serial.print(bme.humidity);
    Serial.println("%");

    Serial.print("Gas = ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(" KOhms");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println("m\n");

    delay(2000);
#endif
}

void aS3935Setup() {
    pinMode(AS3935_STRIKE_PIN, INPUT);
    while(!strike.beginSPI(AS3935_CS_PIN, 2000000));

    Serial.println("------------------- aS3935Setup()");

    //strike.resetSettings();
    //strike.clearStatistics();
    //strike.powerDown();

    Serial.println("PARAMETER\t\t\t\t\SETTING\t\tREADING");  

    strike.maskDisturber(AS3935_MASK_DISTURBER);
    Serial.print("Disturbers Masked? ( yes no[d] )\t");
    Serial.print(falseTrue[AS3935_MASK_DISTURBER]);
    Serial.print("\t\t");
    Serial.println(falseTrue[strike.readMaskDisturber()]);

    strike.setIndoorOutdoor(AS3935_INDOOR_OUTDOOR);
    Serial.print("IndoorOutdoor( out 0x12 [d], in 0xE )\t0x");
    Serial.print(AS3935_INDOOR_OUTDOOR, HEX);
    Serial.print("\t\t");
    if (strike.readIndoorOutdoor() == 0xE) inOut = 0;                   // outdoor
    else inOut = 1;                                                     // indoor
    Serial.println(indoorOutdoor[inOut]);

    strike.setNoiseLevel(AS3935_NOISE_LEVEL);
    Serial.print("NoiseLevel( 1 - 7 2[d] )\t\t");
    Serial.print(AS3935_NOISE_LEVEL);
    Serial.print("\t\t");
    Serial.println(strike.readNoiseLevel());

    strike.watchdogThreshold(AS3935_WATCHDOG_THRESHOLD);
    Serial.print("WatchdogThreshold( 1 - 10 2[d] )\t");
    Serial.print(AS3935_WATCHDOG_THRESHOLD);
    Serial.print("\t\t");
    Serial.println(strike.readWatchdogThreshold());

    strike.spikeRejection(AS3935_SPIKE_REJECTION);
    Serial.print("SpikeRejection( 1 - 11 2[d] )\t\t");
    Serial.print(AS3935_SPIKE_REJECTION);
    Serial.print("\t\t");
    Serial.println(strike.readSpikeRejection());

    strike.lightningThreshold(AS3935_LIGHTENING_THRESHOLD);
    Serial.print("LightningThreshold( 1, 5, 9, 26 1[d] )\t");
    Serial.print(AS3935_LIGHTENING_THRESHOLD);
    Serial.print("\t\t");
    Serial.println(strike.readLightningThreshold());
}

void loop() {
    if(digitalRead(AS3935_STRIKE_PIN) == HIGH) lightningEvent();
}

void cycleNEO() {
    int delayTime = 25;
    strip.setBrightness(255);
    
    for(int x = 0; x < LED_COUNT; x++) {
        strip.setPixelColor(x, NEO_WHITE); 
        strip.show();
        delay(delayTime);
    }
    for(int x = 0; x < LED_COUNT; x++) {
        strip.setPixelColor(x, NEO_LIGHTBLUE); 
        strip.show();
        delay(delayTime);
    }
    for(int x = 0; x < LED_COUNT; x++) {
        strip.setPixelColor(x, NEO_CYAN);
        strip.show();
        delay(delayTime);
    }
    for(int x = 0; x < LED_COUNT; x++) {
        strip.setPixelColor(x, NEO_BLUE); 
        strip.show();
        delay(delayTime);
    }
    for(int x = 0; x < LED_COUNT; x++) {
        strip.setPixelColor(x, NEO_OFF); 
        strip.show();
        delay(delayTime);
    }
}

void alarm() {

#if DEBUG
        Serial.println("alarm()");
#endif

    int melody[] = { NOTE_C4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_C4 };    
    int noteDurations[] = { 8, 8, 8, 8, 8, 8, 8, 8 };    // 4 = qtr note, 8 = 8th note
    
    for (int thisNote = 0; thisNote < 8; thisNote++) {
        int noteDuration = 3000 / noteDurations[thisNote];
        tone(TONE_PIN, melody[thisNote], noteDuration);
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
    }
}

void lightningEvent() {
    DateTime now = rtc.now();
    char strike_buf[40] = "\0";
    int intVal = strike.readInterruptReg();

    if(intVal == AS3935_INT_NOISE) {                            // NOISE
        sprintf(strike_buf, "Noise (0x0%X) %d:%2d", 
                intVal, now.hour(), now.minute());
        //strip.setPixelColor(7, NEO_ORANGE);
    }

    else if(intVal == AS3935_INT_DISTURBER) {                   // DISTURBER
        sprintf(strike_buf, "disturb (0x0%X) %d:%2d", 
                intVal, now.hour(), now.minute());
        //strip.setPixelColor(7, NEO_YELLOW);
    }

    else if(intVal == AS3935_INT_LIGHTNING) {                   // STRIKE
        sprintf(strike_buf, "STRIKE %d:%2d  %d.%dkm", 
                now.hour(), now.minute(), 
                (int)strike.distanceToStorm(), (int)(strike.distanceToStorm()*10)%10);

        u8g2.setDrawColor(0);
        u8g2.drawBox(0, STRIKE_LINE - LINE_HEIGHT, 127, LINE_HEIGHT);

        u8g2.setDrawColor(1);
        u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getStrWidth(strike_buf))/2, STRIKE_LINE);
        u8g2.print(strike_buf);

        u8g2.sendBuffer();
        cycleNEO();
        alarm();
        strip.setPixelColor(7, NEO_RED);
        strip.setPixelColor(5, NEO_GREEN);
    }

    strip.show();

#ifdef DEBUG
        Serial.println(strike_buf);
#endif  
}

void getReadings() {

#if DEBUG
    
    Serial.print(" ------------------- getReadings() at ");
    DateTime now = rtc.now();
    Serial.print(now.hour(), DEC);      Serial.print("hr ");    
    Serial.print(now.minute(), DEC);    Serial.print("min ");
    Serial.print(now.second(), DEC);    Serial.println("sec");
#endif

    uvIndex         = uv.readUVI();             delay(50);          // VEML6075 UV Index
    uvBReading      = uv.readUVB();             delay(50);          // VEML6075 UVB reading
    temperatureC    = bme.temperature;          delay(50);          // BME680 temperature °Celcius)
    humidityPercent = bme.humidity;             delay(50);          // BME680 relative humididty (%)
    barameterKPA    = bme.pressure / 100.0;     delay(50);          // BME680 barometric pressure (hPascals) 
    
    temperatureF    = celsuisFahrenheit(temperatureC);              // BME680 temperature °Fahrenheit)    
    dewpointF       = getDewpoint(temperatureC, humidityPercent);   // calc Dew Point
    heatIndexF      = getHeatIndex(temperatureF, humidityPercent);  // calc Heat Index
    minutes2MED     = med();                                        // mins to Minimal Erythemal Dose (MED)
    
    char mmed[40];
    char uvix[40] = "";
    char uvbr[40] = "";
    char baro[40] = "";
    char temp[40] = "";
    char humd[40] = "";
    char htix[40] = "";
    char dwpt[40] = "";

#if DEBUG
    Serial.println("sprintfs  u8g2.drawUTF8");
#endif
    
    sprintf(mmed, "%d min",  (int)minutes2MED);
    sprintf(uvix, "%d.%02d", (int)uvIndex,      (int)(uvIndex*100)%100);
    sprintf(uvbr, "%d.%02d", (int)uvBReading,   (int)(uvBReading*100)%100);
    sprintf(htix, "%d°F",    (int)heatIndexF);
    sprintf(dwpt, "%d°F",    (int)dewpointF);
    sprintf(temp, "%d°F",    (int)temperatureF);
    sprintf(humd, "%d%%",    (int)humidityPercent);
    sprintf(baro, "%dpa",    (int)barameterKPA);

    u8g2.drawUTF8(COLUMN_2 - 7, MIN2MED_LINE, mmed); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, UVI_LINE, uvix);
    u8g2.drawBox(SPRITE0_X, UVI_LINE - 4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, UVB_LINE, uvbr);
    u8g2.drawBox(SPRITE0_X, UVB_LINE - 4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, HEATINDEX_LINE, htix);
    u8g2.drawBox(SPRITE0_X, HEATINDEX_LINE - 4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, DEWPOINT_LINE, dwpt);
    u8g2.drawBox(SPRITE0_X, DEWPOINT_LINE - 4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, TEMPERATURE_LINE, temp);
    u8g2.drawBox(SPRITE0_X, TEMPERATURE_LINE -4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, HUMIDITY_LINE, humd);
    u8g2.drawBox(SPRITE0_X, HUMIDITY_LINE -4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.drawUTF8(COLUMN_2, BAROMETER_LINE, baro);
    u8g2.drawBox(SPRITE0_X, BAROMETER_LINE -4, 4, 4); delay(50); u8g2.sendBuffer();

    u8g2.setDrawColor(0); u8g2.drawBox(SPRITE0_X, UVI_LINE -4, 4, BAROMETER_LINE - 4);
    delay(50); u8g2.sendBuffer();
    u8g2.setDrawColor(1);

#if DEBUG
    Serial.print("MED          ");   Serial.println(minutes2MED);
    Serial.print("UV Index     ");   Serial.println(uvIndex);
    Serial.print("UV B Reading ");   Serial.println(uvBReading);
    Serial.print("Heat Index   ");   Serial.println(htix);
    Serial.print("Dewpoint     ");   Serial.println(dwpt);
    Serial.print("Temperature  ");   Serial.println(temp);
    Serial.print("Humdity      ");   Serial.println(humd);
    Serial.print("Pressure     ");   Serial.println(baro);
#endif

}
