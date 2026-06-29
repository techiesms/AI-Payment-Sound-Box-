// ******************************************************************************************************************************
// I2S MIC RECORDING (INMP441) - init, capture loop, stop
// Shared globals defined in paytm_ai_box.ino: PSRAM_BUFFER, PSRAM_BUFFER_counter,
// PSRAM_BUFFER_max_usage, flg_is_recording, flg_I2S_initialized, std_cfg, rx_handle, myWAV_Header
// ******************************************************************************************************************************

bool I2S_Recording_Init() {
  if (I2S_LR == HIGH) std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
  if (I2S_LR == LOW)  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  // Pin the mic to I2S_NUM_1. Audio.h's TTS path uses I2S_NUM_0 internally;
  // sharing the same unit makes the mic go silent after the first TTS playback.
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);

  DebugPrintln("> I2S_Recording_Init - Initializing:");
  DebugPrintln("- Total PSRAM: " + String(ESP.getPsramSize()));
  DebugPrintln("- Free PSRAM:  " + String(ESP.getFreePsram()));

  if (RECORD_PSRAM) {
    if (ESP.getFreePsram() > 0) {
      PSRAM_BUFFER_max_usage = ESP.getFreePsram() / 2;
      int max_seconds = (PSRAM_BUFFER_max_usage / (SAMPLE_RATE * BITS_PER_SAMPLE/8));

      if (max_seconds < 5) {
        Serial.println("* ERROR - Not enough free PSRAM!");
        while (true);
      } else {
        PSRAM_BUFFER = (uint8_t*) ps_malloc(PSRAM_BUFFER_max_usage);
        DebugPrintln("> PSRAM allocated: " + String(PSRAM_BUFFER_max_usage) + " bytes");
        DebugPrintln("> Max recording length: " + String(max_seconds) + " sec\n");
      }
    } else {
      Serial.println("* ERROR - No PSRAM found!");
      while (true);
    }
  }

  flg_I2S_initialized = true;
  return flg_I2S_initialized;
}

bool Recording_Loop() {
  if (!flg_I2S_initialized) {
    Serial.println("* ERROR - I2S not initialized!");
    return false;
  }

  if (!flg_is_recording) {
    flg_is_recording = true;
    if (RECORD_PSRAM) {
      PSRAM_BUFFER_counter = 44;
      for (int i = 0; i < (int)PSRAM_BUFFER_counter; i++) {
        PSRAM_BUFFER[i] = ((uint8_t*)&myWAV_Header)[i];
      }
      DebugPrintln("> Recording started...");
    }
  }

  if (flg_is_recording) {
    int16_t audio_buffer[1024];
    size_t  bytes_read = 0;

    i2s_channel_read(rx_handle, audio_buffer, sizeof(audio_buffer), &bytes_read, portMAX_DELAY);
    size_t values_recorded = bytes_read / 2;

    // Gain boost
    if (GAIN_BOOSTER_I2S > 1 && GAIN_BOOSTER_I2S <= 64) {
      for (size_t i = 0; i < values_recorded; ++i) {
        int32_t amp = (int32_t)audio_buffer[i] * GAIN_BOOSTER_I2S;
        if (amp >  32767) amp =  32767;
        if (amp < -32768) amp = -32768;
        audio_buffer[i] = (int16_t)amp;
      }
    }

    if (RECORD_PSRAM) {
      if (PSRAM_BUFFER_counter + values_recorded * 2 < PSRAM_BUFFER_max_usage) {
        memcpy((PSRAM_BUFFER + PSRAM_BUFFER_counter), audio_buffer, values_recorded * 2);
        PSRAM_BUFFER_counter += values_recorded * 2;
      }
    }
  }
  return true;
}

bool Recording_Stop(String* audio_filename, uint8_t** buff_start, long* audiolength_bytes, float* audiolength_sec) {
  if (!flg_is_recording)    return false;
  if (!flg_I2S_initialized) return false;

  flg_is_recording = false;

  *audio_filename    = "";
  *buff_start        = NULL;
  *audiolength_bytes = 0;
  *audiolength_sec   = 0;

  if (RECORD_PSRAM) {
    myWAV_Header.flength = (long) PSRAM_BUFFER_counter - 8;
    myWAV_Header.dlength = (long) PSRAM_BUFFER_counter - 44;

    for (int i = 0; i < 44; i++) {
      PSRAM_BUFFER[i] = ((uint8_t*)&myWAV_Header)[i];
    }

    *buff_start        = PSRAM_BUFFER;
    *audiolength_bytes = PSRAM_BUFFER_counter;
    *audiolength_sec   = (float)(PSRAM_BUFFER_counter - 44) / (SAMPLE_RATE * BITS_PER_SAMPLE/8);

    DebugPrintln("> Recording done. Length: " + String(*audiolength_sec) + " sec");
  }
  return true;
}
