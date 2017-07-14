#include "Wire.h"
#include "SPI.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_SPIFlash_FatFs.h"
#include "Adafruit_SharpMem.h"
#include "Adafruit_VEML6070.h"
#include "SimpleTimer.h"
#include "RTCZero.h"
#include "Adafruit_GFX.h"

#define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV
#define FLASH_SS       SS1                              // Flash chip SS pin.
#define FLASH_SPI_PORT SPI1                             // What SPI port is Flash on?
Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI
Adafruit_W25Q16BV_FatFs fatfs(flash);
#define FILE_NAME      "data.csv"

#define BME280_ADDRESS 0x76
unsigned long int hum_raw,temp_raw,pres_raw;
signed long int t_fine;
uint16_t dig_T1;
 int16_t dig_T2;
 int16_t dig_T3;
uint16_t dig_P1;
 int16_t dig_P2;
 int16_t dig_P3;
 int16_t dig_P4;
 int16_t dig_P5;
 int16_t dig_P6;
 int16_t dig_P7;
 int16_t dig_P8;
 int16_t dig_P9;
 int8_t  dig_H1;
 int16_t dig_H2;
 int8_t  dig_H3;
 int16_t dig_H4;
 int16_t dig_H5;
 int8_t  dig_H6;
Adafruit_VEML6070 uv = Adafruit_VEML6070();

#define SCK 24
#define MOSI 23
#define CS 13
Adafruit_SharpMem display(SCK, MOSI, CS);      // 96 x 96
#define BLACK 0
#define WHITE 1

int FITZ          = 13;                         // Fitzpatrick Score 0-32
int SPF           = 30;                         // SPF Applied       0-100
bool H2O          = 0;                          // On Water          bool
bool SNOW         = 0;                          // Snow Present      bool 
double UVINDEX    = 6.00;                       // UV Index          0.00-26.00
double SEA_LVL_MB = 1013.25;                    // sea level reference reading (mb)
double BARO_MB    = 3000.00;                    // Barometer (Pa)    0-1,086
double ALT_M      = 1000.00;                    // Altitude (m)      0-11,000
double TEMP_C; 
double TEMP_F;
double BARO_PA;
double BARO_HG;     
double HUMD; 
double DEWPOINT;
double HEATIX;
int RECORDS       = 0;                          // Number of SD records written

#define SPEAKER         A1
#define VBAT_PIN        1
#define BUTTON          5
#define BLACK           0                                       // For Sharp
#define WHITE           1                                       // For Sharp
#define CHAR_W          6
#define CHAR_H          8

SimpleTimer timer;
RTCZero rtc;

