#pragma once

#define VERSION "\n== AI Box - Voice + UPI ==\n"

// === DEBUG ===
extern bool DEBUG;
#define DebugPrint(x)   if(DEBUG){Serial.print(x);}
#define DebugPrintln(x) if(DEBUG){Serial.println(x);}

// === CREDENTIALS (defaults only — runtime values come from NVS via WiFiManager portal) ===
#define DEFAULT_OPENAI_KEY      ""
#define DEFAULT_ELEVENLABS_KEY  ""
#define DEFAULT_RAZORPAY_KEY    ""
#define DEFAULT_RAZORPAY_SECRET ""
#define DEFAULT_AIO_USERNAME    ""
#define DEFAULT_AIO_KEY         ""

// Config portal (SoftAP)
#define SETUP_AP_SSID  "AIBox-Setup"
#define SETUP_AP_PASS  "setup12345"

// Runtime config (set by wifi_config.ino, used everywhere else)
extern String cfg_openai_key;
extern String cfg_elevenlabs_key;
extern String cfg_razorpay_key;
extern String cfg_razorpay_secret;
extern String cfg_aio_user;
extern String cfg_aio_key;

// === HARDWARE PINS ===
// TFT_eSPI uses: 9 (DC), 10 (CS), 11 (MOSI), 12 (SCLK), 19 (MISO), 21 (RST),
//                45 (BL), 47 (TOUCH_CS). Voice path must avoid these.
// OPI PSRAM (8MB) reserves GPIO 33..37 — keypad rows MUST stay outside.
#define pin_I2S_DOUT    7
#define pin_I2S_LRC     5
#define pin_I2S_BCLK    6
#define pin_RECORD_BTN  2
#define NO_PIN          -1
#define pin_TOUCH       NO_PIN
#define pin_VOL_POTI    NO_PIN
#define pin_VOL_BTN     NO_PIN

// TFT backlight pin (must match TFT_BL in User_Setup.h)
#define PIN_TFT_BL      45
// Inactivity before display sleeps (ms). 0 = never sleep.
#define SCREEN_TIMEOUT_MS 60000UL

// === AUDIO RECORDING ===
#define RECORD_PSRAM     true
#define RECORD_SDCARD    false
#define AUDIO_FILE       "/Audio.wav"
#define SAMPLE_RATE      16000
#define BITS_PER_SAMPLE  16
#define GAIN_BOOSTER_I2S 4

// I2S Microphone pins (INMP441)
#define I2S_LR   LOW
#define I2S_WS   4
#define I2S_SD   14
#define I2S_SCK  1

// === NTP ===
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      19800   // IST (+5:30)
#define DAYLIGHT_OFFSET_SEC 0

// === MQTT (Adafruit IO) - server constants only; creds come from runtime cfg ===
#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883

// === KEYPAD (4x4) ===
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4
// rows: 35,36,37,38   cols: 15,16,17,18

// === UPI / Razorpay QR base ===
#define UPI_QR_BASE "upi://pay?ver=01&mode=19&pa=techiesms@yesbank&pn=techiesms&tr=RZPYRP2Gx8PYLfX3gXqrv2&cu=INR&mc=5065&qrMedium=04&tn=Paymenttotechiesms"

// === Display colors ===
#define COLOR_BG           0x0841
#define COLOR_CARD         0x18C3
#define COLOR_ACCENT       0x07FF
#define COLOR_WHITE        TFT_WHITE
#define COLOR_GOLD         0xFEA0
#define COLOR_SUCCESS      0x07E0
#define COLOR_SUCCESS_DARK 0x03E0
#define COLOR_TEXT_DIM     0x8410

// === UI screens ===
enum UIScreen {
  SCREEN_IDLE_PROMPT = 0,   // landing — key hints, NO QR
  SCREEN_STATIC_QR,         // open-amount QR (B)
  SCREEN_DYNAMIC_QR,        // fixed-amount QR (D)
  SCREEN_VOICE_STATUS,
  SCREEN_AI_RESULT,
  SCREEN_SUCCESS
};
