// ******************************************************************************************************************************
// TFT DISPLAY - layout, QR rendering, screen-state transitions, sleep/wake
// Shared globals defined in paytm_ai_box.ino: tft, qrcode, current_screen, last_dynamic_amt
// ******************************************************************************************************************************

// ---- Display sleep / wake ----------------------------------------------------
static bool          display_asleep   = false;
static unsigned long last_activity_ms = 0;

void display_init() {
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  display_asleep   = false;
  last_activity_ms = millis();
}

// Forward decl - redraws whichever screen was active before sleeping
void redrawCurrentScreen();

void display_wake() {
  if (!display_asleep) return;
  digitalWrite(PIN_TFT_BL, HIGH);
  display_asleep = false;
  Serial.println("Display: wake");
}

void display_sleep() {
  if (display_asleep) return;
  // Blank the panel BEFORE killing the backlight so no faint white card / QR
  // is visible while the backlight ramps down.
  tft.fillScreen(TFT_BLACK);
  digitalWrite(PIN_TFT_BL, LOW);
  display_asleep = true;
  Serial.println("Display: sleep (screen cleared, backlight off)");
}

bool display_isAsleep() { return display_asleep; }

// Called from inside draw functions — just refreshes the timer and turns the
// backlight back on if needed. Does NOT redraw, because the caller will.
void display_kick() {
  last_activity_ms = millis();
  if (display_asleep) display_wake();
}

// Called from the input wake path (keypad/button). Wakes + repaints whatever
// screen was up so the user sees something instead of a half-erased frame.
void display_userWake() {
  last_activity_ms = millis();
  if (display_asleep) {
    display_wake();
    redrawCurrentScreen();
  }
}

// Call once per loop iteration
void display_serviceTimeout() {
  if (SCREEN_TIMEOUT_MS == 0) return;
  if (!display_asleep && (millis() - last_activity_ms) > SCREEN_TIMEOUT_MS) {
    display_sleep();
  }
}

// ---- UI drawing -------------------------------------------------------------

void drawHeader() {
  display_kick();
  tft.fillRect(0, 0, 320, 65, COLOR_BG);
  tft.fillRect(0, 0, 320, 4, COLOR_ACCENT);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(3);
  tft.setCursor(15, 12);
  tft.print("AI Box");
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(15, 48);
  tft.print("[D] Pay  [B] QR  [C] Clr  [#] Del  [Hold] Ask");
}

void drawTypingArea(String text) {
  display_kick();
  tft.fillRect(0, 65, 320, 85, COLOR_BG);
  tft.fillRoundRect(12, 70, 296, 75, 10, COLOR_CARD);
  tft.fillRect(12, 70, 6, 75, COLOR_ACCENT);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(30, 78);
  tft.print("ENTER AMOUNT");
  tft.setTextColor(COLOR_GOLD);
  tft.setTextSize(3);
  tft.setCursor(30, 95);
  tft.print("Rs.");
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(4);
  tft.setCursor(100, 95);
  tft.print(text);
}

// --- Idle landing screen: no QR, just instructions ---
void showIdlePrompt() {
  display_kick();
  current_screen = SCREEN_IDLE_PROMPT;
  tft.fillRect(0, 150, 320, 330, COLOR_BG);

  // Card
  tft.fillRoundRect(20, 180, 280, 290, 14, COLOR_CARD);
  tft.drawRoundRect(20, 180, 280, 290, 14, COLOR_ACCENT);

  // Title pill
  tft.fillRoundRect(80, 165, 160, 32, 16, COLOR_ACCENT);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(115, 173);
  tft.print("READY");

  // Hints
  const int x_key = 40, x_lbl = 110, y0 = 220, dy = 42;
  tft.setTextSize(2);

  tft.setTextColor(COLOR_GOLD);  tft.setCursor(x_key, y0 + 0*dy);  tft.print("[B]");
  tft.setTextColor(COLOR_WHITE); tft.setCursor(x_lbl, y0 + 0*dy);  tft.print("Open-amount QR");

  tft.setTextColor(COLOR_GOLD);  tft.setCursor(x_key, y0 + 1*dy);  tft.print("[D]");
  tft.setTextColor(COLOR_WHITE); tft.setCursor(x_lbl, y0 + 1*dy);  tft.print("Fixed-amount QR");

  tft.setTextColor(COLOR_GOLD);  tft.setCursor(x_key, y0 + 2*dy);  tft.print("[#]");
  tft.setTextColor(COLOR_WHITE); tft.setCursor(x_lbl, y0 + 2*dy);  tft.print("Backspace");

  tft.setTextColor(COLOR_GOLD);  tft.setCursor(x_key, y0 + 3*dy);  tft.print("[*]");
  tft.setTextColor(COLOR_WHITE); tft.setCursor(x_lbl, y0 + 3*dy);  tft.print("Decimal");

  // Voice hint
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(40, 425);
  tft.print("Hold button: ask AI");
}

