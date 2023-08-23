e#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <DFRobot_SGP40.h>
#include <PMS.h>
#include <Animation.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define SCREEN_WIDTH 128 // Szerokosc wyswiatlacza OLED
#define SCREEN_HEIGHT 64 // Wysokosc wyswietlacza OLED
#define OLED_RESET -1 // Pin do resetu wyswietlacza
#define SCREEN_ADRESS 0x3c // Adres wyswietlacza OLED
#define IMAGE_WIDTH 128 // Szerokosc wyswietlanych bitmap
#define IMAGE_HEIGHT 64 // Wysokosc wyswietlanych bitmap
#define X_START_POS 0 // Pozycja poczatkowa X kursora do wyswietlania bitmap
#define Y_START_POS 0 // Pozycja poczatkowa Y kursora do wyswietlania bitmap
#define DELAY_FRAMES 25 // Opoznienie miedzy klatkami animacji
#define ANIMATION_COLOR 1 // Kolor animowanych bitmap

#define DEVICE "ESP8266"

#define MILLIS_IN_DAY 86400000

#define WIFI_SSID "Redmi 9C NFC" // WiFi AP SSID
#define WIFI_PASSWORD "skomplikowanehaslo321" // WiFi password

#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "9-iOqK17HnnCQoDO7ndsywITnfdPe9mB990lTq-njPlGA2NfS32QDUobs8eRvIyVcIYTopFcEukUieBq8KeFSQ=="
#define INFLUXDB_ORG "37a49885c159024b"
#define INFLUXDB_BUCKET "Temperatura"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3" // Zdefiniowanie strefy czasowej dla Warszawy


ESP8266WiFiMulti wifiMulti;

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert); // Inicjalizacja klienta bazy InfluxDB

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Inicjalizacja wyswietlacza

Animation Animation(X_START_POS, Y_START_POS, SCREEN_WIDTH, SCREEN_HEIGHT, DELAY_FRAMES, ANIMATION_COLOR); // Inicjalizacja biblioteki animacji


PMS pms(Serial);
PMS::DATA data;

DHT_Unified dht(13, DHT11); //Inicjalizacjia pinu 13 do DHT11

Adafruit_BMP280 bme; // Inicjalizacja BMP280 I2C

DFRobot_SGP40    mySgp40; // Inicjalizacjia SGP40
bool starttime = false;
unsigned long czas_aktualny;
unsigned long czas_start;
bool firstRun = true;
bool avgPM = false;

unsigned int counter=0, cycles = 0;
float PM1 = 0, PM25 = 0, PM10 = 0;
float PM1set=0, PM25set=0, PM10set=0;
float PM25avg, PM10avg;
float PM25tab[900], PM10tab[900];
//Wspolczynniki do obliczen AQI
float PM25CLOW[8] = {0, 12.1, 35.5, 55.5, 150.5, 250.5, 350.5};
float PM25CHIGH[8] = {12, 35.4, 55.4, 150.4, 250.4, 350.4, 500.4};
float PM10CLOW[8] = {0, 55, 155, 255, 355, 425, 505};
float PM10CHIGH[8] = {54, 154, 254, 354, 424, 504, 604};
float AQIILOW[8] = {0, 51, 101, 151, 201, 301, 401};
float AQIIHIGH[8] = {50, 100, 150, 200, 300, 400, 500};
short AQI25set, AQI10set;
int AQIPM25, AQIPM10;

float temperatureBMP;
float humidityDHT;
float pressureBMP;
unsigned int vocSGP;
Point sensor("AirQuality");

void setup() {

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADRESS)) {
    while(true); // Nieskonczona petla do oczekiwania
    display.display();
    display.clearDisplay();
    display.invertDisplay(true); // Inwersja wyswietlacza o 180°
  }
  
  display.setTextSize(3); // Domyslna wielkosc tekstu
  display.setTextColor(SSD1306_WHITE); // Domyslny kolor tekstu
  
  Serial.begin(9600); // Inicjalizacja portu szeregowego do debuggowania
  Animation.gear();
  while(mySgp40.begin(10000) != true){ // Oczekiwanie na skonfigurowanie polaczenia z SGP40
    Animation.sgp40Error(); // Wyświetlenie komunikatu o problemie z SGP40
    delay(1000);
  }
  
  dht.begin();
  sensor_t Sensor;
    
  if (!bme.begin()){
    Animation.bmp280Error(); // Wyświetlenie komunikatu o problemie z BMP280
    delay(1000);
  }
  
  Animation.gear();
  Animation.network();
  
  WiFi.mode(WIFI_STA);  //Inicjalizacja Wifi
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);  //Logowanie do sieci WiFi

  while (wifiMulti.run() != WL_CONNECTED) // Oczekiwanie na nawiązanie połączenia
    delay(100);
  
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov"); //Synchronizacja czasu

  // Sprawdzenie polaczenia z baza danych
  if (client.validateConnection())
    Animation.connectionSuccess();  // Wyświetlenie komunikatu o poprawnym nawiązaniu połączenia z bazą InfluxDB
   else
    Animation.connectionFailed();  // Wyświetlenie komunikatu o błędzie przy nawiązaniu połączenia z bazą InfluxDB 
    
   sensor.addTag("device", DEVICE);

  pms.activeMode();    // PMS tryb aktywny

  Animation.network();
  
}

