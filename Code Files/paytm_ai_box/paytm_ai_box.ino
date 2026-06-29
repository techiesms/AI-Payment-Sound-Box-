// ------------------------------------------------------------------------------------------------------------------------------
// AI BOX - Razorpay Voice Assistant + TFT QR Display + 4x4 Keypad + MQTT payment confirmation
// Hardware: INMP441 mic, MAX98357 speaker, TFT_eSPI display, 4x4 keypad, ESP32-S3
// ------------------------------------------------------------------------------------------------------------------------------

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SD.h>
#include <Audio.h>
#include "driver/i2s_std.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <qrcode_espi.h>
#include <Keypad.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "config.h"

// === DEBUG ===
bool DEBUG = true;

// === Detected language from the last STT response ("eng", "hin", ...) ===
// Declared here so every other .ino sees it (Arduino concatenates the main
// sketch file first, so this definition precedes all uses).
String detected_lang = "eng";

// === Audio (TTS + STT recording) globals ===
Audio    audio_play;
uint32_t gl_TOUCH_RELEASED;
uint8_t* PSRAM_BUFFER;
size_t   PSRAM_BUFFER_max_usage;
size_t   PSRAM_BUFFER_counter = 0;
bool     flg_is_recording    = false;
bool     flg_I2S_initialized = false;
int      gl_VOL_INIT = 21;

i2s_std_config_t std_cfg = {
  .clk_cfg = {
    .sample_rate_hz = SAMPLE_RATE,
    .clk_src        = I2S_CLK_SRC_DEFAULT,
    .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
  },
  .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
  .gpio_cfg = {
    .mclk = I2S_GPIO_UNUSED,
    .bclk = (gpio_num_t) I2S_SCK,
    .ws   = (gpio_num_t) I2S_WS,
    .dout = I2S_GPIO_UNUSED,
    .din  = (gpio_num_t) I2S_SD,
    .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
  },
};
i2s_chan_handle_t rx_handle;

struct WAV_HEADER {
  char  riff[4]        = {'R','I','F','F'};
  long  flength        = 0;
  char  wave[4]        = {'W','A','V','E'};
  char  fmt[4]         = {'f','m','t',' '};
  long  chunk_size     = 16;
  short format_tag     = 1;
  short num_chans      = 1;
  long  srate          = SAMPLE_RATE;
  long  bytes_per_sec  = SAMPLE_RATE * (BITS_PER_SAMPLE/8);
  short bytes_per_samp = (BITS_PER_SAMPLE/8);
  short bits_per_samp  = BITS_PER_SAMPLE;
  char  dat[4]         = {'d','a','t','a'};
  long  dlength        = 0;
} myWAV_Header;

// === Display + QR ===
TFT_eSPI    tft = TFT_eSPI();
QRcode_eSPI qrcode(&tft);
UIScreen    current_screen     = SCREEN_IDLE_PROMPT;
float       last_dynamic_amt   = 0.0f;
unsigned long success_shown_at = 0;