double Time2Burn () {   
    double uvi_f =  (UVINDEX * (H2O * 1.5)) +
                    (UVINDEX * (SNOW * 1.85)) +
                    (UVINDEX * (ALT_M * 1.2));
                    
    double t2b_b =  (-3.209E-5 * pow(FITZ, 5)) +
                    (2.959E-3 * pow(FITZ, 4)) -
                    (0.103 * pow(FITZ, 3)) +
                    (1.664* pow(FITZ, 2)) +
                    (3.82 * FITZ) + 
                    34.755;
    return (t2b_b / uvi_f) * SPF;
}
double c2f(double celsius) { 
    return ((celsius * 9) / 5) + 32; 
}
double pa2hg(double pascals) { 
    return (pascals * 0.000295333727); 
}
double dewPoint(double cel, int hum) {
    double Td = (237.7 * ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01))) / 
                (17.271 - ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01)));
    return Td;
}
double heatIndex(double tempF, double humidity) {
    if (tempF < 80 || humidity < 40) { double pass = tempF; return pass;}
    double c1=-42.38, c2=2.049, c3=10.14, c4=-0.2248, c5=-6.838e-3;
    double c6=-5.482e-2, c7=1.228e-3, c8=8.528e-4, c9=-1.99e-6  ;
    double t = tempF;
    double r = humidity;
    double a = ((c5 * t) + c2) * t + c1;
    double b = ((c7 * t) + c4) * t + c3;
    double c = ((c9 * t) + c8) * t + c6;
    double rv = (c * r + b) * r + a;
    return rv;
}
void GetReadings() {
    double temp_act = 0.0, press_act = 0.0,hum_act=0.0;
    signed long int temp_cal;
    unsigned long int press_cal,hum_cal;
    readData();
    temp_cal = calibration_T(temp_raw);
    press_cal = calibration_P(pres_raw);
    hum_cal = calibration_H(hum_raw);  
    UVINDEX     = uv.readUV();
    UVINDEX     /= 100.0;
    TEMP_C      = (double)temp_cal / 100.0;             // temp C
    TEMP_F      = c2f(TEMP_C);
    BARO_PA     = (double)press_cal / 100.0;            // baro pa
    BARO_MB     = BARO_PA / 100;
    BARO_HG     = pa2hg(BARO_PA);  
    float atmospheric = BARO_PA / 100.0F;
    ALT_M       = 44330.0 * (1.0 - pow(atmospheric / SEA_LVL_MB, 0.1903));    
    HUMD        = hum_act = (double)hum_cal / 1024.0;   // humd rh%
    DEWPOINT    = dewPoint(TEMP_C, HUMD);
    HEATIX      = heatIndex(TEMP_F, HUMD);
    Serial.print("UVi ");  Serial.print(UVINDEX); Serial.print(" | ");
    Serial.print("MED ");  Serial.print(Time2Burn()); Serial.print("secs"); Serial.print(" | ");
    Serial.print("Alt ");  Serial.print(ALT_M); Serial.print("m"); Serial.print(" | ");
    Serial.print("Temp "); Serial.print(TEMP_C); Serial.print("c"); Serial.print(" | ");
    Serial.print("Temp "); Serial.print(TEMP_F); Serial.print("f"); Serial.print(" | ");
    Serial.print("Baro "); Serial.print(BARO_PA); Serial.print("pa"); Serial.print(" | ");
    Serial.print("Baro "); Serial.print(BARO_MB); Serial.print("mb"); Serial.print(" | ");
    Serial.print("Baro "); Serial.print(BARO_HG); Serial.print("hg"); Serial.print(" | ");
    Serial.print("Humd "); Serial.print(HUMD); Serial.print("%"); Serial.print(" | ");
    Serial.print("DwPt "); Serial.print(DEWPOINT); Serial.print("f"); Serial.print(" | ");
    Serial.print("HtIx "); Serial.print(HEATIX); Serial.print("f"); Serial.println(" | ");
}

void setup() {
    pinMode(SPEAKER, OUTPUT);
    Wire.begin();
    uv.begin(VEML6070_1_T);                                     // pass in the integration time constant
    display.begin();
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.refresh();
    flash.begin(FLASH_TYPE);
    displayWrite("Flash chip JEDEC ID: 0x"); 
    fatfs.begin();
    File dataFile = fatfs.open(FILE_NAME, FILE_WRITE);
    dataFile.print("date  ,");
    dataFile.print("time  ,");
    dataFile.print("batt,");  
    dataFile.print("uvi,");
    dataFile.print("alt ,");
    dataFile.println("t2b  ");    
    dataFile.close();
    uint8_t osrs_t = 1;                                         // Temperature oversampling x 1
    uint8_t osrs_p = 1;                                         // Pressure oversampling x 1
    uint8_t osrs_h = 1;                                         // Humidity oversampling x 1
    uint8_t mode = 3;                                           // Normal mode
    uint8_t t_sb = 5;                                           // Tstandby 1000ms
    uint8_t filter = 0;                                         // Filter off 
    uint8_t spi3w_en = 0;                                       // 3-wire SPI Disable
    uint8_t ctrl_meas_reg = (osrs_t << 5) | (osrs_p << 2) | mode;
    uint8_t config_reg    = (t_sb << 5) | (filter << 2) | spi3w_en;
    uint8_t ctrl_hum_reg  = osrs_h;
    writeReg(0xF2,ctrl_hum_reg);
    writeReg(0xF4,ctrl_meas_reg);
    writeReg(0xF5,config_reg);
    readTrim();
    byte seconds, minutes, hours, days, months, years;
    char s_month[5];
    int tmonth, tday, tyear, thour, tminute, tsecond;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    rtc.begin();    
    sscanf(__DATE__, "%s %d %d", s_month, &tday, &tyear);
    sscanf(__TIME__, "%d:%d:%d", &thour, &tminute, &tsecond);
    tmonth = (strstr(month_names, s_month) - month_names) / 3;
    years = tyear - 2000; months = tmonth + 1; days = tday; 
    hours = thour; minutes = tminute; seconds = tsecond;
    rtc.setTime(hours, minutes, seconds); 
    rtc.setDate(days, months, years);
    pinMode(VBAT_PIN, INPUT);
    pinMode(BUTTON, INPUT_PULLUP);    
    timer.setInterval(20000, FileWrite);          // write readings to SD every 2 seconds
    timer.setInterval(2000, GetReadings);           // get sensor readings every 2 seconds
    chime(1);
}