void loop() {
  
  display.clearDisplay();
  sensor.clearFields();

  // Pomiar temperatury.
  sensors_event_t event;
  Animation.temperature();
  temperatureBMP = bme.readTemperature();
  
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(10,20);
  display.print(temperatureBMP,1);
  display.print((char)247);
  display.println("C");
  display.display();
  sensor.addField("tempBMP", temperatureBMP); //dodanie temperatury do bazy
  delay(7500);

  // Pomiar wilgotności
  Animation.humidity();
  dht.humidity().getEvent(&event);
  
  if (isnan(event.relative_humidity)){
     Animation.dht11Error();
    delay(1000);
  }
  else{
     humidityDHT = event.relative_humidity;
  }

  display.clearDisplay();
  display.setTextSize(5);
  display.setCursor(20,15);
  display.print(humidityDHT,0);
  display.println("%");
  display.display();
  sensor.addField("humDHT", humidityDHT); //dodanie wilgotnosci do bazy
  delay(7500);

  // Pomiar ciśnienia
  Animation.pressure();
  pressureBMP = (bme.readPressure()/100);
  display.setTextSize(3);
  display.clearDisplay();
  display.setCursor(0,20);
  display.print(pressureBMP, 0);
  display.println("hPa");
  display.display();
  
  sensor.addField("pressBMP", pressureBMP); //dodanie cisnienia do bazy
  delay(7500);

  //korekta LZO wzgledem wilgotnosci i temperatury
   mySgp40.setRhT(/*wilgotnosc = */ int(event.relative_humidity), /*temperatura = */ int(bme.readTemperature()));
  
   Animation.voc();
   vocSGP = mySgp40.getVoclndex();
   display.setTextSize(5);
   display.clearDisplay();

   if(vocSGP < 100){ // Wartość LZO jest dobra, wyświetl uśmiechniętą buźkę
    display.setCursor(32,16);
    display.print(vocSGP);
    display.display();
    delay(7500);
    Animation.smileface();
    Animation.smileface();
   }
   if(vocSGP >= 100){ // Wartość LZO jest dobra, wyświetl uśmiechniętą buźkę, wyśrodkowanie tekstu dla 3cyfrowej wartości
    display.setCursor(20,15);
    display.print(vocSGP);
    display.display();
    delay(5000);
    if (vocSGP <= 100){
      Animation.smileface();
      Animation.smileface();
    }
    else if (vocSGP > 100 && vocSGP <= 300){ // Wartość LZO w normie, wyświetl buźkę z neutralną minką 
      Animation.neutralface();
      Animation.neutralface();
    }
    else{ // Wartość LZO przekroczyła 300, zła jakość powietrza, wyświetl smutną buźkę
      Animation.sadface();
      Animation.sadface();
    }
    delay(3500);
   }
   
   display.setTextSize(3);
   sensor.addField("vocSGP40", vocSGP); //dodanie LZO do bazy
    
    pms.requestRead(); // Rozpocznij pomiar PM
    delay(1000);
    
    for(int i = 0; i < 20; i++){ // Usrednij 20 odczytow PM do jednego
      if(pms.readUntil(data)){
        PM1set += data.PM_AE_UG_1_0;
        PM25set += data.PM_AE_UG_2_5;
        PM10set += data.PM_AE_UG_10_0;
    }
    }

    // Oblicz usredniona wartosc z 20 pomiarow PM
    PM1 = PM1set/20;
    PM25 = PM25set/20;
    PM10 = PM10set/20;

    PM1set = 0;
    PM25set = 0;
    PM10set = 0;

    // Rozpocznij pomiar czasu dla wyliczenia indeksu AQI dla pierwszego cyklu
    if(firstRun){ 
      czas_start = millis();
      firstRun = false;
    }
    
   // Wpisanie wartości do tablicy    
    PM25tab[counter] = PM25;
    PM10tab[counter] = PM10;
    counter++;

    //Wybierz aktualną liczbe ukonczonych cykli, po 24h bez zmian
    cycles = max(counter, cycles);
    czas_aktualny = millis();
    if(((czas_aktualny - czas_start) > MILLIS_IN_DAY)  || avgPM){

      // Po 24h rozpocznij wypelnianie tablicy od poczatku
      if((czas_aktualny - czas_start) > MILLIS_IN_DAY){
        counter = 0;
        czas_start = millis();
      }

      // Flaga do wyświetlania AQI na OLED     
      avgPM = true;

      // Oblicz wartosc srednia stezenia PM
      for (int i = 0; i < cycles; i++){
      PM25avg += PM25tab[i];
      PM10avg += PM10tab[i];
    }
    PM25avg /= cycles;
    PM10avg /= cycles;
    
   // Przypisz odpowiednia flage w zaleznosci od stezenia PM2.5
   if(PM25avg <= 12)
    AQI25set = 0;
   if(PM25avg > 12 && PM25avg <= 35.4)
    AQI25set = 1;
   if(PM25avg > 35.4 && PM25avg <= 55.4)
    AQI25set = 2; 
   if(PM25avg > 55.4 && PM25avg <= 150.4)
    AQI25set = 3;
   if(PM25avg > 150.4 && PM25avg <= 250.4)
    AQI25set = 4;
   if(PM25avg > 250.4 && PM25avg <= 350.4)
    AQI25set = 5;
   if(PM25avg > 350.4)
    AQI25set = 6;
    
   // Oblicz wspolczynnik AQI dla PM2.5
   AQIPM25 = ((AQIIHIGH[AQI25set] - AQIILOW[AQI25set])/(PM25CHIGH[AQI25set] - PM25CLOW[AQI25set]))*(PM25avg - PM25CLOW[AQI25set])+AQIILOW[AQI25set];

  // Przypisz odpowiednia flage w zaleznosci od stezenia PM10
   if(PM10avg <= 54)
    AQI10set = 0;
   if(PM10avg > 54 && PM10avg <= 154)
    AQI10set = 1;
   if(PM10avg > 154 && PM10avg <= 254)
    AQI10set = 2; 
   if(PM10avg > 254 && PM10avg <=354)
    AQI10set = 3;
   if(PM10avg > 354 && PM10avg <= 424)
    AQI10set = 4;
   if(PM10avg > 424 && PM10avg <= 504)
    AQI10set = 5;
   if(PM25avg > 504)
    AQI10set = 6;

   // Oblicz wspolczynnik AQI dla PM2.5  
   AQIPM10 = ((AQIIHIGH[AQI10set] - AQIILOW[AQI10set])/(PM10CHIGH[AQI10set] - PM10CLOW[AQI10set]))*(PM10avg - PM10CLOW[AQI10set])+AQIILOW[AQI10set];

   PM25avg = 0;
   PM10avg = 0;
    }

   Animation.pm();
   delay(1000);
  
   display.clearDisplay();
   display.setTextSize(3);
   display.setCursor(30, 2);
   display.println(PM1,1);
   display.setTextSize(2);
   display.setCursor(48,50);
   display.print("PM1");
   display.display();
   delay(6000);
  //Dodanie PM1 do bazy
   sensor.addField("pm1", PM1);

   Animation.pm();
   delay(1000);
   display.clearDisplay();
   display.setTextSize(3);
   display.setCursor(30, 2);
   display.println(PM25,1);
   display.setTextSize(2);
   display.setCursor(40,50);
   display.print("PM2,5");
   display.display();
   delay(3000);
   //Dodanie PM2.5 od bazy
   sensor.addField("pm2.5", PM25);
   if(avgPM){
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(27, 20);
    display.println("AQI");
    display.display();
    delay(1500);
    display.clearDisplay();
    display.setTextSize(5);
    display.setCursor(20,15);
    display.println(AQIPM25);
    display.display();
    delay(2500); 
    if(AQIPM25<51){
      Animation.smileface();
      Animation.smileface();
    }
    else if(AQIPM25 >= 51 && AQIPM25 < 151){
      Animation.neutralface();
      Animation.neutralface();
    }
    else if(AQIPM25 >= 151){
      Animation.sadface();
      Animation.sadface();
    }
    delay(3500); 
    }
    else{
      delay(9500);
    }

   Animation.pm();
   delay(1000);
   //Dodanie PM10 do bazy
   sensor.addField("pm10", PM10);

   display.clearDisplay();
   display.setTextSize(3);
   display.setCursor(30, 2);
   display.println(PM10,1);
   display.setTextSize(2);
   display.setCursor(40,50);
   display.print("PM10");
   display.display();
   delay(3000);
    if(avgPM){
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(27, 20);
    display.println("AQI");
    display.display();
    delay(1500);
    display.clearDisplay();
    display.setTextSize(5);
    display.setCursor(20,15);
    display.println(AQIPM10);
    display.display();
    delay(2500);
    if(AQIPM10<51){
      Animation.smileface();
      Animation.smileface();
    }
    else if(AQIPM10 >= 51 && AQIPM10 < 151){
      Animation.neutralface();
      Animation.neutralface();
    }
    else if(AQIPM10 >= 151){
      Animation.sadface();
      Animation.sadface();
    }
    delay(3500);      
   }
   else{
    delay(9500);
   }
   sensor.addField("pm10", PM10);

   //Wyslij wszystkie wartosc do bazy InfluxDB
   sensor.toLineProtocol();
   client.writePoint(sensor);

   //Sprawdz WiFi i ponownie polacz w razie potrzeby
   if (wifiMulti.run() != WL_CONNECTED) {
      Animation.connectionFailed();
   }
   //Błąd network timeout
    if (!client.writePoint(sensor)) {
      Animation.influxDBError();  //Wyświetlenie komunikatu o problemie z zapisem do bazy InfluxDB 

    }
   

}
