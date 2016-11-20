#include <EtherCard.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>

#define APIKEY "O4AM5UMQ6AXOE9GN" // put your key here
#define ethCSpin 48 // put your CS/SS pin here.

#define DHTPORT 7
#define DHTTYPE DHT22
#define pinMQ7 A0
#define pinMQ135 A1
#define pinPM25 3
#define measurePinGP2Y A5
#define ledPinGP2Y 12

DHT dht(DHTPORT, DHTTYPE);
Adafruit_BMP085 bmp;

unsigned long sampletime_ms = 30000;
unsigned long starttime;
unsigned long duration = 0;
unsigned long elapsedtime = 0;

unsigned int samplingTime = 280;
unsigned int deltaTime = 40;
unsigned int sleepTime = 9680;


// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
const char website[] PROGMEM = "api.thingspeak.com";
byte Ethernet::buffer[700];
uint32_t timer;
Stash stash;
byte session;

//timing variable
int res = 100; // was 0

void initialize_ethernet(void){  
  for(;;){ // keep trying until you succeed 

    if (ether.begin(sizeof Ethernet::buffer, mymac, ethCSpin) == 0){ 
      Serial.println( F("Failed to access Ethernet controller"));
      continue;
    }
    
    if (!ether.dhcpSetup()){
      Serial.println(F("DHCP failed"));
      continue;
    }

    ether.printIp("IP:  ", ether.myip);
    ether.printIp("GW:  ", ether.gwip);  
    ether.printIp("DNS: ", ether.dnsip);  

    if (!ether.dnsLookup(website))
      Serial.println(F("DNS failed"));

    ether.printIp("SRV: ", ether.hisip);

    //reset init value
    res = 180;
    break;
  }
}

void setup () {
  Serial.begin(9600);
  Serial.println("[ThingSpeak + sensors]");

  //Initialize Ethernet
  initialize_ethernet();

  //Start the dht and bmp sensors
  dht.begin();
  //bmp.begin();

  //Start the PM25 and GP2Y sensors
  pinMode(pinPM25,INPUT);
  starttime=millis();
  
  pinMode(ledPinGP2Y,OUTPUT);
}


void loop () { 
  //if correct answer is not received then re-initialize ethernet module
  if (res > 220){

  initialize_ethernet();

  }
  
  res = res + 1;
  
  ether.packetLoop(ether.packetReceive());
  
  //200 res = 10 seconds (50ms each res)
  if (res == 200) {
    
  //----------DHT22----------
  float humDHT22 = dht.readHumidity();
  float tempDHT22 = dht.readTemperature();

  // Compute heat index in Celsius (isFahreheit = false)
  float heatDHT22 = dht.computeHeatIndex(tempDHT22, humDHT22, false);

  //----------BMP180----------
  float presBMP180=(bmp.readPressure()/100.0);//this is for pressure in hectoPascal
  float tempBMP180=(bmp.readTemperature());

  //----------MQ-7----------
  float gasMQ7=analogRead(pinMQ7);

  //----------MQ-135----------
  float gasMQ135=analogRead(pinMQ135);
  
 
  //----------PM2.5----------
  duration = duration + pulseIn(pinPM25,LOW);
  elapsedtime=millis()-starttime;
  
  float ratio = duration/(elapsedtime*10.0);
  float concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62;

  duration = 0;
  starttime = millis();   

  //----------GP2Y----------
  digitalWrite(ledPinGP2Y,LOW);
  delayMicroseconds(samplingTime);
  float voMeasured = analogRead(measurePinGP2Y);

  delayMicroseconds(deltaTime);
  digitalWrite(ledPinGP2Y,HIGH);
  delayMicroseconds(sleepTime);

  float calcVoltage = voMeasured*(5.0/1024);
  float dustDensity = 0.17*calcVoltage-0.1;

  //----------ThingSpeak----------
    byte sd = stash.create();
    stash.print("field1=");
    stash.print(humDHT22);
    stash.print("&field2=");
    stash.print(tempDHT22);
    stash.print("&field3=");
    stash.print(gasMQ7);
    stash.print("&field4=");
    stash.print(gasMQ135);
    stash.print("&field5=");
    stash.print(concentration);
    stash.print("&field6=");
    stash.print(dustDensity);
    stash.print("&field7=");
    stash.print(presBMP180);
    stash.print("&field8=");
    stash.print(tempBMP180);
    stash.save();


    // generate the header with payload - note that the stash size is used,
    // and that a "stash descriptor" is passed in as argument using "$H"
    Stash::prepare(PSTR("POST /update HTTP/1.0" "\r\n"
      "Host: $F" "\r\n"
      "Connection: close" "\r\n"
      "X-THINGSPEAKAPIKEY: $F" "\r\n"
      "Content-Type: application/x-www-form-urlencoded" "\r\n"
      "Content-Length: $D" "\r\n"
      "\r\n"
      "$H"),
    website, PSTR(APIKEY), stash.size(), sd);

    // send the packet - this also releases all stash buffers once done
    session = ether.tcpSend(); 

 // added from: http://jeelabs.net/boards/7/topics/2241
 int freeCount = stash.freeCount();
    if (freeCount <= 3) {   Stash::initMap(56); } 
  }
  
   const char* reply = ether.tcpReply(session);
   
   if (reply != 0) {
     res = 0;
     Serial.println(F(" >>>REPLY recieved...."));
     Serial.println(reply);
   }
   delay(300);
}