int state       = HIGH;
int reading;
int previous    = LOW;
long time       = 0;
long debounce   = 200;

void loop() {
    displayWrite("");
//    checkButton();
    timer.run();                                        // Runs timed events
}

void FileWrite() {
    char SDDate[6]; sprintf(SDDate, "%02d%02d%02d", rtc.getYear(),rtc.getMonth(),rtc.getDay());
    char SDTime[6]; sprintf(SDTime, "%02d%02d%02d", rtc.getHours(),rtc.getMinutes(), rtc.getSeconds());
    File dataFile = fatfs.open(FILE_NAME, FILE_WRITE);
    dataFile.print(SDDate);                                   datafile.print(",");
    dataFile.print(SDTime);                                   datafile.print(",");
    dataFile.print((analogRead(VBAT_PIN) * 6.6)/1024, 1);     datafile.print(",");  
    dataFile.print(uv.readUV());                              datafile.print(",");       
    dataFile.print(bme.readAltitude(SEA_LVL_MB),1);           datafile.print(",");
    dataFile.println(Time2Burn());
    dataFile.close();
    RECORDS++;
}


void checkButton() {                                    // was button pressed
    reading = digitalRead(BUTTON);    
    if (reading == HIGH && previous == LOW && millis() - time > debounce) {    
        if (state == HIGH) state = LOW; 
        else state = HIGH; 
        time = millis();    
    }   
    previous = reading; 
    if (state == HIGH) { 
        displayWrite(""); 
    }
    else { 
        display.clearDisplay(); 
        display.refresh();
    }    
}

void displayWrite(String statmsg) {

    Serial.println("displayWrite_1");

    display.clearDisplay();
    if (statmsg != "") {                                            // if statmsg prsent display it
        display.setCursor(0, display.height()/2);   
        display.print(statmsg);
        display.refresh();
        return;
    }
    header(((analogRead(VBAT_PIN) * 6.6)/1024));                        // go print header
    body();                                                             // go print body
    display.refresh();
}

