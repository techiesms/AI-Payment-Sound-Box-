// ******************************************************************************************************************************
// KEYPAD - 4x4 polling. Handles amount entry, QR mode toggle, clear, backspace.
// Returns true if anything visual changed (so caller can refresh the typing area).
// Shared globals defined in paytm_ai_box.ino: keypad, inputText
// ******************************************************************************************************************************

void handleKeypad(char key) {
  if (!key) return;
  DebugPrintln(String("Keypad: ") + key);

  switch (key) {
    case 'B':                  // Back to static (open-amount) QR
      inputText = "";
      drawTypingArea("0.00");
      showStaticQR();
      break;

    case 'C':                  // Clear amount; if already empty, go back to idle
      if (inputText.length() == 0 &&
          (current_screen == SCREEN_STATIC_QR || current_screen == SCREEN_DYNAMIC_QR ||
           current_screen == SCREEN_AI_RESULT)) {
        returnToIdle();
      } else {
        inputText = "";
        drawTypingArea("0.00");
      }
      break;

    case '*':                  // Decimal point
      if (inputText.indexOf('.') == -1) {
        inputText += (inputText.length() == 0 ? "0." : ".");
        drawTypingArea(inputText);
      }
      break;

    case '#':                  // Backspace
      if (inputText.length() > 0) {
        inputText.remove(inputText.length() - 1);
        drawTypingArea(inputText.length() ? inputText : "0.00");
      }
      break;

    case 'D': {                // Generate dynamic QR
      float amt = inputText.toFloat();
      if (amt > 0) {
        showDynamicQR(amt);
      } else {
        flashAmountHint();     // visible feedback when no amount entered
      }
      break;
    }

    case 'A':                  // Reserved (could trigger voice query)
      break;

    default:
      if (key >= '0' && key <= '9') {
        if (inputText.length() < 8) {
          inputText += key;
          drawTypingArea(inputText);
        }
      }
      break;
  }
}
