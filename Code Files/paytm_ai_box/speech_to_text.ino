// ******************************************************************************************************************************
// SPEECH TO TEXT via ElevenLabs (multipart upload of WAV in PSRAM)
// ******************************************************************************************************************************

String SpeechToText_ElevenLabs(String audio_filename, uint8_t* PSRAM, long PSRAM_length, String language, const char* API_Key) {
  uint32_t t_start = millis();
  Serial.print("Sending to ElevenLabs STT...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n* ERROR - WiFi not connected!");
    return "";
  }

  if (PSRAM_length > 500000) {
    Serial.println("\n* ERROR - Audio file too large (>500KB)");
    return "";
  }

  uint32_t t_file_loaded = millis();

  HTTPClient http;
  http.begin("https://api.elevenlabs.io/v1/speech-to-text");
  http.setTimeout(30000);
  http.setConnectTimeout(10000);
  http.addHeader("xi-api-key", String(API_Key));

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  String body_start  = "--" + boundary + "\r\n";
  body_start += "Content-Disposition: form-data; name=\"model_id\"\r\n\r\n";
  body_start += "scribe_v1\r\n";
  body_start += "--" + boundary + "\r\n";
  body_start += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  body_start += "Content-Type: audio/wav\r\n\r\n";

  String body_end = "\r\n--" + boundary + "--\r\n";

  size_t total_size = body_start.length() + PSRAM_length + body_end.length();

  uint8_t* complete_body = (uint8_t*)malloc(total_size);
  if (!complete_body) {
    Serial.println("\n* ERROR - Failed to allocate memory for HTTP body!");
    http.end();
    return "";
  }

  memcpy(complete_body,                                          body_start.c_str(), body_start.length());
  memcpy(complete_body + body_start.length(),                    PSRAM,              PSRAM_length);
  memcpy(complete_body + body_start.length() + PSRAM_length,     body_end.c_str(),   body_end.length());

  uint32_t t_request_prepared = millis();
  uint32_t t_request_sent     = millis();
  int httpResponseCode = http.POST(complete_body, total_size);
  uint32_t t_response_received = millis();

  free(complete_body);

  String transcription = "";
  String response = http.getString();
  uint32_t t_response_parsed = millis();

  if (httpResponseCode == 200) {
    DebugPrintln("\nHTTP 200 OK");
    DebugPrintln("Response: " + response);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    if (error == DeserializationError::Ok) {
      if (doc.containsKey("text")) {
        transcription = doc["text"].as<String>();
        transcription.trim();
      }
      if (doc.containsKey("language_code")) {
        detected_lang = doc["language_code"].as<String>();
        DebugPrintln("Detected language: " + detected_lang);
      }
    } else {
      Serial.println("\n* ERROR - JSON parse failed: " + String(error.c_str()));
    }
  } else {
    Serial.printf("\n* ERROR - HTTP %d\n", httpResponseCode);
    Serial.println("Response: " + response);
  }

  http.end();

  DebugPrintln("\n---------------------------------------------------");
  DebugPrintln("-> Audio buffer size: " + String(PSRAM_length) + " bytes");
  DebugPrintln("-> Latency File Loading: "        + String((float)(t_file_loaded      - t_start)            / 1000, 3) + " sec");
  DebugPrintln("-> Latency Request Preparation: " + String((float)(t_request_prepared - t_file_loaded)      / 1000, 3) + " sec");
  DebugPrintln("-> Latency ElevenLabs STT Response: " + String((float)(t_response_received - t_request_sent) / 1000, 3) + " sec");
  DebugPrintln("-> Latency Response Parsing: "    + String((float)(t_response_parsed  - t_response_received)/ 1000, 3) + " sec");
  DebugPrintln("=> TOTAL Duration: "              + String((float)(t_response_parsed  - t_start)            / 1000, 3) + " sec");
  DebugPrintln("=> Transcription: [" + transcription + "]");
  DebugPrintln("---------------------------------------------------");

  return transcription;
}
