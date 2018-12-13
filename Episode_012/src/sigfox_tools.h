#define UINT16_t_MAX  65536
#define INT16_t_MAX   UINT16_t_MAX/2

int16_t convertoFloatToInt16(float value, long max, long min) {
  float conversionFactor = (float) (INT16_t_MAX) / (float)(max - min);
  return (int16_t)(value * conversionFactor);
}

uint16_t convertoFloatToUInt16(float value, long max, long min = 0) {
  float conversionFactor = (float) (UINT16_t_MAX) / (float)(max - min);
  return (uint16_t)(value * conversionFactor);
}

void printSigfoxInfo() {
  String version = SigFox.SigVersion();
  String ID = SigFox.ID();
  String PAC = SigFox.PAC();

  // Display module informations
  Serial.println("MKRFox1200 Sigfox first configuration");
  Serial.println("SigFox FW version " + version);
  Serial.println("ID  = " + ID);
  Serial.println("PAC = " + PAC);

  Serial.println("");

  Serial.print("Module temperature: ");
  Serial.println(SigFox.internalTemperature());

  Serial.println("Register your board on https://backend.sigfox.com/activate with provided ID and PAC");

}

void sendSigfoxMessage(SigfoxMessage *msg) {
  // Start the module
  SigFox.begin();
  // Wait at least 30ms after first configuration (100ms before)
  delay(100);

  // get the temperature value from the sigfox module
  float temperature = SigFox.internalTemperature();
  // convert the floating point value to an int16 value
  msg->moduleTemperature = convertoFloatToInt16(temperature, 60, -60);

  // Clears all pending interrupts
  SigFox.status();
  delay(1);

  SigFox.beginPacket();
  SigFox.write((uint8_t*) msg, sizeof(*msg));
  int ret = SigFox.endPacket();
  SigFox.end();

  if (DEBUG) {
    Serial.println("Temperature: " + String(temperature));

    if (ret > 0) {
      Serial.println("Transmisson failed: " + String(ret));
    } else {
      Serial.println("Transmission: OKay");
    }
  }
}
