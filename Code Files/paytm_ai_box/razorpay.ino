// ******************************************************************************************************************************
// RAZORPAY API handlers
//   - fetchRazorpayRange(from, to)        : total in date range
//   - fetchRazorpayLast(n)                : last N captured payments
//   - fetchRazorpayCount(from, to)        : count in date range
// Bilingual TTS using detected_lang (set by speech_to_text.ino).
// ******************************************************************************************************************************

static String razorpay_get(const String& url) {
  HTTPClient http;
  http.begin(url);
  http.setAuthorization(cfg_razorpay_key.c_str(), cfg_razorpay_secret.c_str());
  int code = http.GET();
  String payload = (code == 200) ? http.getString() : String("");
  if (code != 200) Serial.printf("Razorpay HTTP %d\n", code);
  http.end();
  return payload;
}

static void razorpay_speak(const String& en, const String& hi) {
  TextToSpeech(isHindi() ? hi : en);
}

// ---------------------------------------------------------------------------
// Total in date range (existing behavior)
void fetchRazorpayRange(long fromTimestamp, long toTimestamp) {
  String url = "https://api.razorpay.com/v1/payments?from=" + String(fromTimestamp) +
               "&to=" + String(toTimestamp) + "&count=100";
  DebugPrintln("Razorpay URL: " + url);

  Serial.println("Contacting Razorpay API...");
  String payload = razorpay_get(url);
  if (payload == "") {
    showAIResult("Network error", "Razorpay API request failed.");
    razorpay_speak("Failed to reach Razorpay.", "Razorpay से जुड़ नहीं सका।");
    return;
  }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload)) {
    Serial.println("JSON parse error");
    return;
  }

  JsonArray items = doc["items"];
  int  count = 0;
  long total = 0;
  for (JsonObject p : items) {
    const char* status = p["status"];
    if (status && strcmp(status, "captured") == 0) {
      count++;
      total += p["amount"].as<long>();
    }
  }

  String amtStr = String(total / 100.0, 2);
  Serial.println("Total: Rs." + amtStr + " (" + String(count) + " txns)");

  if (count == 0) {
    showAIResult("No data", "No transactions found in the requested date range.");
    razorpay_speak("No transactions found in the requested date range.",
                   "इस अवधि में कोई भुगतान नहीं मिला।");
  } else {
    showAIResult("Total: Rs. " + amtStr,
                 String(count) + " transactions in range.");
    razorpay_speak("Total amount received is " + speakRupees(total) + ".",
                   "कुल राशि " + speakRupeesHindi(total) + " प्राप्त हुई।");
  }
}

// ---------------------------------------------------------------------------
// Last N captured payments
void fetchRazorpayLast(int n) {
  if (n < 1)  n = 1;
  if (n > 20) n = 20;

  String url = "https://api.razorpay.com/v1/payments?count=" + String(n * 3);  // fetch more to filter captured
  DebugPrintln("Razorpay URL: " + url);

  Serial.println("Contacting Razorpay API...");
  String payload = razorpay_get(url);
  if (payload == "") {
    showAIResult("Network error", "Razorpay API request failed.");
    razorpay_speak("Failed to reach Razorpay.", "Razorpay से जुड़ नहीं सका।");
    return;
  }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload)) {
    Serial.println("JSON parse error");
    return;
  }

  JsonArray items = doc["items"];
  int  found = 0;
  long total = 0;
  long firstAmount = 0;
  long firstTs     = 0;

  for (JsonObject p : items) {
    const char* status = p["status"];
    if (status && strcmp(status, "captured") == 0) {
      if (found == 0) {
        firstAmount = p["amount"].as<long>();
        firstTs     = p["created_at"].as<long>();
      }
      total += p["amount"].as<long>();
      found++;
      if (found >= n) break;
    }
  }

  if (found == 0) {
    showAIResult("No data", "No captured payments found.");
    razorpay_speak("No captured payments found.",
                   "कोई भुगतान नहीं मिला।");
    return;
  }

  if (n == 1) {
    // Last single payment
    String amtStr = String(firstAmount / 100.0, 2);
    String when   = spokenDate(firstTs) + " at " + spokenTime(firstTs);
    showAIResult("Last: Rs. " + amtStr, when);
    razorpay_speak("Your last payment was " + speakRupees(firstAmount) + ", received on " + when + ".",
                   "आपका पिछला भुगतान " + speakRupeesHindi(firstAmount) + " था।");
  } else {
    String amtStr = String(total / 100.0, 2);
    showAIResult("Last " + String(found) + ": Rs. " + amtStr,
                 "Sum of recent " + String(found) + " payments.");
    razorpay_speak("Your last " + String(found) + " payments total " + speakRupees(total) + ".",
                   "पिछले " + String(found) + " भुगतान की कुल राशि " + speakRupeesHindi(total) + " है।");
  }
}

// ---------------------------------------------------------------------------
// Count in date range
void fetchRazorpayCount(long fromTimestamp, long toTimestamp) {
  String url = "https://api.razorpay.com/v1/payments?from=" + String(fromTimestamp) +
               "&to=" + String(toTimestamp) + "&count=100";
  DebugPrintln("Razorpay URL: " + url);

  Serial.println("Contacting Razorpay API...");
  String payload = razorpay_get(url);
  if (payload == "") {
    showAIResult("Network error", "Razorpay API request failed.");
    razorpay_speak("Failed to reach Razorpay.", "Razorpay से जुड़ नहीं सका।");
    return;
  }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, payload)) {
    Serial.println("JSON parse error");
    return;
  }

  JsonArray items = doc["items"];
  int count = 0;
  for (JsonObject p : items) {
    const char* status = p["status"];
    if (status && strcmp(status, "captured") == 0) count++;
  }

  showAIResult("Count: " + String(count), "captured payments in range.");
  razorpay_speak(String(count) + " payments received in this period.",
                 String(count) + " भुगतान प्राप्त हुए।");
}
