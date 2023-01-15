/*******************************************************************************
   Credits to:
   2015 Thomas Telkamp and Matthijs Kooijman
   2018 Terry Moore, MCCI
   2023 hjunleon
 *******************************************************************************/

//Helium
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

//Credentials
#include "helium_crd.h"

//GPS
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

//###########################################
//###########################################

// Pin mapping Helium
const lmic_pinmap lmic_pins = {
  .nss = 5,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 4,
  .dio = {34, 35, 33},  //DIO0,DIO1,DIO2
};

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 30; // <------------------- Change to lower the send rate

static osjob_t sendjob;

//Data object which will be send to the Helium network
uint8_t mydata[20]={};
//Used to keep running the "os_runloop_once()" function running until a package is send successfully 
bool packageSent = false;


//GPS
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 9600;

// The TinyGPSPlus object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//####################################################################################################
//---------------------------------------------------------------------------------------------------
//-------------------------------------------Setup---------------------------------------------------
//---------------------------------------------------------------------------------------------------
//####################################################################################################
void setup() {
  Serial.begin(9600);
  Serial.println("-----------############-Setup-##########-------------");

#ifdef VCC_ENABLE
  // For Pinoccio Scout boards
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);
  delay(1000);
#endif

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();
    
  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
  
  //Run LMIC loop until he as finish
  while(packageSent == false)
  {
    os_runloop_once();
  }
  packageSent = true;


 //GPS
 ss.begin(GPSBaud);
}


//####################################################################################################
//---------------------------------------------------------------------------------------------------
//-------------------------------------------Loop---------------------------------------------------
//---------------------------------------------------------------------------------------------------
//####################################################################################################
void loop() {
   Serial.println("-----------############-Loop-##########-------------");

   //ToDo need to be fixed
    while (ss.available() > 0)
      if (gps.encode(ss.read()))
        displayInfo();
      
  
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    delay (2000);
  }


  
  //Helium send loop----------------------------------------
  //Used to only print the debug message below once to the console
  bool messageOnce=false;
  
  while(packageSent == false)
  {
    if(messageOnce = false){
      Serial.println("############Helium Loop##########");
       messageOnce=true;
    }
    
    os_runloop_once();
  }
  packageSent = true;
  messageOnce=false;
}


//####################################################################################################
//---------------------------------------------------------------------------------------------------
//-------------------------------------------Functions---------------------------------------------------
//---------------------------------------------------------------------------------------------------
//####################################################################################################
//GPS

void displayInfo()
{
  Serial.print(F("Location: "));
  if (gps.location.isValid())
  {
    //Create the GPS coordinates string
    String gpsLatLongString = String (gps.location.lat(), 6)+","+String(gps.location.lng(), 6);
    Serial.print(gpsLatLongString);

    int lenGPSString = sizeof(gpsLatLongString);

    //Add the GPS string to the Helium object which will be send
    for (int i=0; i<19;i++){
      mydata[i]= gpsLatLongString[i];
//    Serial.print(gps.location.lat(), 6);
//    Serial.print(F(","));
//    Serial.print(gps.location.lng(), 6);
      
    }
    //Set to false to sned this coordinates as next package
    packageSent = false;
  }  
  else
  {
    Serial.print(F("INVALID"));
    packageSent = true;
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
    packageSent = true;
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
    packageSent = true;
  }

  Serial.println();
}


//====================================
//Helium

void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}


void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}


void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
        Serial.println(devaddr, HEX);
        Serial.print("AppSKey: ");
        for (size_t i = 0; i < sizeof(artKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(artKey[i]);
        }
        Serial.println("");
        Serial.print("NwkSKey: ");
        for (size_t i = 0; i < sizeof(nwkKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(nwkKey[i]);
        }
        Serial.println();
      }
      // Disable link check validation (automatically enabled
      // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
      LMIC_setLinkCheckMode(0);
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.
      ||
      || case EV_RFU1:
      ||     Serial.println(F("EV_RFU1"));
      ||     break;
    */
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.print(F("Received "));
        Serial.print(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
      }
      packageSent = true;
      
      // Schedule next transmission
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.w
      ||
      || case EV_SCAN_FOUND:
      ||    Serial.println(F("EV_SCAN_FOUND"));
      ||    break;
    */
    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;

    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned) ev);
      break;
  }
}

void do_send(osjob_t* j) {
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, mydata, sizeof(mydata), 0); // Perhaps send a struct. Multi rate system may be complicated, so just send less frequently, at most inference for certain data can do more often?
    Serial.println(F("Packet queued"));
    Serial.print("sizeof(mydata) "); Serial.println(sizeof(mydata));
    Serial.println("+====================================+");
    
    
  }
  // Next TX is scheduled after TX_COMPLETE event.
}
