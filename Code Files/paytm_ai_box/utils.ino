// ******************************************************************************************************************************
// UTILITIES - date->timestamp + amount formatting + language helpers
// ******************************************************************************************************************************

// detected_lang lives in paytm_ai_box.ino (must precede first use)
bool isHindi() { return detected_lang == "hin" || detected_lang == "hi"; }

// ------------- English number-to-words (covers 0 .. 999,999,999,999) -------------
String numberToWords(long n) {
  if (n < 0)  return "negative " + numberToWords(-n);
  if (n == 0) return "zero";

  static const char* ones[] = {
    "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
    "seventeen", "eighteen", "nineteen"
  };
  static const char* tens[] = {
    "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
  };

  String r;

  if (n >= 1000000000L) {
    r += numberToWords(n / 1000000000L) + " billion ";
    n %= 1000000000L;
  }
  if (n >= 1000000L) {
    r += numberToWords(n / 1000000L) + " million ";
    n %= 1000000L;
  }
  if (n >= 1000) {
    r += numberToWords(n / 1000) + " thousand ";
    n %= 1000;
  }
  if (n >= 100) {
    r += String(ones[n / 100]) + " hundred ";
    n %= 100;
  }
  if (n >= 20) {
    r += tens[n / 10];
    if (n % 10) { r += " "; r += ones[n % 10]; }
  } else if (n > 0) {
    r += ones[n];
  }

  r.trim();
  while (r.indexOf("  ") >= 0) r.replace("  ", " ");
  return r;
}

// Spoken-friendly amount in words: "four thousand three hundred twelve rupees"
String speakRupees(long paise) {
  long rupees = paise / 100;
  long pse    = paise % 100;
  if (pse == 0) return numberToWords(rupees) + " rupees";
  return numberToWords(rupees) + " rupees and " + numberToWords(pse) + " paise";
}

// Hindi version: leaves the numeral as-is for now (Devanagari word table is large).
// Add a hindiNumberToWords() later if the same mispronunciation appears in Hindi.
String speakRupeesHindi(long paise) {
  long rupees = paise / 100;
  long pse    = paise % 100;
  if (pse == 0) return String(rupees) + " रुपये";
  return String(rupees) + " रुपये " + String(pse) + " पैसे";
}

// Pretty short date for spoken output: "October 5"
String spokenDate(time_t ts) {
  struct tm* tm = localtime(&ts);
  char b[32];
  strftime(b, sizeof(b), "%B %e", tm);
  return String(b);
}

String spokenTime(time_t ts) {
  struct tm* tm = localtime(&ts);
  char b[16];
  strftime(b, sizeof(b), "%H:%M", tm);
  return String(b);
}

long dateToTimestamp(int year, int month, int day, int hour, int minute, int second) {
  struct tm t;
  t.tm_year  = year - 1900;
  t.tm_mon   = month - 1;
  t.tm_mday  = day;
  t.tm_hour  = hour;
  t.tm_min   = minute;
  t.tm_sec   = second;
  t.tm_isdst = -1;

  time_t timestamp = mktime(&t);
  return (long)timestamp;
}
