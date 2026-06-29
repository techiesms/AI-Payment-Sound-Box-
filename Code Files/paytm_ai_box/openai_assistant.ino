// ******************************************************************************************************************************
// OPENAI - Date interpretation for Razorpay queries + JSON extraction helper
// ******************************************************************************************************************************

String OpenAI_RazorpayAssistant(String UserRequest, String currentDate) {
  if (UserRequest == "") return "{\"error\":\"Empty input\"}";

  UserRequest.replace("\"", "\\\"");
  UserRequest.replace("\n", " ");
  UserRequest.replace("\r", "");

  String systemPrompt = "{\"role\":\"system\",\"content\":\"";
  systemPrompt += "You are a Razorpay payment assistant. Convert the user's question into a structured JSON action.\\n\\n";
  systemPrompt += "CURRENT DATE: " + currentDate + "\\n\\n";
  systemPrompt += "ACTIONS (return EXACTLY one):\\n";
  systemPrompt += "1) range_total - total amount in a date range. Use for: 'today', 'yesterday', 'last 10 days', 'this/last month', 'from X to Y'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"range_total\\\\\\\",\\\\\\\"from_year\\\\\\\":Y,\\\\\\\"from_month\\\\\\\":M,\\\\\\\"from_day\\\\\\\":D,\\\\\\\"to_year\\\\\\\":Y,\\\\\\\"to_month\\\\\\\":M,\\\\\\\"to_day\\\\\\\":D}\\n";
  systemPrompt += "2) last_payment - the single most recent captured payment. Use for: 'last payment', 'latest transaction', 'how much was the last one'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"last_payment\\\\\\\"}\\n";
  systemPrompt += "3) last_n - the last N captured payments (sum + count). Use for: 'last 5 payments', 'recent 3 transactions'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"last_n\\\\\\\",\\\\\\\"n\\\\\\\":5}\\n";
  systemPrompt += "4) count_range - number of payments in a date range. Use for: 'how many transactions today', 'payment count this month'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"count_range\\\\\\\",\\\\\\\"from_year\\\\\\\":Y,\\\\\\\"from_month\\\\\\\":M,\\\\\\\"from_day\\\\\\\":D,\\\\\\\"to_year\\\\\\\":Y,\\\\\\\"to_month\\\\\\\":M,\\\\\\\"to_day\\\\\\\":D}\\n\\n";
  systemPrompt += "If unclear: {\\\\\\\"action\\\\\\\":\\\\\\\"error\\\\\\\",\\\\\\\"message\\\\\\\":\\\\\\\"Unable to understand\\\\\\\"}\\n\\n";
  systemPrompt += "Return JSON only, no explanations. Questions may be in English or Hindi.\"}";

  String request_body = "{\"model\":\"gpt-4o-mini\",\"messages\":[";
  request_body += systemPrompt;
  request_body += ",{\"role\":\"user\",\"content\":\"" + UserRequest + "\"}],";
  request_body += "\"temperature\":0,\"max_tokens\":200}";

  WiFiClientSecure client_tcp;
  client_tcp.setInsecure();
  if (!client_tcp.connect("api.openai.com", 443)) {
    Serial.println("Connection to OpenAI failed!");
    return "{\"error\":\"Connection failed\"}";
  }

  client_tcp.println("POST /v1/chat/completions HTTP/1.1");
  client_tcp.println("Host: api.openai.com");
  client_tcp.println("Authorization: Bearer " + cfg_openai_key);
  client_tcp.println("Content-Type: application/json; charset=utf-8");
  client_tcp.println("Content-Length: " + String(request_body.length()));
  client_tcp.println("Connection: close");
  client_tcp.println();
  client_tcp.print(request_body);

  String LLM_Response = "";
  uint32_t t_start = millis();
  while (client_tcp.connected() || client_tcp.available()) {
    if (millis() - t_start > 20000) {
      client_tcp.stop();
      return "{\"error\":\"Timeout\"}";
    }
    while (client_tcp.available()) {
      LLM_Response += (char)client_tcp.read();
    }
    delay(10);
  }
  client_tcp.stop();

  return extractJSON(LLM_Response);
}

String extractJSON(String response) {
  if (DEBUG) {
    Serial.println("\n--- RAW RESPONSE (first 500 chars) ---");
    Serial.println(response.substring(0, min(500, (int)response.length())));
    Serial.println("--- END RAW RESPONSE ---\n");
  }

  int contentStart = response.indexOf("\"content\":");
  if (contentStart < 0) {
    contentStart = response.indexOf("\"content\": \"");
    if (contentStart < 0) {
      DebugPrintln("No content field found in response");
      return "{\"error\":\"No content in response\"}";
    }
  }

  DebugPrintln("Found content field at position: " + String(contentStart));
  contentStart = response.indexOf("\"", contentStart + 10) + 1;

  if (contentStart <= 0 || contentStart >= (int)response.length()) {
    DebugPrintln("Could not find content start quote");
    return "{\"error\":\"Malformed response - no opening quote\"}";
  }

  int  contentEnd = contentStart;
  bool found      = false;
  int  iterations = 0;

  while (!found && contentEnd < (int)response.length() && iterations < 1000) {
    iterations++;
    contentEnd = response.indexOf("\"", contentEnd);
    if (contentEnd > 0) {
      if (response.charAt(contentEnd - 1) == '\\') {
        contentEnd++;
      } else {
        found = true;
      }
    } else {
      DebugPrintln("Reached end of string without finding closing quote");
      break;
    }
  }

  if (!found || contentEnd <= contentStart) {
    DebugPrintln("Could not find content end quote");
    return "{\"error\":\"Malformed response - no closing quote\"}";
  }

  String content = response.substring(contentStart, contentEnd);
  content.replace("\\n",  " ");
  content.replace("\\r",  " ");
  content.replace("\\\"", "\"");
  content.replace("\\/",  "/");
  content.replace("\\\\", "\\");
  content.trim();

  int braceStart = content.indexOf("{");
  int braceEnd   = content.lastIndexOf("}");

  if (braceStart >= 0 && braceEnd > braceStart) {
    String jsonOnly = content.substring(braceStart, braceEnd + 1);
    DebugPrintln("Final JSON: " + jsonOnly);
    return jsonOnly;
  }

  DebugPrintln("No valid JSON braces found in content");
  return "{\"error\":\"Invalid JSON format in response\"}";
}
