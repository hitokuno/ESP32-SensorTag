#include "BLEDevice.h"
//#include "BLEScan.h"

// The remote device we wish to connect to.
static BLEUUID        deviceUUID("0000aa80-0000-1000-8000-00805f9b34fb");
// The remote service we wish to connect to.
static BLEUUID  notificationUUID("00002902-0000-1000-8000-00805f9b34fb");
static BLEUUID       serviceUUID("f000aa80-0451-4000-b000-000000000000");
static BLEUUID          dataUUID("f000aa81-0451-4000-b000-000000000000");
static BLEUUID configurationUUID("f000aa82-0451-4000-b000-000000000000");
static BLEUUID        periodUUID("f000aa83-0451-4000-b000-000000000000");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pNotificationCharacteristic;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* pConfigurationCharacteristic;
static BLERemoteCharacteristic* pPeriodCharacteristic;
int rssi = -10000;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
}

bool connectToServer(BLEAddress pAddress) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());

    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(dataUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(dataUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic");

    //  Enable Accelerometer
    pConfigurationCharacteristic = pRemoteService->getCharacteristic(configurationUUID);
    if (pConfigurationCharacteristic == nullptr) {
      Serial.print("Failed to find our Configuration: ");
      Serial.println(configurationUUID.toString().c_str());
      return false;
    }
    // Gyroscope must be enabled to get Accelerometer due to unknown reason..
    uint8_t accelerometerEnabled[2] = {B00111111, B00000000}; //2^3+2^4+2^5;
    pConfigurationCharacteristic->writeValue( accelerometerEnabled, 2);
    // Read the value of the characteristic.
    uint16_t valueInt16 = pConfigurationCharacteristic->readUInt16();
    Serial.print(" - Enabled accelerometer: ");
    Serial.println(valueInt16, BIN);

    // Set update period.
    //Resolution 10 ms. Range 100 ms (0x0A) to 2.55 sec (0xFF). Default 1 second (0x64).
    uint8_t accelerometerPeriod = 10;
    pPeriodCharacteristic = pRemoteService->getCharacteristic(periodUUID);
    pPeriodCharacteristic->writeValue( accelerometerPeriod, 1);
    Serial.print(" - Accelerometer priod: ");
    uint8_t valueInt8 = pPeriodCharacteristic->readUInt8();
    Serial.print(valueInt8);
    Serial.println("*10ms");

    uint8_t notification = 0x0001;
    pNotificationCharacteristic = pRemoteService->getCharacteristic(notificationUUID);
    if (pNotificationCharacteristic == nullptr) {
      Serial.print("Failed to find our notification: ");
      Serial.println(notificationUUID.toString().c_str());
      //return false;
    }

    pRemoteCharacteristic->registerForNotify(notifyCallback);
}
/**
 * Scan for BLE servers and find the strongest RSSI one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(deviceUUID)) {

      //
      int thisRssi = advertisedDevice.getRSSI();
      BLEAddress *thisAddress;
      thisAddress = new BLEAddress(advertisedDevice.getAddress());
      Serial.print("Found our device!  address: ");
      Serial.print(thisAddress->toString().c_str());
      //advertisedDevice.getScan()->stop();
      Serial.print("  RSSI: ");
      Serial.println(thisRssi);

      if ( thisRssi > rssi ) {
        rssi = thisRssi;
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      }

      doConnect = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  scanBle();
} // End of setup.

void scanBle() {
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if (connected) {
    Serial.print("Connected...");
    std::string value = pRemoteCharacteristic->readValue();
    String strings = value.c_str();
    //printGyroscope(strings);
    printAccelerometer(strings);
    Serial.println("");
    accelerometerToAnalogWrite(strings);
  }

  delay(100); // Delay a second between loops.
} // End of loop

void printGyroscope(String strings) {
  Serial.print("  Gyroscope X: ");
  Serial.print(strings.charAt( 1), HEX);
  Serial.print(strings.charAt( 0), HEX);
  Serial.print("  Gyroscope Y: ");
  Serial.print(strings.charAt( 3), HEX);
  Serial.print(strings.charAt( 2), HEX);
  Serial.print("  Gyroscope Z: ");
  Serial.print(strings.charAt( 5), HEX);
  Serial.print(strings.charAt( 4), HEX);
}

void printAccelerometer(String strings) {
  Serial.print("  Accelerometer X: ");
  Serial.print(strings.charAt( 7), HEX);
  Serial.print(strings.charAt( 6), HEX);
  Serial.print("  Accelerometer Y: ");
  Serial.print(strings.charAt( 9), HEX);
  Serial.print(strings.charAt( 8), HEX);
  Serial.print("  Accelerometer Z: ");
  Serial.print(strings.charAt(11), HEX);
  Serial.print(strings.charAt(10), HEX);
}

void accelerometerToAnalogWrite(String strings) {
  dacWrite(25, strings.charAt( 7));
  dacWrite(26, strings.charAt( 9));
}
