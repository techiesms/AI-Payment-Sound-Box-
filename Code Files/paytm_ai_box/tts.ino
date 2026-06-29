// ******************************************************************************************************************************
// TEXT TO SPEECH via OpenAI (streamed by Audio.h into MAX98357)
// ******************************************************************************************************************************

void TextToSpeech(String p_request) {
  if (p_request == "") return;

  Serial.print("TTS Audio [Razorpay Assistant | onyx]");
  Serial.println();

  String model  = "tts-1";
  String voice  = "onyx";
  String vspeed = "1.0";

  audio_play.openai_speech(cfg_openai_key, model, p_request, "", voice, "aac", vspeed);
}

// Audio.h callback (optional)
void audio_info(const char* info) {
  // String info_str = (String) info;
  // if (info_str.indexOf("skipCRLF") == -1) DebugPrintln("AUDIO.H info: " + info_str);
}
