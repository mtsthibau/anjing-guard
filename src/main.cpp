/*
 * Hello World — M5Stack Cardputer Adv
 * Firmware loader: bmorcelli/Launcher  (https://bmorcelli.github.io/Launcher/)
 *
 * Hardware:
 *   Display  — ST7789V2 240×135 TFT (handled by M5Unified)
 *   Keyboard — TCA8418 I2C controller  SDA=8  SCL=9  INT=11  addr=0x34
 *   BtnA     — GPIO0 shoulder button   (handled by M5Unified)
 *   Speaker  — NS4168 I2S             (handled by M5Unified)
 *
 * Controls:
 *   Keyboard  → type characters; Enter clears; Backspace deletes last char
 *   BtnA      → clear input + ack beep
 *
 * Key remap formula (TCA8418 → physical 4×14 layout) from M5Stack source:
 *   buffer       = (rawCode & 0x7F) - 1
 *   tca_row      = buffer / 10
 *   tca_col      = buffer % 10
 *   phys_col     = tca_row * 2 + (tca_col > 3 ? 1 : 0)   → 0..13
 *   phys_row     = (tca_col + 4) % 4                       → 0..3
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <Adafruit_TCA8418.h>
#include <Wire.h>

// ─── TCA8418 config ───────────────────────────────────────────────────────────
#define KB_SDA  8
#define KB_SCL  9
#define KB_INT  11
#define KB_ADDR 0x34

Adafruit_TCA8418 tca;
bool kbReady = false;

// ─── Key tables (4 rows × 14 cols — standard Cardputer physical layout) ───────
// Modifier positions: SHIFT=r3c0  FN=r3c12  SYM=r3c11  CTRL=r2c0
// '\0' = modifier or unmapped key (handled separately)

static const char kbNorm[4][14] = {
    /* r0 */ {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b'},
    /* r1 */ {'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\0'},
    /* r2 */ {'\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\r', '\0'},
    /* r3 */ {'\0', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '\0', ' '}
};

static const char kbShft[4][14] = {
    /* r0 */ {'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b'},
    /* r1 */ {'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\0'},
    /* r2 */ {'\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '\r', '\0'},
    /* r3 */ {'\0', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '\0', '\0', ' '},
};

bool  shiftHeld = false;
bool  fnHeld    = false;
String input    = "";

// ─── Decode TCA8418 raw event → physical (row, col) ──────────────────────────
// Source: github.com/m5stack/M5Cardputer  src/utility/Keyboard/KeyboardReader/TCA8418.cpp
static void mapKey(uint8_t raw, uint8_t &row, uint8_t &col) {
    uint16_t buf = (raw & 0x7F);
    buf--;
    uint8_t tca_row = buf / 10;
    uint8_t tca_col = buf % 10;
    col = tca_row * 2 + (tca_col > 3 ? 1 : 0);
    row = (tca_col + 4) % 4;
}

// ─── Display helpers ──────────────────────────────────────────────────────────
void drawUI() {
    int w = M5.Display.width();

    // Title bar
    M5.Display.fillRect(0, 0, w, 30, 0x003366U);
    M5.Display.setTextColor(0x00BFFFU, 0x003366U);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString("Anjing Guard", w / 2, 7);

    // Hint
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x555555U, TFT_BLACK);
    M5.Display.drawString("fn+key: type   BtnA: clear", 4, 36);

    // Input label
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("Input:", 4, 52);
}

void redrawInput() {
    int w = M5.Display.width();
    M5.Display.fillRect(46, 49, w - 46, 13, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x00FF7FU, TFT_BLACK);
    // Show last 24 chars so text never overflows
    String tail = input.length() > 24 ? input.substring(input.length() - 24) : input;
    M5.Display.drawString(tail, 48, 52);
}

// Tiny debug row — shows the raw mapping for the last key press.
// Helps verify / calibrate the key table on your specific unit.
void redrawDebug(uint8_t r, uint8_t c, char ch) {
    int w = M5.Display.width();
    M5.Display.fillRect(0, 68, w, 11, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x666666U, TFT_BLACK);
    char buf[40];
    snprintf(buf, sizeof(buf), "dbg r=%u c=%u -> '%c' 0x%02X",
             r, c, (ch >= 0x20 && ch <= 0x7E) ? ch : ' ', (uint8_t)ch);
    M5.Display.drawString(buf, 4, 69);
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    drawUI();
    redrawInput();

    // Startup chime — C E G
    M5.Speaker.setVolume(64);
    M5.Speaker.tone(523, 80);  delay(90);
    M5.Speaker.tone(659, 80);  delay(90);
    M5.Speaker.tone(784, 120); delay(130);
    M5.Speaker.stop();

    // TCA8418 keyboard
    Wire.begin(KB_SDA, KB_SCL);
    kbReady = tca.begin(KB_ADDR, &Wire);
    if (kbReady) {
        tca.matrix(7, 8);
        tca.flush();
        // Use interrupt pin if wired (Cardputer Adv has INT on GPIO11)
        pinMode(KB_INT, INPUT);
        tca.enableInterrupts();
    }
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    M5.update();

    // BtnA (GPIO0 shoulder button) → clear input
    if (M5.BtnA.wasPressed()) {
        input = "";
        redrawInput();
        M5.Speaker.setVolume(48);
        M5.Speaker.tone(330, 60);
        delay(70);
        M5.Speaker.stop();
    }

    if (!kbReady) return;

    // Drain all pending TCA8418 key events
    while (tca.available() > 0) {
        int     event   = tca.getEvent();
        bool    pressed = (event & 0x80) != 0;
        uint8_t raw     = event & 0x7F;

        uint8_t row, col;
        mapKey(raw, row, col);
        if (row >= 4 || col >= 14) continue;

        // Track modifier state
        if (row == 3 && col == 0)  { shiftHeld = pressed; continue; } // SHIFT
        if (row == 3 && col == 12) { fnHeld    = pressed; continue; } // FN
        if (!pressed) continue;

        char c = shiftHeld ? kbShft[row][col] : kbNorm[row][col];

        if      (c == '\b')              { if (!input.isEmpty()) input.remove(input.length() - 1); }
        else if (c == '\r' || c == '\n') { input = ""; }
        else if (c >= 0x20 && c <= 0x7E && input.length() < 40) { input += c; }

        if (c) {
            redrawInput();
            redrawDebug(row, col, c);
        }
    }

    delay(10);
}
