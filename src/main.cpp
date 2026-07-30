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

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <SPI.h>
#include <Arduino.h>

#define RED_LED D8      ///< Access-denied indicator
#define GREEN_LED D4    ///Access-granted indicator LED_BuiltIn activeLOW

#define PN532_SCK   (D7)
#define PN532_MOSI   (D5)
#define PN532_SS   (D1)
#define PN532_MISO   (D6)

Adafruit_PN532 nfc(PN532_SCK,PN532_MISO,PN532_MOSI,PN532_SS);

uint8_t SELECT_APDU[] = {
  0x00, 0xA4, 0x04, 0x00,
  0x07,
  0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x00
};

int accessDecision = 0;

void actOnDecision(int decision);
bool selectHceApplet(uint8_t *token, uint8_t *tokenLen);
void printHex(const char *label, const uint8_t *data, uint8_t len);
void waitForTargetRemoval();

void setup() {

  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
    if(!versiondata){
        Serial.print("Didn't find PN532 board");
        while(1) { delay(10); } ;
    }

    Serial.println("Found chip PN532"); 

    //nfc.setPassiveActivationRetries(1);

  nfc.SAMConfig();      // configure PN532 Secure Access Module (required before reads)

  Serial.println("Scan card");
}

void loop() {
   uint8_t uid[7] = {0};
  uint8_t uidLength = 0;

  // Activate one ISO14443A target and read its UID. Blocks until a card or
  // phone is presented. The target stays activated afterwards, so we can
  // immediately try an APDU exchange on it.
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (!success) {
    return;
  }

  uint8_t token[64];
  uint8_t tokenLen = 0;

  if (selectHceApplet(token, &tokenLen)) {
    // Phone running our HCE app: use the applet's token as the identifier.
    printHex("TOKEN value:", token, tokenLen);
  } else {
    // Ordinary card (or a phone that didn't answer our AID): use the UID.
    printHex("UID value:", uid, uidLength);
  }

  // Live-query step: the host on Serial replies with the access decision.
  // TODO: replace with a direct query to the Flask/SQLite backend.
  accessDecision = Serial.readString().toInt();
  actOnDecision(accessDecision);

  waitForTargetRemoval();
}

/**
 * @brief Send the SELECT-AID APDU and check for a valid applet response.
 * @param token    out: identifier bytes returned by the applet (SW stripped)
 * @param tokenLen out: length of @p token in bytes
 * @return true if a target answered our AID with status word 0x9000
 *
 * On success the response is <token bytes> + SW1 SW2 (0x90 0x00). We strip
 * the trailing status word and hand back only the identifier payload.
 */
bool selectHceApplet(uint8_t *token, uint8_t *tokenLen) {
  uint8_t response[64];
  uint8_t responseLength = sizeof(response);

  bool ok = nfc.inDataExchange(SELECT_APDU, sizeof(SELECT_APDU),
                               response, &responseLength);
  if (!ok || responseLength < 2) {
    return false;                 // no APDU channel -> not our HCE app (likely a card)
  }

  // Require a trailing 0x90 0x00 status word.
  if (response[responseLength - 2] != 0x90 ||
      response[responseLength - 1] != 0x00) {
    return false;
  }

  *tokenLen = responseLength - 2;  // drop SW1 SW2
  for (uint8_t i = 0; i < *tokenLen; i++) {
    token[i] = response[i];
  }
  return true;
}

/**
 * @brief Print an identifier as space-separated "0xNN" bytes on one line.
 */
void printHex(const char *label, const uint8_t *data, uint8_t len) {
  Serial.print(label);
  for (uint8_t i = 0; i < len; i++) {
    Serial.print("0x");
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

/**
 * @brief Block until the presented target is physically removed.
 *
 * The Adafruit library has no inRelease(); instead we poll with a short
 * timeout until no target is seen, so the same card/phone isn't re-read
 * on the next loop iteration.
 */
void waitForTargetRemoval() {
  uint8_t tmpUid[7];
  uint8_t tmpLen;
  while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tmpUid, &tmpLen, 200)) {
    delay(100);
  }
}

void actOnDecision(int decision) {
  if (decision == 0) {            // denied: red on, green off
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
  } else if (decision == 1) {     // granted: red off, green on (green active LOW)
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  } else {                        // unknown decision: alternating blink
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    delay(100);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    delay(100);
  }
}