void header(float _battery) {
    char cTime[6]; 
    sprintf(cTime, "%02d%02d%02d", rtc.getHours(),rtc.getMinutes(),rtc.getMinutes());
    display.setCursor(0, 0);
    display.print(cTime);
    
    int BATTTEXT_WIDTH      = 4 * CHAR_W;
    int BATTICON_WIDTH      = 3 * CHAR_W;
    int BATTTEXT_STARTX     = display.width() - (BATTTEXT_WIDTH + BATTICON_WIDTH);
    int BATTTEXT_STARTY     = 0;
    int BATTICON_STARTX     = display.width() - BATTICON_WIDTH;
    int BATTICON_STARTY     = 0;
    int BATTICON_BARWIDTH3  = ((BATTICON_WIDTH) / 3);
    display.setCursor(BATTTEXT_STARTX, BATTTEXT_STARTY);
    display.print(_battery, 1);
    display.println("v");
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX, BATTICON_STARTY + 1,
                      BATTICON_STARTX, BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY + 6,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY + 6, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 1, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 1, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX + BATTICON_WIDTH - 1,
                      BATTICON_STARTY + 2, BATTICON_STARTX + 
                      BATTICON_WIDTH - 1, BATTICON_STARTY + 4, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 6, TEXT_COLOR);     
    if (_battery > 4.26F) {                                 // draw bars
        display.fillRect(BATTICON_STARTX + 2, BATTICON_STARTY + 2, // x , y
        BATTICON_BARWIDTH3 * 3, 3, TEXT_COLOR);                     // w, h, color        
    }
    else if ((_battery <= 4.26F) && (_battery >= 4.1F)) {
        for (uint8_t i = 0; i < 3; i++) {
            display.fillRect(BATTICON_STARTX + 2 + (i * BATTICON_BARWIDTH3),
            BATTICON_STARTY + 2, BATTICON_BARWIDTH3 - 1, 3, TEXT_COLOR);
        }
    }
    else if ((_battery < 4.1F) && (_battery >= 3.8F)) {    // 2 bars
        for (uint8_t i = 0; i < 2; i++) {                   
            display.fillRect(BATTICON_STARTX + 2 + (i * BATTICON_BARWIDTH3),
            BATTICON_STARTY + 2, BATTICON_BARWIDTH3 - 1, 3, TEXT_COLOR);
        }
    }
    else if ((_battery < 3.8F) && (_battery >= 3.4F)) {     // 1 bar
            display.fillRect(BATTICON_STARTX + 2,
            BATTICON_STARTY + 2, BATTICON_BARWIDTH3 - 1, 3, TEXT_COLOR);
    }
}

void body() {    
    display.fillRect(0, 9, (UVINDEX * 6) + 3, 8, TEXT_COLOR);
    display.setCursor(0, 18); 
    display.print("UVi ");
    display.print(UVINDEX);
    display.setCursor(0, 27);
    display.print("MED ");
    display.print(Time2Burn());
    display.print("secs");
    display.setCursor(0, 36);
    display.print("Alt ");
    display.print(ALT_M);
    display.print("m");
    display.refresh();
}

void chime(int times) {
    int toneH = 1911;
    int toneL= 506;
    for (int ii=0; ii<times; ii++) {
        int melody[] = {toneL, toneH};
        int rhythm[] = {18, 18};
        long vel = 20000;
        for (int i=0; i<2; i++) {
            int note = melody[i];
            int tempo = rhythm[i]; 
            long tvalue = tempo * vel;
            long tempo_progress = 0;
            while (tempo_progress < tvalue) {
                digitalWrite(SPEAKER, HIGH);
                delayMicroseconds(note / 2);
                digitalWrite(SPEAKER, LOW);
                delayMicroseconds(note / 2);  
                tempo_progress += note;
            }
        delayMicroseconds(1000);
        } 
    }
}