// === Keypad ===
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
// Rows moved off 35/36/37 — those are reserved by 8MB OPI PSRAM on ESP32-S3.
byte rowPins[KEYPAD_ROWS] = { 38, 39, 40, 41 };
byte colPins[KEYPAD_COLS] = { 15, 16, 17, 18 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
String inputText = "";

// === MQTT (payment confirmation feed) — built after config load ===
WiFiClient               mqttClient;
Adafruit_MQTT_Client*    mqtt       = nullptr;
Adafruit_MQTT_Subscribe* amountFeed = nullptr;
// Stable storage for the credential C-strings passed to Adafruit_MQTT
static char buf_aio_user[64];
static char buf_aio_key[80];
static char buf_aio_feed[128];

static void buildMqtt() {
  strncpy(buf_aio_user, cfg_aio_user.c_str(), sizeof(buf_aio_user) - 1);
  strncpy(buf_aio_key,  cfg_aio_key.c_str(),  sizeof(buf_aio_key)  - 1);
  snprintf(buf_aio_feed, sizeof(buf_aio_feed), "%s/feeds/amount", buf_aio_user);

  Serial.println("------- MQTT setup -------");
  Serial.printf("  user: '%s' (len %d)\n", buf_aio_user, (int)strlen(buf_aio_user));
  Serial.printf("  key : '%c%c%c...' (len %d)\n",
                buf_aio_key[0], buf_aio_key[1], buf_aio_key[2], (int)strlen(buf_aio_key));
  Serial.printf("  feed: %s\n", buf_aio_feed);
  Serial.printf("  host: %s:%d\n", AIO_SERVER, AIO_SERVERPORT);
  Serial.println("--------------------------");

  if (strlen(buf_aio_user) == 0 || strlen(buf_aio_key) == 0) {
    Serial.println("WARN: AIO username/key empty. Hold 'A' 3s to re-enter setup portal.");
  }

  mqtt       = new Adafruit_MQTT_Client(&mqttClient, AIO_SERVER, AIO_SERVERPORT, buf_aio_user, buf_aio_key);
  amountFeed = new Adafruit_MQTT_Subscribe(mqtt, buf_aio_feed);
  mqtt->subscribe(amountFeed);
}

// ******************************************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);

  // --- Buttons / Touch ---
  if (pin_RECORD_BTN != NO_PIN) pinMode(pin_RECORD_BTN, INPUT_PULLUP);
  if (pin_VOL_BTN    != NO_PIN) pinMode(pin_VOL_BTN,    INPUT_PULLUP);
  gl_TOUCH_RELEASED = (pin_TOUCH != NO_PIN) ? touchRead(pin_TOUCH) : 0;

  Serial.println(VERSION);

  // --- TFT ---
  tft.init();
  tft.setRotation(0);              // Portrait 320x480
  qrcode.init();
  display_init();                  // backlight on, start activity timer
  tft.fillScreen(COLOR_BG);
  drawHeader();
  drawTypingArea("0.00");

  // --- I2S mic ---
  I2S_Recording_Init();

  // --- WiFi + credential portal (blocks until configured) ---
  wifi_connectOrPortal();

  // --- NTP ---
  Serial.print("> Syncing time with NTP");  
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  time_t now = time(nullptr);
  while (now < 1700000000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(". Synced.");

  // --- Audio out ---
  audio_play.setPinout(pin_I2S_BCLK, pin_I2S_LRC, pin_I2S_DOUT);
  audio_play.setVolume(gl_VOL_INIT);   // 21 = max for Audio.h
  Serial.printf("Audio volume set to %d/21\n", gl_VOL_INIT);

  // --- Build MQTT client now that credentials are loaded ---
  buildMqtt();

  // --- Initial screen: clean idle prompt, no QR until user chooses a mode ---
  showIdlePrompt();

  TextToSpeech("Hello! AI Box ready. Enter an amount on the keypad, or hold the button to ask me about your transactions.");

  Serial.println("\n=== AI BOX READY ===");
}

// ******************************************************************************************************************************
void loop() {
  String   UserRequest;
  String   record_SDfile;
  uint8_t* record_buffer;
  long     record_bytes;
  float    record_seconds;

  // --- MQTT (payment confirmations) ---
  MQTT_connect();
  MQTT_poll();
  MQTT_serviceSuccessTimeout();

  // --- Keypad: ONE poll per loop, then route to both consumers ---
  char keypress = keypad.getKey();

  // If display was asleep, the first key press only wakes it (don't act on it)
  if (keypress) {
    if (display_isAsleep()) {
      display_userWake();   // backlight on + repaint last screen
      keypress = 0;
    } else {
      display_kick();       // just refresh the activity timer
    }
  }

  // Long-press 'A' detector (uses key edge + keypad state)
  wifi_checkResetTrigger(keypress);

  // Amount entry / QR mode handling - allow on idle, QR screens, and AI result
  if (current_screen == SCREEN_IDLE_PROMPT || current_screen == SCREEN_STATIC_QR ||
      current_screen == SCREEN_DYNAMIC_QR  || current_screen == SCREEN_AI_RESULT) {
    handleKeypad(keypress);
  }

  // --- Serial Monitor text input ---
  while (Serial.available() > 0) {
    UserRequest = Serial.readStringUntil('\n');
    UserRequest.replace("\r", "");
    UserRequest.replace("\n", "");
    UserRequest.trim();
    if (UserRequest != "") {
      Serial.println("\nYou> [" + UserRequest + "]");
    }
  }

  // --- Voice button / touch ---
  bool flg_RECORD_BTN   = (pin_RECORD_BTN != NO_PIN) ? digitalRead(pin_RECORD_BTN) : HIGH;
  bool flg_RECORD_TOUCH = false;

  if (pin_TOUCH != NO_PIN) {
    uint32_t cur = touchRead(pin_TOUCH);
    if (cur < 16383) {
      flg_RECORD_TOUCH = (cur <= (uint32_t)(gl_TOUCH_RELEASED * 0.9));
    } else {
      flg_RECORD_TOUCH = (cur >  (uint32_t)(gl_TOUCH_RELEASED * 1.1));
    }
  }

  // If the user presses the button while display is sleeping, wake the screen
  // and ignore this press cycle (so we don't immediately start recording).
  static bool btn_wakeup_active = false;
  if ((flg_RECORD_BTN == LOW || flg_RECORD_TOUCH) && display_isAsleep()) {
    display_userWake();        // wake + repaint
    btn_wakeup_active = true;
  }
  if (flg_RECORD_BTN == HIGH && !flg_RECORD_TOUCH) {
    btn_wakeup_active = false;  // cleared on release
  }

  // Start recording
  if ((flg_RECORD_BTN == LOW || flg_RECORD_TOUCH) && !btn_wakeup_active) {
    if (!flg_is_recording) {
      if (audio_play.isRunning()) {
        audio_play.stopSong();
        Serial.println("\n< STOP AUDIO >");
      }
      showVoiceStatus("Listening...", "Release to send", COLOR_ACCENT);
    }
    delay(20);
    Recording_Loop();
  }

  // Stop -> transcribe (skip if this release belongs to a wake-up press)
  if (flg_RECORD_BTN == HIGH && !flg_RECORD_TOUCH && !btn_wakeup_active) {
    if (Recording_Stop(&record_SDfile, &record_buffer, &record_bytes, &record_seconds)) {
      if (record_seconds > 0.4) {
        showVoiceStatus("Transcribing", "ElevenLabs STT", COLOR_GOLD);
        Serial.print("\nYou {STT}> ");
        UserRequest = SpeechToText_ElevenLabs(record_SDfile, record_buffer, record_bytes, "", cfg_elevenlabs_key.c_str());
        Serial.println("[" + UserRequest + "]");
      } else {
        // Too short - bail back to whatever screen we had
        returnToIdle();
      }
    }
  }

  // --- Process request (voice or serial) ---
  if (UserRequest != "") {
    showVoiceStatus("Thinking...", "Asking OpenAI", COLOR_ACCENT);

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char dateStr[50];
    strftime(dateStr, sizeof(dateStr), "%A, %B %d, %Y", timeinfo);

    Serial.print("\nAI Box> ");

    String result = OpenAI_RazorpayAssistant(UserRequest, String(dateStr));
    result.replace("\\\"", "\"");

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, result);

    if (err) {
      showAIResult("Parse error", "Could not understand the response.");
    } else {
      String action = doc["action"].as<String>();
      Serial.println("Action: " + action);

      if (action == "range_total") {
        long fromTs = dateToTimestamp(doc["from_year"], doc["from_month"], doc["from_day"], 0, 0, 0);
        long toTs   = dateToTimestamp(doc["to_year"],   doc["to_month"],   doc["to_day"],   23, 59, 59);
        showVoiceStatus("Fetching", "Razorpay API", COLOR_GOLD);
        fetchRazorpayRange(fromTs, toTs);

      } else if (action == "last_payment") {
        showVoiceStatus("Fetching", "Last payment", COLOR_GOLD);
        fetchRazorpayLast(1);

      } else if (action == "last_n") {
        int n = doc["n"].as<int>();
        if (n < 1) n = 1;
        String s = "Last " + String(n) + " payments";
        showVoiceStatus("Fetching", s.c_str(), COLOR_GOLD);
        fetchRazorpayLast(n);

      } else if (action == "count_range") {
        long fromTs = dateToTimestamp(doc["from_year"], doc["from_month"], doc["from_day"], 0, 0, 0);
        long toTs   = dateToTimestamp(doc["to_year"],   doc["to_month"],   doc["to_day"],   23, 59, 59);
        showVoiceStatus("Fetching", "Razorpay count", COLOR_GOLD);
        fetchRazorpayCount(fromTs, toTs);

      } else {
        String msg = doc["message"].as<String>();
        if (msg == "") msg = "Try: 'last payment', 'today total', 'last 5 payments'";
        showAIResult("Sorry", msg);
      }
    }
  }

  // --- Audio playback service ---
  audio_play.loop();

  // --- Display sleep timer ---
  display_serviceTimeout();

  vTaskDelay(1);
}
