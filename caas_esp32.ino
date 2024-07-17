#include "UUID.h"

UUID uuid;
uint16_t counter = 0;

bool mqttWasDisconnected = false;
bool bufferFull = false;
bool fifoFull = false;
unsigned long previousTimeAcc = millis();
bool collectionFinished = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    ;

  setupUUID();
  setupNetwork();
  setupIMU();

  previousTimeAcc = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  // startFifo();
  while (!(collectionFinished && isFifoEmpty())) {

    checkMQTT();
    keepMQTTAlive();

    // Restart the session if mqtt lost connection
    if (mqttWasDisconnected) {
      Serial.println(F("MQTT disconnected. Restarting..."));
      mqttWasDisconnected = false;
      resetCollection();
    }

    // Restart the session if both buffers was full
    if (bufferFull) {
      Serial.println(F("Buffers are full. Restarting..."));
      resetCollection();
    }

    // Restart the session if fifo is full
    if (fifoFull) {
      Serial.println(F("FIFO is full. Restarting..."));
      fifoFull = false;
      resetCollection();
      startFifo();
    }

    getAcceleration();
    // publish accelerometer data to cloud
    publishAcceleration(uuid.toCharArray());

    if (millis() - previousTimeAcc > 40000) {
      Serial.print(F("It has been 40 seconds. Stopping data collection for "));
      Serial.println(uuid.toCharArray());
      disableAcc();
      collectionFinished = true;
    }
  }
  stopFifo();
  Serial.print("Collection finished: ");
  Serial.println(collectionFinished);
  Serial.println("Program completed. Exiting...");
  exit(0);
}

void resetCollection() {
  const char* jsonMessage = generateIncompleteJson();
  publishMessage(jsonMessage);

  resetFifo();
  clearBuffers();  // clear both buffers and set bufferFull to false
  uuid.generate();
  counter = 0;  // reset publish counter to 0
  previousTimeAcc = millis();
  delay(2000);
  // collectDataState = true;
}

void checkFIFO() {
  if (isFifoFull()) {
    fifoFull = true;
    resetFifo();
    stopFifo();
  }
}

void setupUUID() {
  uuid.seed(extractEntropy());
  uuid.generate();
}

// generate a random seed https://github.com/RobTillaart/UUID/blob/master/examples/UUID_random_seed/UUID_random_seed.ino#L9
uint32_t extractEntropy() {
  // GET COMPILE TIME ENTROPY
  uint32_t r = 0;
  uint16_t len = strlen(__FILE__);
  for (uint16_t i = 0; i < len; i++) {
    r ^= __FILE__[i];
    r = r * (r << 3);
  }
  len = strlen(__DATE__);
  for (uint16_t i = 0; i < len; i++) {
    r ^= __DATE__[i];
    r = r * (r << 17);
  }
  len = strlen(__TIME__);
  for (uint16_t i = 0; i < len; i++) {
    r ^= __TIME__[i];
    r = r * (r << 7);
  }

  // GET RUNTIME ENTROPY
  Serial.println("Generating runtime entropy for uuid seed...");
  uint32_t mask = 0;
  for (int i = 0; i < 8; i++) {
    mask <<= 7;
    mask ^= micros();
  }
  return r ^ mask;
}