void readTrim() {
    uint8_t data[32],i=0;
    Wire.beginTransmission(BME280_ADDRESS);
    Wire.write(0x88);
    Wire.endTransmission();
    Wire.requestFrom(BME280_ADDRESS,24);
    while(Wire.available()){
        data[i] = Wire.read();
        i++;
    }    
    Wire.beginTransmission(BME280_ADDRESS);
    Wire.write(0xA1);
    Wire.endTransmission();
    Wire.requestFrom(BME280_ADDRESS,1);
    data[i] = Wire.read();
    i++;
    Wire.beginTransmission(BME280_ADDRESS);
    Wire.write(0xE1);
    Wire.endTransmission();
    Wire.requestFrom(BME280_ADDRESS,7);
    while(Wire.available()){
        data[i] = Wire.read();
        i++;    
    }
    dig_T1 = (data[1] << 8) | data[0];
    dig_T2 = (data[3] << 8) | data[2];
    dig_T3 = (data[5] << 8) | data[4];
    dig_P1 = (data[7] << 8) | data[6];
    dig_P2 = (data[9] << 8) | data[8];
    dig_P3 = (data[11]<< 8) | data[10];
    dig_P4 = (data[13]<< 8) | data[12];
    dig_P5 = (data[15]<< 8) | data[14];
    dig_P6 = (data[17]<< 8) | data[16];
    dig_P7 = (data[19]<< 8) | data[18];
    dig_P8 = (data[21]<< 8) | data[20];
    dig_P9 = (data[23]<< 8) | data[22];
    dig_H1 = data[24];
    dig_H2 = (data[26]<< 8) | data[25];
    dig_H3 = data[27];
    dig_H4 = (data[28]<< 4) | (0x0F & data[29]);
    dig_H5 = (data[30] << 4) | ((data[29] >> 4) & 0x0F);
    dig_H6 = data[31];   
}
void writeReg(uint8_t reg_address, uint8_t data) {
    Wire.beginTransmission(BME280_ADDRESS);
    Wire.write(reg_address);
    Wire.write(data);
    Wire.endTransmission();    
}
void readData() {
    int i = 0;
    uint32_t data[8];
    Wire.beginTransmission(BME280_ADDRESS);
    Wire.write(0xF7);
    Wire.endTransmission();
    Wire.requestFrom(BME280_ADDRESS,8);
    while(Wire.available()){
        data[i] = Wire.read();
        i++;
    }
    pres_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    hum_raw  = (data[6] << 8) | data[7];
}
signed long int calibration_T(signed long int adc_T) {    
    signed long int var1, var2, T;
    var1 = ((((adc_T >> 3) - ((signed long int)dig_T1<<1))) * ((signed long int)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((signed long int)dig_T1)) * ((adc_T>>4) - ((signed long int)dig_T1))) >> 12) * ((signed long int)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T; 
}
unsigned long int calibration_P(signed long int adc_P) {
    signed long int var1, var2;
    unsigned long int P;
    var1 = (((signed long int)t_fine)>>1) - (signed long int)64000;
    var2 = (((var1>>2) * (var1>>2)) >> 11) * ((signed long int)dig_P6);
    var2 = var2 + ((var1*((signed long int)dig_P5))<<1);
    var2 = (var2>>2)+(((signed long int)dig_P4)<<16);
    var1 = (((dig_P3 * (((var1>>2)*(var1>>2)) >> 13)) >>3) + ((((signed long int)dig_P2) * var1)>>1))>>18;
    var1 = ((((32768+var1))*((signed long int)dig_P1))>>15);
    if (var1 == 0) {
        return 0;
    }    
    P = (((unsigned long int)(((signed long int)1048576)-adc_P)-(var2>>12)))*3125;
    if(P<0x80000000) {
       P = (P << 1) / ((unsigned long int) var1);   
    }
    else {
        P = (P / (unsigned long int)var1) * 2;    
    }
    var1 = (((signed long int)dig_P9) * ((signed long int)(((P>>3) * (P>>3))>>13)))>>12;
    var2 = (((signed long int)(P>>2)) * ((signed long int)dig_P8))>>13;
    P = (unsigned long int)((signed long int)P + ((var1 + var2 + dig_P7) >> 4));
    return P;
}
unsigned long int calibration_H(signed long int adc_H) {
    signed long int v_x1;    
    v_x1 = (t_fine - ((signed long int)76800));
    v_x1 = (((((adc_H << 14) -(((signed long int)dig_H4) << 20) - (((signed long int)dig_H5) * v_x1)) + 
              ((signed long int)16384)) >> 15) * (((((((v_x1 * ((signed long int)dig_H6)) >> 10) * 
              (((v_x1 * ((signed long int)dig_H3)) >> 11) + ((signed long int) 32768))) >> 10) + (( signed long int)2097152)) * 
              ((signed long int) dig_H2) + 8192) >> 14));
   v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((signed long int)dig_H1)) >> 4));
   v_x1 = (v_x1 < 0 ? 0 : v_x1);
   v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);
   return (unsigned long int)(v_x1 >> 12);   
}

