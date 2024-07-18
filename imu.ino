#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "SensorQMI8658.hpp"

#define USE_WIRE

#ifndef SENSOR_SDA
#define SENSOR_SDA 6
#endif

#ifndef SENSOR_SCL
#define SENSOR_SCL 7
#endif

#ifndef SENSOR_IRQ
#define SENSOR_IRQ -1
#endif

#define IMU_CS 5

#define IMU_INT1 47
#define IMU_INT2 48

SensorQMI8658 qmi;

const uint16_t bufferSize = 4096;
const uint8_t bufferClone = 2;

// initialize the double buffer kernel and user
uint8_t uartUser = 1;
uint8_t uartKernel = 0;

IMUdataRaw accDataBuffer[bufferClone][bufferSize];
IMUdataRaw gyroDataBuffer[0];  // keep only one for gyro data buffer, since we are not collecting gyro data
uint16_t accDataCounter[bufferClone][1] = { { 0 }, { 0 } };

void setupIMU(void) {

#ifdef USE_WIRE
  //Using WIRE !!
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, SENSOR_SDA, SENSOR_SCL)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) {
      delay(1000);
    }
  }
#else
  //Using SPI !!
  if (!qmi.begin(IMU_CS)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) {
      delay(1000);
    }
  }
#endif

  /* Get chip id*/
  Serial.print("Device ID:");
  Serial.println(qmi.getChipID(), HEX);

  qmi.configAccelerometer(
    /*
         * ACC_RANGE_2G
         * ACC_RANGE_4G
         * ACC_RANGE_8G
         * ACC_RANGE_16G
         * */
    SensorQMI8658::ACC_RANGE_4G,
    /*
         * ACC_ODR_1000Hz
         * ACC_ODR_500Hz
         * ACC_ODR_250Hz
         * ACC_ODR_125Hz
         * ACC_ODR_62_5Hz
         * ACC_ODR_31_25Hz
         * ACC_ODR_LOWPOWER_128Hz
         * ACC_ODR_LOWPOWER_21Hz
         * ACC_ODR_LOWPOWER_11Hz
         * ACC_ODR_LOWPOWER_3H
        * */
    SensorQMI8658::ACC_ODR_125Hz,
    // SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,
    /*
        *  LPF_MODE_0     //2.66% of ODR
        *  LPF_MODE_1     //3.63% of ODR
        *  LPF_MODE_2     //5.39% of ODR
        *  LPF_MODE_3     //13.37% of ODR
        * */
    SensorQMI8658::LPF_MODE_0,
    // selfTest enable
    true);


  // enable interrupt
  qmi.disableSyncSampleMode(); // sync sample mode 0 for fifo
  attachWTMInterrupt(); 
  startFifo();
  // Enable data ready to interrupt pin2
  qmi.enableINT(SensorQMI8658::IntPin2);

  qmi.disableGyroscope();
  qmi.enableAccelerometer();

  // Print register configuration information
  qmi.dumpCtrlRegister();

  Serial.println("Read data now...");
}

void switchBuffers() {
  // use xor (TODO)
  uartUser = (!uartUser) & 0x01;
  uartKernel = (!uartKernel) & 0x01;

  // Reset the counter for the ISR
  accDataCounter[uartKernel][0] = 0;
}


bool areBuffersEmpty() {
  return (accDataCounter[uartKernel][0] == 0 && accDataCounter[uartUser][0] == 0);
}

void clearBuffers() {
  accDataCounter[uartUser][0] = 0;
  accDataCounter[uartKernel][0] = 0;
  bufferFull = false;
}

void getAcceleration() {

  bool status = qmi.isFIFOHitWTM();
  Serial.println("Interrupt Detected.");
  Serial.print("Status: ");
  Serial.println(status);
  Serial.print("Sample Count: ");
  Serial.println(qmi.getFIFOSampleCount());
  // if (qmi.getFIFOSampleCount() > 0) {
  if (status) {
    if (accDataCounter[uartKernel][0] >= bufferSize) {
      if (accDataCounter[uartUser][0] == 0) {
        switchBuffers();
      } else {
        Serial.println("Current buffer is full and second buffer is not empty! Restarting...");
        bufferFull = true;
        // collectDataState = false;
        return;
      }
    }

    // If the reading is successful, counter will be returned
    // passing in the address of the array which is the first element of the array, the last parameter is the startFrom index to
    // tell the program where the indexing is starting from
    int bufferCounter = qmi.readFromFifoRaw(&accDataBuffer[uartKernel][0], (bufferSize - accDataCounter[uartKernel][0]), &gyroDataBuffer[0], 0, accDataCounter[uartKernel][0]);

    if (bufferCounter == 0) {
      return;
    }
    accDataCounter[uartKernel][0] += bufferCounter;  // increment the buffer counter
  }
}

bool isFifoEmpty() {
  // Check if FIFO is empty.
  return !qmi.isFIFONotEmpty();
}

bool isFifoFull() {
  // Check if FIFO is full.
  return qmi.isFIFOFull();
}

void disableAcc() {
  qmi.disableAccelerometer();  // flush FIFO data
}

void resetFifo() {
  qmi.resetFIFO();
}

void stopFifo() {
  qmi.configFIFO(
    /**
        * FIFO_MODE_BYPASS      -- Disable fifo
        * FIFO_MODE_FIFO        -- Will not overwrite
        * FIFO_MODE_STREAM      -- Cover
        */
    SensorQMI8658::FIFO_MODE_BYPASS,
    /*
         * FIFO_SAMPLES_16
         * FIFO_SAMPLES_32
         * FIFO_SAMPLES_64
         * FIFO_SAMPLES_128
        * */
    SensorQMI8658::FIFO_SAMPLES_128,

    //FiFo mapped interrupt IO port
    SensorQMI8658::IntPin2,
    // watermark level
    64);
}

void startFifo() {
  qmi.configFIFO(
    /**
        * FIFO_MODE_BYPASS      -- Disable fifo
        * FIFO_MODE_FIFO        -- Will not overwrite
        * FIFO_MODE_STREAM      -- Cover
        */
    SensorQMI8658::FIFO_MODE_FIFO,
    /*
         * FIFO_SAMPLES_16
         * FIFO_SAMPLES_32
         * FIFO_SAMPLES_64
         * FIFO_SAMPLES_128
        * */
    SensorQMI8658::FIFO_SAMPLES_128,

    //FiFo mapped interrupt IO port
    SensorQMI8658::IntPin2,
    // watermark level
    64);
}

void publishAcceleration(char* currentUUID) {
  if (accDataCounter[uartUser][0] == bufferSize) {
    // const char* jsonMessage = generateAccJson();
    // publishMessage(jsonMessage);

    accDataCounter[uartUser][0] = 0;
  }
}

void attachWTMInterrupt() {

  pinMode(IMU_INT1, INPUT);
#ifdef IMU_INT2
  pinMode(IMU_INT2, INPUT);
#endif
  attachInterrupt(IMU_INT2, getAcceleration, HIGH);
}