// --- Static (open-amount) QR ---
// IMPORTANT: badge + footer text are drawn AFTER qrcode.create() so they
// sit on top of the QR's white quiet zone instead of being overwritten.
void showStaticQR() {
  display_kick();
  current_screen = SCREEN_STATIC_QR;
  tft.fillRect(0, 150, 320, 330, COLOR_BG);

  // QR card
  tft.fillRoundRect(28, 190, 264, 270, 12, COLOR_WHITE);
  tft.drawRoundRect(28, 190, 264, 270, 12, COLOR_ACCENT);

  // QR (draw FIRST so we can paint badge + footer on top)
  qrcode.setQRPosition(48, 205, 5);
  qrcode.create(String(UPI_QR_BASE));

  // Mode badge — straddles the top edge of the card, painted on top of QR
  tft.fillRoundRect(40, 158, 240, 36, 18, COLOR_ACCENT);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(60, 168);
  tft.print("OPEN AMOUNT MODE");

  // Footer band — solid white strip so dark text is fully legible
  tft.fillRoundRect(36, 425, 248, 30, 8, COLOR_WHITE);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(42, 432);
  tft.print("Scan & enter amount");
}

// --- Dynamic (fixed-amount) QR ---
void showDynamicQR(float amt) {
  display_kick();
  current_screen   = SCREEN_DYNAMIC_QR;
  last_dynamic_amt = amt;

  tft.fillRect(0, 150, 320, 330, COLOR_BG);

  // QR card
  tft.fillRoundRect(28, 190, 264, 270, 12, COLOR_WHITE);
  tft.drawRoundRect(28, 190, 264, 270, 12, COLOR_GOLD);

  // QR
  String qr = String(UPI_QR_BASE) + "&am=" + String(amt, 2);
  qrcode.setQRPosition(48, 205, 5);
  qrcode.create(qr);

  // Mode badge (on top of QR quiet zone)
  tft.fillRoundRect(40, 158, 240, 36, 18, COLOR_GOLD);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(60, 168);
  tft.print("FIXED AMOUNT MODE");

  // Footer band with the amount in large type, on top of QR quiet zone
  tft.fillRoundRect(36, 420, 248, 36, 8, COLOR_WHITE);
  tft.setTextColor(COLOR_GOLD);
  tft.setTextSize(3);
  String amtStr = "Rs. " + String(amt, 2);
  int textW = amtStr.length() * 18;        // approx char width @ size 3
  int x = 160 - textW / 2;
  if (x < 40) x = 40;
  tft.setCursor(x, 427);
  tft.print(amtStr);
}

// Thick anti-jagged tick centered at (cx, cy)
static void drawCheckmark(int cx, int cy, uint16_t color, int thickness) {
  // Three points of the tick: short stroke + long stroke
  int x1 = cx - 24, y1 = cy + 2;    // bottom-left start
  int x2 = cx - 6,  y2 = cy + 22;   // bottom vertex
  int x3 = cx + 28, y3 = cy - 22;   // top-right end

  int half = thickness / 2;
  for (int t = -half; t <= half; t++) {
    tft.drawLine(x1, y1 + t, x2, y2 + t, color);
    tft.drawLine(x2, y2 + t, x3, y3 + t, color);
    tft.drawLine(x1 + t, y1, x2 + t, y2, color);
    tft.drawLine(x2 + t, y2, x3 + t, y3, color);
  }
  // Rounded endcaps + vertex
  tft.fillCircle(x1, y1, half, color);
  tft.fillCircle(x2, y2, half, color);
  tft.fillCircle(x3, y3, half, color);
}

