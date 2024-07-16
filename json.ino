#include <ArduinoJson.h>
#include "base64.hpp"

const uint16_t base64Size = 512;

struct encodedResult {
  // unsigned char* message;
  unsigned char* xMessage;
  unsigned char* yMessage;
  unsigned char* zMessage;
  unsigned int length;
};

// so far 512 and 1024 json buffer works
char* generateAccJson() {
  const unsigned int jsonBufferSize = 1024;

  static char accJsonBuffer[jsonBufferSize];  // Statically allocate buffer
  StaticJsonDocument<(jsonBufferSize * 2)> doc;    // Reuse the JsonDocument

  unsigned char base64BufferX[base64Size];
  unsigned char base64BufferY[base64Size];
  unsigned char base64BufferZ[base64Size];

  // Clear the document for reuse
  doc.clear();
  JsonObject body = doc.createNestedObject("body");
  JsonObject data = body.createNestedObject("DATA");

  // pass in buffers as reference so that value is not deleted after the function call
  struct encodedResult er = encode64Buffers(base64BufferX, base64BufferY, base64BufferZ);

  body["ACTION"] = "sendAccDataPackets";
  body["SESSION_ID"] = uuid.toCharArray();
  body["MACHINE_ID"] = "Ender3Test";
  data["X"] = (char*) er.xMessage;
  data["X_LENGTH"] = er.length;
  data["Y"] = (char*) er.yMessage;
  data["Y_LENGTH"] = er.length;
  data["Z"] = (char*) er.zMessage;
  data["Z_LENGTH"] = er.length;
  data["COUNT"] = counter++;

  serializeJson(doc, accJsonBuffer, sizeof(accJsonBuffer));
  Serial.println("JSON generated.");
  return accJsonBuffer;
}

char* generateIncompleteJson() {
  static char incompleteJsonBuffer[128];  // Statically allocate buffer
  StaticJsonDocument<256> doc;            // Reuse the JsonDocument

  doc.clear();
  JsonObject body = doc.createNestedObject("body");
  body["ACTION"] = "incompleteSession";
  body["SESSION_ID"] = uuid.toCharArray();
  body["MACHINE_ID"] = "Ender3Test";

  serializeJson(doc, incompleteJsonBuffer, sizeof(incompleteJsonBuffer));
  Serial.println("JSON generated.");
  return incompleteJsonBuffer;
}

uint8_t convertLowByte(uint16_t x) {
  return (uint8_t)x;
}

uint8_t convertHighByte(uint16_t x) {
  return (uint16_t)x >> 0x8;
}

uint16_t combineByte(uint8_t high, uint8_t low) {
  return ((uint16_t)(high) << 0x8) | (uint16_t)(low);
}

uint8_t getBase64Length(uint8_t n) {
  return (((4 * n / 3) + 3) & ~3) + 10;  // add 1 for null terminator and 9 so that theres extra memory
}

struct encodedResult encode64Buffers(unsigned char* base64XBuffer, unsigned char* base64YBuffer, unsigned char* base64ZBuffer) {
  // TODO: Instead of split we can shift 8 bits per loop

  // split 16-bit buffers into 8-bit ones
  // amount of vals is double the original count
  size_t valCount = accDataCounter[uartUser][0] * 2;
  unsigned char splitXValues[valCount];  // Allocate dynamic memory
  unsigned char splitYValues[valCount];  // Allocate dynamic memory
  unsigned char splitZValues[valCount];  // Allocate dynamic memory

  // uint8_t base64Length = getBase64Length(valCount);  // TODO: check why this stops working at bigger than 32 buff size
  struct encodedResult er;

  for (size_t i = 0; i < valCount; i += 2) {
    splitXValues[i] = (char)convertHighByte(accDataBuffer[uartUser][i].x);
    splitXValues[i + 1] = (char)convertLowByte(accDataBuffer[uartUser][i].x);

    splitYValues[i] = (char)convertHighByte(accDataBuffer[uartUser][i].y);
    splitYValues[i + 1] = (char)convertLowByte(accDataBuffer[uartUser][i].y);

    splitZValues[i] = (char)convertHighByte(accDataBuffer[uartUser][i].z);
    splitZValues[i + 1] = (char)convertLowByte(accDataBuffer[uartUser][i].z);
  }

  er.length = encode_base64(splitXValues, valCount, base64XBuffer);
  er.length = encode_base64(splitYValues, valCount, base64YBuffer);
  er.length = encode_base64(splitZValues, valCount, base64ZBuffer);

  er.xMessage = base64XBuffer;
  er.yMessage = base64YBuffer;
  er.zMessage = base64ZBuffer;

  return er;
}
