/**
 * @file NFC_Reader_main.cpp
 * @brief Proof of concept acces-control firmware for ESP8266
 * 
 * Reads a MIFARE/ISO14443A UID from a PN532 reader over SPI, prints it
 * to Serial, and waits for an external access decision (0/1/other) sent
 * back over Serial. Drives a red/green LED pair to reflect the decision.
 * 
 * This is a live-query prototype: the access decision currently comes
 * from whatever is listening on Serial (e.g. a Python script talking to
 * the Flask/SQLite backend), not from onboard logic.
 *
 * @hardware ESP8266, PN532, 2x LED
 * @author Can
 * @date 2026-07
 */

#include <PN532.h>
#include <PN532_SPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Arduino.h>

#define PN532_SS D1     ///< PN532 SPI chip-select pin

#define RED_LED D8      ///< Access-denied indicator
#define GREEN_LED D4    ///Access-granted indicator LED_BuiltIn activeLOW

PN532_SPI pn532spi(SPI, PN532_SS);
PN532 nfc(pn532spi);

void actOnDecision(int decision);

unsigned long currentTimeInMs;
unsigned long currentTimeIns;

int accessDecision = 0;

void setup() {

  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  nfc.begin();
  nfc.SAMConfig();      // configure PN532 Secure Access Module (required before reads)

  Serial.println("Scan card");
}

void loop() {
  currentTimeInMs = millis();
  currentTimeIns = currentTimeInMs / 100;
  
  uint8_t success;
  uint8_t uid[7] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success)
  {
    Serial.print("UID value:");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print("0x");
      if (uid[i] < 0x10) Serial.print("0");
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    //TODO: replace with a proper backend query
    //Current method: Whatever host os attached to Serial sends back the access decision as an integer.
    accessDecision = Serial.readString().toInt();
    actOnDecision(accessDecision);

    // Release the tag and wait until it's lifted away before scanning
    // again, otherwise the PN532 keeps re-reporting the same tag every
    // loop and never frees up to detect a different card.
    nfc.inRelease();
    uint8_t tmpUid[7];
    uint8_t tmpLen;
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tmpUid, &tmpLen, 200)) {
      nfc.inRelease();
      delay(100);
    }
  }
}

void actOnDecision(int decision){
  if (decision == 0){
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
  }
  else if(decision == 1){
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  }
  
  else{
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    delay(100);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    delay(100);
  }
  
}