void showPaymentReceived(String amt) {
  display_kick();   // always wake on payment
  current_screen = SCREEN_SUCCESS;
  tft.fillScreen(COLOR_BG);

  // Outer dim ring for a glow effect
  tft.fillCircle(160, 130, 80, COLOR_SUCCESS_DARK);
  tft.fillCircle(160, 130, 65, COLOR_SUCCESS);

  // White tick inside the green circle
  drawCheckmark(160, 130, TFT_WHITE, 8);

  // Centered "PAYMENT RECEIVED" (size 3 ≈ 18 px per char, screen width 320)
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);
  {
    const char* w = "PAYMENT";
    int px = 160 - (int)strlen(w) * 18 / 2;
    tft.setCursor(px, 240);
    tft.print(w);
  }
  tft.setTextColor(COLOR_SUCCESS);
  {
    const char* w = "RECEIVED";
    int px = 160 - (int)strlen(w) * 18 / 2;
    tft.setCursor(px, 275);
    tft.print(w);
  }

  tft.fillRoundRect(30, 330, 260, 70, 12, COLOR_CARD);
  tft.drawRoundRect(30, 330, 260, 70, 12, COLOR_SUCCESS);
  tft.setTextColor(COLOR_GOLD);
  tft.setTextSize(3);
  String s = "Rs. " + amt;
  int w = s.length() * 18;
  int x = 160 - w / 2;
  if (x < 40) x = 40;
  tft.setCursor(x, 350);
  tft.print(s);
}

// Returns to idle (called after success screen / cancellations)
void returnToIdle() {
  inputText = "";
  tft.fillScreen(COLOR_BG);
  drawHeader();
  drawTypingArea("0.00");
  showIdlePrompt();   // no QR until the user picks a mode
}

// --- Voice / AI status overlays ---
// We reuse the lower QR area for status text instead of wiping the whole screen,
// so the header + typing area stay visible.

void showVoiceStatus(const char* line1, const char* line2, uint16_t accent) {
  display_kick();
  current_screen = SCREEN_VOICE_STATUS;
  tft.fillRect(0, 150, 320, 330, COLOR_BG);
  tft.fillRoundRect(20, 180, 280, 220, 12, COLOR_CARD);
  tft.drawRoundRect(20, 180, 280, 220, 12, accent);

  tft.fillRoundRect(60, 168, 200, 26, 13, accent);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(100, 176);
  tft.print("VOICE ASSISTANT");

  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(3);
  tft.setCursor(40, 240);
  tft.print(line1);

  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(40, 290);
  tft.print(line2);
}

// Wrap a long string into the response card
void showAIResult(String title, String body) {
  display_kick();
  current_screen = SCREEN_AI_RESULT;
  tft.fillRect(0, 150, 320, 330, COLOR_BG);
  tft.fillRoundRect(20, 180, 280, 280, 12, COLOR_CARD);
  tft.drawRoundRect(20, 180, 280, 280, 12, COLOR_GOLD);

  tft.fillRoundRect(60, 168, 200, 26, 13, COLOR_GOLD);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(105, 176);
  tft.print("AI RESPONSE");

  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(35, 205);
  tft.print(title);

  // body: simple word-wrap
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  int x = 35, y = 245;
  const int x_max = 290, line_h = 22, char_w = 12;
  int max_chars = (x_max - x) / char_w;

  int idx = 0;
  while (idx < (int)body.length() && y < 440) {
    int end = idx + max_chars;
    if (end >= (int)body.length()) end = body.length();
    else {
      int space = body.lastIndexOf(' ', end);
      if (space > idx) end = space;
    }
    String line = body.substring(idx, end);
    line.trim();
    tft.setCursor(x, y);
    tft.print(line);
    y += line_h;
    idx = end + 1;
  }
}

// Repaints whichever screen we were on before sleep. Used by display_userWake.
void redrawCurrentScreen() {
  tft.fillScreen(COLOR_BG);
  drawHeader();
  drawTypingArea(inputText.length() ? inputText : "0.00");
  switch (current_screen) {
    case SCREEN_STATIC_QR:  showStaticQR();                 break;
    case SCREEN_DYNAMIC_QR: showDynamicQR(last_dynamic_amt); break;
    case SCREEN_IDLE_PROMPT:
    default:                showIdlePrompt();               break;
    // SCREEN_VOICE_STATUS, SCREEN_AI_RESULT, SCREEN_SUCCESS are transient —
    // fall through to idle when waking from sleep on those.
  }
}

// --- "Enter amount first" hint flash for D-with-empty-input ---
void flashAmountHint() {
  display_kick();
  tft.fillRect(0, 65, 320, 85, COLOR_BG);
  tft.fillRoundRect(12, 70, 296, 75, 10, 0x8000);  // dark red card
  tft.fillRect(12, 70, 6, 75, TFT_RED);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 88);
  tft.print("Enter amount first");
  tft.setTextSize(1);
  tft.setTextColor(0xFB6D);
  tft.setCursor(30, 115);
  tft.print("Type digits, then press D again");
  delay(1500);
  drawTypingArea(inputText.length() ? inputText : "0.00");
}
