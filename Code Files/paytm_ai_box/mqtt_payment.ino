// ******************************************************************************************************************************
// MQTT - Adafruit IO payment-confirmation feed.
// `mqtt` and `amountFeed` are pointers built in setup() after the WiFiManager portal supplies credentials.
// ******************************************************************************************************************************

// Probe raw TCP reachability of the broker. If this fails the network blocks the port.
static bool mqtt_probeTCP() {
  WiFiClient probe;
  probe.setTimeout(4);
  bool ok = probe.connect(AIO_SERVER, AIO_SERVERPORT);
  probe.stop();
  return ok;
}

void MQTT_connect() {
  if (!mqtt) return;
  if (mqtt->connected()) return;

  // Back off so we don't get rate-limit-banned by Adafruit IO while debugging.
  static unsigned long next_attempt = 0;
  if (millis() < next_attempt) return;

  static unsigned long last_probe_log = 0;
  if (millis() - last_probe_log > 30000) {
    last_probe_log = millis();
    bool reachable = mqtt_probeTCP();
    Serial.printf("TCP %s:%d reachable: %s\n", AIO_SERVER, AIO_SERVERPORT, reachable ? "YES" : "NO (network blocks port)");
  }

  int8_t ret = mqtt->connect();
  if (ret == 0) {
    Serial.println("MQTT connected.");
    return;
  }

  Serial.printf("MQTT connect failed (code %d): %s\n", ret, mqtt->connectErrorString(ret));
  mqtt->disconnect();
  next_attempt = millis() + 15000;  // wait 15 s before next try
}

void MQTT_poll() {
  if (!mqtt) return;

  Adafruit_MQTT_Subscribe* subscription;
  while ((subscription = mqtt->readSubscription(20))) {
    if (subscription == amountFeed) {
      String receivedData = String((char*)amountFeed->lastread);
      DebugPrintln("MQTT payment: " + receivedData);

      StaticJsonDocument<1024> doc;
      if (deserializeJson(doc, receivedData) == DeserializationError::Ok) {
        if (doc.containsKey("payment")) {
          long amtPaise   = doc["payment"]["entity"]["amount"];
          String finalAmt = String(amtPaise / 100.0, 2);

          showPaymentReceived(finalAmt);
          success_shown_at = millis();

          if (isHindi()) {
            TextToSpeech("भुगतान प्राप्त हुआ! " + speakRupeesHindi(amtPaise) + "।");
          } else {
            TextToSpeech("Payment received. " + speakRupees(amtPaise) + ".");
          }
        }
      }
    }
  }
}

void MQTT_serviceSuccessTimeout() {
  if (current_screen == SCREEN_SUCCESS && success_shown_at != 0 &&
      (millis() - success_shown_at) > 4000) {
    success_shown_at = 0;
    returnToIdle();
  }
}
