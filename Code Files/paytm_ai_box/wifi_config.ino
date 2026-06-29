// ******************************************************************************************************************************
// WIFI + CREDENTIAL MANAGEMENT
//   - Persistent storage in NVS via Preferences.
//   - WiFiManager captive portal for first boot or "forget config" trigger.
//   - Custom fields: OpenAI key, ElevenLabs key, Razorpay key/secret, Adafruit IO user/key.
// ******************************************************************************************************************************

#include <Preferences.h>
#include <WiFiManager.h>

// Runtime config values (declared extern in config.h)
String cfg_openai_key;
String cfg_elevenlabs_key;
String cfg_razorpay_key;
String cfg_razorpay_secret;
String cfg_aio_user;
String cfg_aio_key;

static Preferences prefs;
static const char* PREF_NS = "aibox";

static void config_load() {
  prefs.begin(PREF_NS, true);  // read-only
  cfg_openai_key      = prefs.getString("openai",      DEFAULT_OPENAI_KEY);
  cfg_elevenlabs_key  = prefs.getString("eleven",      DEFAULT_ELEVENLABS_KEY);
  cfg_razorpay_key    = prefs.getString("rzp_key",     DEFAULT_RAZORPAY_KEY);
  cfg_razorpay_secret = prefs.getString("rzp_secret",  DEFAULT_RAZORPAY_SECRET);
  cfg_aio_user        = prefs.getString("aio_user",    DEFAULT_AIO_USERNAME);
  cfg_aio_key         = prefs.getString("aio_key",     DEFAULT_AIO_KEY);
  prefs.end();
}

static void config_save(const char* openai, const char* eleven,
                        const char* rzp_key, const char* rzp_secret,
                        const char* aio_user, const char* aio_key) {
  prefs.begin(PREF_NS, false);
  prefs.putString("openai",     openai);
  prefs.putString("eleven",     eleven);
  prefs.putString("rzp_key",    rzp_key);
  prefs.putString("rzp_secret", rzp_secret);
  prefs.putString("aio_user",   aio_user);
  prefs.putString("aio_key",    aio_key);
  prefs.end();
}

// Wipe all stored credentials + WiFi config, then reboot into portal mode.
void config_resetAndReboot() {
  Serial.println("Wiping config and rebooting into setup portal...");

  WiFiManager wm;
  wm.resetSettings();   // clears WiFi creds

  prefs.begin(PREF_NS, false);
  prefs.clear();
  prefs.end();

  delay(500);
  ESP.restart();
}

// Show "setup mode" info on the TFT while the portal is up
static void drawSetupScreen() {
  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 0, 320, 4, COLOR_GOLD);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 20);
  tft.print("SETUP MODE");

  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(20, 80);
  tft.print("1. Connect phone to WiFi:");
  tft.setTextColor(COLOR_GOLD);
  tft.setCursor(20, 110);
  tft.print(SETUP_AP_SSID);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(2);
  tft.setCursor(20, 135);
  tft.print("Pwd: " SETUP_AP_PASS);

  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(20, 180);
  tft.print("2. Open browser:");
  tft.setTextColor(COLOR_GOLD);
  tft.setCursor(20, 210);
  tft.print("http://192.168.4.1");

  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(20, 260);
  tft.print("3. Fill WiFi + API keys");

  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(20, 320);
  tft.print("Hold 'A' key for 3 sec later to re-enter setup.");
}

// Blocking: connects to known WiFi, or opens the portal until the user saves.
// Returns once WiFi is connected (or after ESP restart from portal save).
void wifi_connectOrPortal() {
  config_load();

  WiFiManager wm;
  wm.setDebugOutput(DEBUG);
  wm.setConfigPortalTimeout(0);  // stay open until user finishes
  wm.setBreakAfterConfig(true);

  // Custom fields on the portal form
  WiFiManagerParameter p_openai("openai",  "OpenAI API key",         cfg_openai_key.c_str(),      192);
  WiFiManagerParameter p_eleven("eleven",  "ElevenLabs API key",     cfg_elevenlabs_key.c_str(),  96);
  WiFiManagerParameter p_rzp_k ("rzp_k",   "Razorpay key id",        cfg_razorpay_key.c_str(),    48);
  WiFiManagerParameter p_rzp_s ("rzp_s",   "Razorpay key secret",    cfg_razorpay_secret.c_str(), 48);
  WiFiManagerParameter p_aio_u ("aio_u",   "Adafruit IO username",   cfg_aio_user.c_str(),        32);
  WiFiManagerParameter p_aio_k ("aio_k",   "Adafruit IO key",        cfg_aio_key.c_str(),         64);

  wm.addParameter(&p_openai);
  wm.addParameter(&p_eleven);
  wm.addParameter(&p_rzp_k);
  wm.addParameter(&p_rzp_s);
  wm.addParameter(&p_aio_u);
  wm.addParameter(&p_aio_k);

  // Called when portal starts (no saved WiFi or after reset). Show instructions on TFT.
  wm.setAPCallback([](WiFiManager* mgr) {
    Serial.println("Config portal started.");
    drawSetupScreen();
  });

  bool ok = wm.autoConnect(SETUP_AP_SSID, SETUP_AP_PASS);
  if (!ok) {
    Serial.println("WiFi connect failed; restarting.");
    delay(2000);
    ESP.restart();
  }

  // Save whatever the user typed (will equal current values on a non-portal run)
  config_save(p_openai.getValue(), p_eleven.getValue(),
              p_rzp_k.getValue(),  p_rzp_s.getValue(),
              p_aio_u.getValue(),  p_aio_k.getValue());

  // Reload into globals
  config_load();

  Serial.println("WiFi connected: " + WiFi.SSID() + "  IP: " + WiFi.localIP().toString());
}

// Called once per loop with the key value polled in the main loop.
// Long-press 'A' for 3 s → wipe config + reboot into setup portal.
void wifi_checkResetTrigger(char key) {
  static unsigned long press_start = 0;

  if (key == 'A' && press_start == 0) {
    press_start = millis();
    DebugPrintln("Reset trigger armed: hold 'A' for 3 sec.");
  }

  // While 'A' (or any key) is physically down, the library reports PRESSED/HOLD.
  KeyState st = keypad.getState();
  if (press_start != 0 && (st == PRESSED || st == HOLD)) {
    if ((millis() - press_start) > 3000) {
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_GOLD);  tft.setTextSize(3);
      tft.setCursor(20, 100); tft.print("RESETTING");
      tft.setTextColor(COLOR_WHITE); tft.setTextSize(2);
      tft.setCursor(20, 150); tft.print("Reboot into setup...");
      config_resetAndReboot();
    }
  } else if (st == IDLE || st == RELEASED) {
    press_start = 0;
  }
}
