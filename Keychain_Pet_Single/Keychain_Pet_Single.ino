/*
  Keychain Pet — animated pixel scenes on an SSD1306 OLED (ESP8266)
  -----------------------------------------------------------------
  Libraries (Arduino IDE > Library Manager):
    - Adafruit SSD1306
    - Adafruit GFX Library
  Board: "Generic ESP8266 Module"
  Flash Size: 4MB (FS:2MB OTA:~1019KB)   <-- needs a filesystem for the pictures

  Animations:
    1. Mermaid tail - swishes, flicks water, sparkles and bubbles
    2. Shell        - opens and closes, the pearl shines
    3. Beach        - waves wash up the sand, starfish, seagull
    4. Starry night - swirling sky, pulsing stars, swaying cypress
        5. Robot eyes   - glance around, blink, and change mood
    6. Tea          - steam curls up from a teacup, the tea bag tag swings
    7. Dragon       - wings flap, and every few seconds it breathes fire
    8. Roses        - a rose bush sways in gusts of wind, petals blow away

  What plays is a playlist of up to PLAY_MAX items, each an animation or
  a picture sent from a phone. They play in order, `seconds` each (set
  from the phone). A playlist with one item just keeps showing it.

  The phone page (no app, no internet needed):
    For a while after switching on, the keychain makes its own Wi-Fi
    network (AP_NAME / AP_PASS). Join it on the phone and open 192.168.4.1.
    There you can add pictures and animations, remove them, and change
    the order. The phone turns photos into 128x64 black and white itself.
    Everything is saved in flash, so it survives switching off. The
    Wi-Fi turns itself off after AP_BOOT_SECONDS if nobody opens the
    page, or AP_IDLE_MINUTES after the page was last used.

  Updating the firmware, any of these:
    - 192.168.4.1/update from a phone or laptop on the keychain's Wi-Fi
      (user "admin", password OTA_PASSWORD), with the .bin from
      Sketch > Export Compiled Binary
    - Arduino IDE with the laptop joined to the keychain's Wi-Fi:
      pick the OTA_NAME network port (within AP_BOOT_SECONDS of switching on)
    - Wi-Fi update mode: touch a wire between GPIO14 and GND while
      switching on; it then joins WIFI_SSID instead (your own hotspot)
*/

#include <Wire.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <EEPROM.h>

// ======================= CONFIG =======================
#define SCREEN_W 128
#define SCREEN_H 64     // 64 for 0.96" OLED, 32 for 0.91" OLED
#define OLED_ADDR 0x3C  // try 0x3D if the screen stays black

// I2C pins  (ESP-12E/F or NodeMCU: SDA = 4 (D2), SCL = 5 (D1))
#define SDA_PIN 4
#define SCL_PIN 5

#define ROTATION 0  // 0/2 = landscape, 1/3 = portrait

#define OTA_PIN 14  // wire this pin to GND at power-on for Wi-Fi update mode

#define SCENE_SECONDS 20  // how long each item plays at first; the phone page can change it

// ---- Wi-Fi update mode (GPIO14 wire): joins YOUR network ----
#define ENABLE_OTA 1
#define WIFI_SSID "Devara"
#define WIFI_PASS "thangamwedsvara"
#define OTA_NAME "keychain-pet"
#define OTA_PASSWORD "pet1234"

#define BATTERY_MONITOR 1  // blinking battery icon when the battery gets low
#define LOW_VCC 3.50f

// ---- Phone page: the keychain's OWN Wi-Fi, on for a while after switch-on ----
#define ENABLE_PHOTO_WIFI 1
#define AP_NAME "Keychain-Pet"
#define AP_PASS "keychain123"  // 8 characters or more
#define AP_BOOT_SECONDS 60           // Wi-Fi off after this long if nobody opens the page
#define AP_IDLE_MINUTES 2            // ...or this long after the page was last used
#define AP_POWER_DBM 13        // phone is right next to it; less = smaller current spikes
#define PLAY_MAX 50            // most items in the playlist (each picture takes one \
                               // 8 KB flash block: 50 = ~400 KB of the 2 MB)
#define SPLASH_MS 4000         // "send me a picture" screen at switch-on
// ======================================================

enum Scene { SCENE_TAIL, SCENE_SHELL, SCENE_BEACH, SCENE_STARRY, SCENE_EYES,
             SCENE_TEA, SCENE_DRAGON, SCENE_ROSES, SCENE_PHOTO };

#define PHOTO_BYTES (SCREEN_W * SCREEN_H / 8)  // 1024, same layout as the screen buffer
#define KIND_ANIM 0
#define KIND_PIC 1

// live animations a picture can have (the phone page lists them in this order)
enum Effect { FX_STILL,
              FX_SWAY,
              FX_BOB,
              FX_SPARKLE,
              FX_HEARTS,
              FX_SNOW,
              FX_RIPPLE,
              FX_PULSE,
              FX_GLITCH,
              FX_COUNT };

#if BATTERY_MONITOR
ADC_MODE(ADC_VCC);
#endif

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// progress / message screen used by the firmware updaters
void otaScreen(const char *line1, const char *line2, int percent) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 4);
  display.print(line1);
  if (line2) {
    display.setCursor(2, 18);
    display.print(line2);
  }
  if (percent >= 0) {
    int w = display.width() - 8;
    display.drawRect(4, display.height() - 16, w, 10, SSD1306_WHITE);
    display.fillRect(6, display.height() - 14, (w - 4) * percent / 100, 6, SSD1306_WHITE);
  }
  display.display();
}

// ======================= PIXEL ART =======================
// Auto-generated from the two pixel-art images

// Mermaid tail  '#' = lit, 'o' = dark, '.' = transparent
#define TAIL_W 43
#define TAIL_H 45
static const char TAIL[] PROGMEM =
  "...............................o..........."
  "..............................oo..........."
  ".............................o#o..........."
  ".............................o#o..........."
  "............................o##o..........."
  "..........................oo##oo..........."
  ".........................o###o#o..........."
  "........................o###o##o..........."
  ".......................oo###o##o..........."
  ".......................o####o##o..........."
  ".....................ooo####o##o..........."
  "....................o###o###o##o..........."
  "o..................o####o##o###o..........."
  "ooooo.oooo....oo...o####o##o###o..........."
  ".o###o####oooo##oo.o####o#o###o............"
  ".o####o####o######o####o##o###o............"
  ".o####o#####o#####o####o#o####o............"
  "..o####o#####o####o####o#o##oo............."
  "..o#####o#####o####o###o#o##o.............."
  "...o#####oo####o###o##o#o##o..............."
  "....o######o####o##o##o#o#o................"
  ".....o######oo###o##o#o#o#o................"
  "......o#######o###o#o##o#o................."
  ".......oo######oo##o#o#o#o................."
  ".........ooo#####o##oo#ooo................."
  "............ooo###ooooooo.................."
  "...............ooo..o###o.................."
  "....................ooooo.................."
  "....................o###o.................."
  "....................o####o................."
  "....................oo#o#o................."
  "...................o##o###o................"
  "...................o#oooo#o................"
  "..................o#o####o#o..............."
  "..................oo#####o##o.............."
  ".................o#oo###oo##o.............."
  ".................o###o#o####oo............."
  "................oo####o####o#o............."
  "................o#o##o#o##o##o............."
  "................o##oo###oo####o............"
  "...............o###oo###oo###oo............"
  "...............oo#o##o#o##o#o##o..........."
  "...............o#o####o####o###o..........."
  "...............oooo##ooo##oooo##oo........."
  "..............oo###oo###oo####o#o#o........";

// Water under the tail (ripples)
#define WAVES_W 43
#define WAVES_H 4
static const char WAVES[] PROGMEM =
  "............oo#oo#o###o#o###o######o......."
  "...........o##o#####o#####o#####o##o......."
  "............o###ooo###o.o###oooo##o........"
  ".............ooo...ooo...ooo....oo.........";

static const int8_t TAIL_SPARKLES[][2] = { { 36, 20 }, { 41, 28 }, { 3, 29 }, { 9, 35 }, { 37, 39 }, { 5, 41 } };
#define TAIL_SPARKLE_N 6

// Upper shell (opens / closes)
#define LID_W 36
#define LID_H 23
static const char LID[] PROGMEM =
  "...............oooooo..............."
  "............ooo######ooo............"
  "...........ooo########ooo..........."
  "........oooo############oooo........"
  "......oooooo############oooooo......"
  ".....o#o###o############o###o#o....."
  "....o#o#####o##########o#####o#o...."
  "....o#o#####o##########o#####o#o...."
  "....o#o######o########o######o#o...."
  "....o#o######o########o######o#o...."
  "...o#ooo####################ooo#o..."
  "..o#o###oo################oo###o#o.."
  "..o#######o##############o#######o.."
  ".o################################o."
  ".o################################o."
  ".o################################o."
  "..o########oo##########oo########o.."
  "..o#o###ooo##############ooo###o#o.."
  "...o#ooo####################ooo#o..."
  "....o##oo####o########o####oo##o...."
  ".....oo##oooo##########oooo##oo....."
  ".......oo##################oo......."
  ".........oooooooooooooooooo.........";

// Lower shell
#define BASE_W 36
#define BASE_H 13
static const char BASE[] PROGMEM =
  ".....oooo##################oooo....."
  "..ooo##########################ooo.."
  ".o################################o."
  "ooo##############################ooo"
  "o##oo##########################oo##o"
  "o#o##oo###ooooo######ooooo###oo##o#o"
  ".oo###oooo#####oooooo#####oooo###oo."
  "..o###o##o################o##o###o.."
  "...oooo##o################o##oooo..."
  ".....ooo##o###o######o###o##ooo....."
  ".......oooo###o######o###oooo......."
  "..........oooooo####oooooo.........."
  "...............oooooo...............";

// first row of the front lip in each BASE column (pearl hides behind it)
static const uint8_t BASE_RIM[BASE_W] PROGMEM = { 13, 4, 4, 5, 5, 6, 6, 7, 7, 7, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 7, 7, 7, 6, 6, 5, 5, 4, 4, 13 };

#define PEARL_CX 17.5f
#define PEARL_CY 21.5f  // in LID/shell coordinates
// ===================== MERMAID TAIL SCENE =====================
// Tail swishes from the base (tip moves most), every few seconds
// it does a big flick that throws water droplets. Sparkles twinkle,
// bubbles rise and the water ripples.

namespace mermaid {

const int X0 = (SCREEN_W - TAIL_W) / 2;
const int Y0 = SCREEN_H - (TAIL_H + WAVES_H) - 1;
const int SEA_Y = Y0 + TAIL_H + 1;  // water surface line

float flick = 0;  // 0..1, big-flick strength (decays)
unsigned long nextFlick = 0;

struct Drop {
  float x, y, vx, vy;
  bool alive;
};
struct Bubble {
  float x, y, speed, wob;
  bool alive;
};
struct Twinkle {
  int x, y;
  unsigned long born;
  unsigned life;
  bool alive;
};
Drop drops[10];
Bubble bubbles[4];
Twinkle twinkles[4];

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < display.width() && y < display.height()) display.drawPixel(x, y, c);
}

// 4-point star. size 0 = dot, 1 = small plus, 2 = big plus
void star(int x, int y, int size, uint16_t c) {
  px(x, y, c);
  for (int i = 1; i <= size; i++) {
    px(x + i, y, c);
    px(x - i, y, c);
    px(x, y + i, c);
    px(x, y - i, c);
  }
}

void spawnDrops(int x, int y, int dir) {
  for (auto &d : drops) {
    if (d.alive) continue;
    d.alive = true;
    d.x = x;
    d.y = y;
    d.vx = dir * (0.4f + random(0, 100) / 100.0f);
    d.vy = -(0.6f + random(0, 100) / 80.0f);
    if (random(3) == 0) break;  // random count
  }
}

void reset() {
  flick = 0;
  nextFlick = millis() + 3000;
  for (auto &d : drops) d.alive = false;
  for (auto &b : bubbles) b.alive = false;
  for (auto &t : twinkles) t.alive = false;
}

void action() {
  flick = 1.0f;
}  // button: big flick now

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;

  // ---- big flick timing ----
  if (now > nextFlick) {
    flick = 1.0f;
    nextFlick = now + random(6000, 11000);
  }
  float amp = 2.2f + flick * 6.0f;
  float ph = t * (flick > 0.1f ? 7.5f : 3.6f);
  float prevTipDx = 0;

  // ---- sea surface across the whole screen ----
  for (int x = 0; x < display.width(); x++) {
    int y = SEA_Y + (int)roundf(sinf(x * 0.33f + t * 3.0f) * 1.2f);
    if ((x + (int)(t * 8)) % 5 != 0) px(x, y, SSD1306_WHITE);
  }

  // ---- tail: every row shifted sideways, more near the top ----
  for (int r = 0; r < TAIL_H; r++) {
    float h = (float)(TAIL_H - 1 - r) / (TAIL_H - 1);  // 0 bottom .. 1 top
    int dx = (int)roundf(amp * h * h * sinf(ph - 1.7f * h));
    if (r == 0) prevTipDx = dx;
    for (int c = 0; c < TAIL_W; c++) {
      char ch = pgm_read_byte(&TAIL[r * TAIL_W + c]);
      if (ch == '#') px(X0 + c + dx, Y0 + r, SSD1306_WHITE);
      else if (ch == 'o') px(X0 + c + dx, Y0 + r, SSD1306_BLACK);
    }
  }

  // droplets fly off the fin tips during a flick
  if (flick > 0.5f && random(4) == 0) {
    int dir = sinf(ph) > 0 ? 1 : -1;
    spawnDrops(X0 + 31 + (int)prevTipDx, Y0 + 1, dir);               // top fin tip
    spawnDrops(X0 + 1 + (int)(amp * 0.7f * sinf(ph)), Y0 + 13, -1);  // left fin tip
  }
  flick *= 0.985f;
  if (flick < 0.02f) flick = 0;

  // ---- water under the tail ripples ----
  for (int r = 0; r < WAVES_H; r++) {
    int dx = (int)roundf(sinf(t * 4.0f + r * 1.4f) * 1.0f);
    for (int c = 0; c < WAVES_W; c++) {
      char ch = pgm_read_byte(&WAVES[r * WAVES_W + c]);
      if (ch == '#') px(X0 + c + dx, Y0 + TAIL_H + r, SSD1306_WHITE);
      else if (ch == 'o') px(X0 + c + dx, Y0 + TAIL_H + r, SSD1306_BLACK);
    }
  }

  // ---- droplets ----
  for (auto &d : drops) {
    if (!d.alive) continue;
    d.x += d.vx;
    d.y += d.vy;
    d.vy += 0.12f;
    if (d.y > SEA_Y) {
      d.alive = false;
      continue;
    }
    px((int)d.x, (int)d.y, SSD1306_WHITE);
  }

  // ---- sparkles from the original picture ----
  for (int i = 0; i < TAIL_SPARKLE_N; i++) {
    float v = sinf(t * (2.2f + i * 0.37f) + i * 1.9f);
    if (v < -0.3f) continue;  // off
    int size = v > 0.75f ? 2 : (v > 0.2f ? 1 : 0);
    star(X0 + TAIL_SPARKLES[i][0], Y0 + TAIL_SPARKLES[i][1], size, SSD1306_WHITE);
  }

  // ---- extra twinkles on the empty sides of the screen ----
  for (auto &tw : twinkles) {
    if (!tw.alive) {
      if (random(40) == 0) {
        tw.alive = true;
        tw.born = now;
        tw.life = random(600, 1400);
        tw.x = random(2) ? random(3, X0 - 4) : random(X0 + TAIL_W + 4, SCREEN_W - 3);
        tw.y = random(3, SEA_Y - 6);
      }
      continue;
    }
    float k = (float)(now - tw.born) / tw.life;  // 0..1
    if (k >= 1) {
      tw.alive = false;
      continue;
    }
    float s = sinf(k * PI);  // grow then shrink
    star(tw.x, tw.y, s > 0.8f ? 2 : (s > 0.35f ? 1 : 0), SSD1306_WHITE);
  }

  // ---- bubbles rising from the water ----
  for (auto &b : bubbles) {
    if (!b.alive) {
      if (random(60) == 0) {
        b.alive = true;
        b.x = random(2) ? random(4, X0 - 2) : random(X0 + TAIL_W + 2, SCREEN_W - 4);
        b.y = SEA_Y - 2;
        b.speed = 0.25f + random(0, 30) / 100.0f;
        b.wob = random(0, 628) / 100.0f;
      }
      continue;
    }
    b.y -= b.speed;
    if (b.y < 2) {
      b.alive = false;
      continue;
    }
    int bx = (int)(b.x + sinf(t * 3 + b.wob) * 1.5f);
    display.drawCircle(bx, (int)b.y, 1, SSD1306_WHITE);
  }
}

}  // namespace mermaid
// ======================= SHELL + PEARL SCENE =======================
// The clam opens and closes. When it opens the pearl rises out,
// a burst of light rays pops out, and every couple of seconds a
// glint sweeps across the pearl with a twinkling star.

namespace shell {

const int X0 = (SCREEN_W - LID_W) / 2;
const int Y0 = (SCREEN_H - (LID_H + BASE_H)) / 2 + 1;

enum State { OPEN,
             CLOSING,
             CLOSED,
             OPENING };
State state = OPEN;
unsigned long stateStart = 0, stateLen = 0;
float closed = 0;            // 0 = fully open, 1 = shut (can go slightly <0 on overshoot)
unsigned long openedAt = 0;  // when it last finished opening (ray burst)
unsigned long nextGlint = 0;

struct Bubble {
  float x, y, speed, wob;
  bool alive;
};
Bubble bubbles[3];

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < display.width() && y < display.height()) display.drawPixel(x, y, c);
}

void go(int s, unsigned long len) {
  state = (State)s;
  stateStart = millis();
  stateLen = len;
}

void reset() {
  closed = 1;
  go(OPENING, 900);
  nextGlint = millis() + 1500;
  for (auto &b : bubbles) b.alive = false;
}

void action() {  // button: open / close now
  if (state == OPEN) go(CLOSING, 450);
  if (state == CLOSED) go(OPENING, 900);
}

float easeInOut(float k) {
  return k < 0.5f ? 2 * k * k : 1 - powf(-2 * k + 2, 2) / 2;
}
float easeOutBack(float k) {
  const float c1 = 1.9f, c3 = c1 + 1;
  return 1 + c3 * powf(k - 1, 3) + c1 * powf(k - 1, 2);
}

void drawBaseRows(int xo, bool frontOnly) {
  for (int r = 0; r < BASE_H; r++)
    for (int c = 0; c < BASE_W; c++) {
      if (frontOnly && r < pgm_read_byte(&BASE_RIM[c])) continue;
      char ch = pgm_read_byte(&BASE[r * BASE_W + c]);
      if (ch == '#') px(X0 + xo + c, Y0 + LID_H + r, SSD1306_WHITE);
      else if (ch == 'o') px(X0 + xo + c, Y0 + LID_H + r, SSD1306_BLACK);
    }
}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;
  float k = stateLen ? min(1.0f, (float)(now - stateStart) / stateLen) : 1;

  // ---------- state machine ----------
  switch (state) {
    case OPEN:
      closed = 0;
      if (k >= 1) go(CLOSING, 450);
      break;
    case CLOSING:
      closed = easeInOut(k);
      if (k >= 1) go(CLOSED, random(1200, 2200));
      break;
    case CLOSED:
      closed = 1;
      if (k >= 1) go(OPENING, 900);
      break;
    case OPENING:
      closed = 1 - easeOutBack(k);  // overshoots a little = bouncy
      if (k >= 1) {
        closed = 0;
        openedAt = now;
        nextGlint = now + 700;
        go(OPEN, random(4000, 6500));
      }
      break;
  }

  // shiver just before it opens
  int xo = 0;
  if (state == CLOSED && k > 0.7f) xo = ((now / 60) % 2) ? 1 : -1;

  // ---------- lower shell ----------
  drawBaseRows(xo, false);

  // ---------- lid: squashed down toward the lower shell ----------
  float topD = 0 + 17.0f * closed;
  float botD = (LID_H - 1) + 6.0f * closed;
  int yA = (int)floorf(topD), yB = (int)ceilf(botD);
  for (int yd = yA; yd <= yB; yd++) {
    int sr = (int)roundf((yd - topD) / (botD - topD) * (LID_H - 1));
    if (sr < 0 || sr >= LID_H) continue;
    for (int c = 0; c < LID_W; c++) {
      char ch = pgm_read_byte(&LID[sr * LID_W + c]);
      if (ch == '#') px(X0 + xo + c, Y0 + yd, SSD1306_WHITE);
      else if (ch == 'o') px(X0 + xo + c, Y0 + yd, SSD1306_BLACK);
    }
  }

  // ---------- pearl (sinks into the shell while it closes) ----------
  float sink = min(1.0f, max(0.0f, closed / 0.6f));
  float pcy = PEARL_CY + sink * sink * 12.0f;
  bool glinting = false;
  float g = 0;
  if (state == OPEN && now > nextGlint) {
    g = (float)(now - nextGlint) / 450.0f;  // glint progress 0..1
    if (g >= 1) nextGlint = now + random(1600, 2800);
    else glinting = true;
  }
  if (sink < 1) {
    for (int r = (int)(pcy - 6); r <= (int)(pcy + 6); r++)
      for (int c = 12; c <= 23; c++) {
        float dx = c - PEARL_CX, dy = r - pcy;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 5.0f) continue;
        int baseRow = r - LID_H;  // hide behind front lip
        if (baseRow >= (int)pgm_read_byte(&BASE_RIM[c])) continue;
        uint16_t col = (d <= 4.0f) ? SSD1306_WHITE : SSD1306_BLACK;
        // soft shadow on the lower-right, dithered
        if (d <= 4.0f && dx + dy > 3.2f && ((r + c) & 1)) col = SSD1306_BLACK;
        // moving glint: a dark diagonal streak sweeping across
        if (glinting && d <= 4.0f) {
          float s = -7 + g * 14;  // streak position
          if (fabsf((dx - dy) * 0.7071f - s) < 0.8f) col = SSD1306_BLACK;
        }
        px(X0 + xo + c, Y0 + r, col);
      }
  }

  // ---------- front lip goes over the pearl ----------
  drawBaseRows(xo, true);

  // ---------- shine effects (XOR so they show on black and on white) ----------
  if (state == OPEN) {
    int cx = X0 + (int)PEARL_CX, cy = Y0 + (int)PEARL_CY;

    // burst of rays right after opening
    unsigned long since = now - openedAt;
    if (since < 700) {
      float p = since / 700.0f;
      int r0 = 6 + (int)(p * 8), r1 = r0 + 3;
      for (int a = 0; a < 8; a++) {
        float ang = a * PI / 4 + PI / 8;
        for (int rr = r0; rr <= r1; rr++)
          px(cx + (int)roundf(cosf(ang) * rr), cy + (int)roundf(sinf(ang) * rr * 0.8f), SSD1306_INVERSE);
      }
    }

    // twinkle star on the pearl's top-right with each glint
    if (glinting || since < 700) {
      float s = glinting ? sinf(g * PI) : sinf((since / 700.0f) * PI);
      int len = (int)roundf(s * 4);
      int sx = cx + 3, sy = cy - 3;
      px(sx, sy, SSD1306_INVERSE);
      for (int i = 1; i <= len; i++) {
        px(sx + i, sy, SSD1306_INVERSE);
        px(sx - i, sy, SSD1306_INVERSE);
        px(sx, sy + i, SSD1306_INVERSE);
        px(sx, sy - i, SSD1306_INVERSE);
      }
    }
  }

  // ---------- a few bubbles drifting up beside the shell ----------
  for (auto &b : bubbles) {
    if (!b.alive) {
      if (random(70) == 0) {
        b.alive = true;
        b.x = random(2) ? random(6, X0 - 6) : random(X0 + LID_W + 6, SCREEN_W - 6);
        b.y = SCREEN_H - 2;
        b.speed = 0.25f + random(0, 30) / 100.0f;
        b.wob = random(0, 628) / 100.0f;
      }
      continue;
    }
    b.y -= b.speed;
    if (b.y < 2) {
      b.alive = false;
      continue;
    }
    display.drawCircle((int)(b.x + sinf(t * 3 + b.wob) * 1.5f), (int)b.y, 1, SSD1306_WHITE);
  }
}

}  // namespace shell
// ========================== BEACH SCENE ==========================
// Waves roll up a sandy shore and slide back out, the foam edge
// wobbles, a starfish sits on the sand and gets washed over now and
// then, and a seagull glides past over the water.
// Everything here is drawn with maths - no stored picture.

namespace beach {

// The shoreline runs diagonally: sea is where f(x,y) is small.
inline float fline(int x, int y) {
  return x * 0.55f + y;
}

float birdX = -20, birdY = 12;
unsigned long nextBird = 0;

const float STAR_X = 74, STAR_Y = 23, STAR_R = 8.5f;  // near the water line
float starVx[10], starVy[10], starIx[10], starIy[10], starHx[10], starHy[10];

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < display.width() && y < display.height()) display.drawPixel(x, y, c);
}

// cheap repeatable "random" for sand grains
inline uint8_t hash2(int x, int y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (h >> 16) & 0xFF;
}

// is the point inside the 10-point star outline?
bool inStar(float x, float y, const float *vx, const float *vy) {
  bool in = false;
  for (int i = 0, j = 9; i < 10; j = i++)
    if (((vy[i] > y) != (vy[j] > y)) && (x < (vx[j] - vx[i]) * (y - vy[i]) / (vy[j] - vy[i]) + vx[i])) in = !in;
  return in;
}

void buildStar(float scale, float *vx, float *vy) {
  for (int i = 0; i < 10; i++) {
    float ang = -PI / 2 + i * PI / 5;
    float rr = (i & 1) ? STAR_R * 0.42f * scale : STAR_R * scale;
    vx[i] = STAR_X + cosf(ang) * rr;
    vy[i] = STAR_Y + sinf(ang) * rr * 0.92f;
  }
}

void reset() {
  buildStar(1.00f, starVx, starVy);  // the starfish itself
  buildStar(0.78f, starIx, starIy);
  buildStar(1.32f, starHx, starHy);  // clear halo around it
  birdX = -20;
  nextBird = millis() + 4000;
}

void action() {
  nextBird = millis();
}  // button: send the seagull across

void frame(unsigned long now) {
  float t = now / 1000.0f;

  // ---- how far the wave has run up the beach ----
  float p = fmodf(t / 4.2f, 1.0f);
  float surge = (p < 0.28f) ? (1 - powf(1 - p / 0.28f, 3))            // rushes in
                            : (1 - powf((p - 0.28f) / 0.72f, 1.4f));  // slides back out
  float shore = 40 + 26 * surge;

  // Speed: work out per-column and per-distance values ONCE instead of
  // calling sin() for all 8192 pixels, and write straight into the screen
  // buffer instead of going through drawPixel().
  static float colShore[SCREEN_W];
  static uint8_t swell[200];
  for (int x = 0; x < SCREEN_W; x++) colShore[x] = shore + 2.2f * sinf(x * 0.16f + t * 0.9f);
  for (int i = 0; i < 200; i++) swell[i] = sinf(i * 0.30f - t * 2.2f) > 0.93f;

  uint8_t *buf = display.getBuffer();
  memset(buf, 0, SCREEN_W * SCREEN_H / 8);

  for (int y = 0; y < SCREEN_H; y++) {
    uint8_t *row = buf + (y >> 3) * SCREEN_W;
    uint8_t bit = 1 << (y & 7);
    for (int x = 0; x < SCREEN_W; x++) {
      float f = x * 0.55f + y;
      float s = colShore[x];
      uint8_t h = hash2(x, y);
      bool lit;
      if (f > s + 3) lit = (h < 7);        // dry sand: sparse grains
      else if (f > s) lit = (h < 40);      // wet sand behind the foam
      else if (f > s - 4) lit = true;      // foam edge
      else if (f > s - 8) lit = (h < 70);  // fizz behind the foam
      else lit = swell[(int)f];            // open water: lines of swell
      if (lit) {
#if ROTATION == 0
        row[x] |= bit;  // fast path
#else
        display.drawPixel(x, y, SSD1306_WHITE);
#endif
      }
    }
  }

  // ---- starfish ----
  float sAtStar = shore + 2.2f * sinf(STAR_X * 0.16f + t * 0.9f);
  bool washed = fline((int)STAR_X, (int)STAR_Y) < sAtStar;  // wave is over it
  // clear a little space so the sand grains don't eat into it
  for (int y = (int)(STAR_Y - STAR_R - 2); y <= (int)(STAR_Y + STAR_R + 2); y++)
    for (int x = (int)(STAR_X - STAR_R - 2); x <= (int)(STAR_X + STAR_R + 2); x++)
      if (inStar(x, y, starHx, starHy)) px(x, y, SSD1306_BLACK);

  for (int y = (int)(STAR_Y - STAR_R - 1); y <= (int)(STAR_Y + STAR_R + 1); y++)
    for (int x = (int)(STAR_X - STAR_R - 1); x <= (int)(STAR_X + STAR_R + 1); x++) {
      if (!inStar(x, y, starVx, starVy)) continue;
      uint16_t c = SSD1306_WHITE;
      if (washed && ((x + y) & 1)) c = SSD1306_BLACK;  // shimmers under water
      px(x, y, c);
    }
  // one dark speck in the middle so it reads as a starfish, not a blob
  px((int)STAR_X, (int)STAR_Y, SSD1306_BLACK);
  px((int)STAR_X - 2, (int)STAR_Y + 1, SSD1306_BLACK);
  px((int)STAR_X + 2, (int)STAR_Y + 1, SSD1306_BLACK);

  // ---- a couple of shells on the sand ----
  px(30, 60, SSD1306_WHITE);
  px(31, 60, SSD1306_WHITE);
  px(30, 61, SSD1306_WHITE);
  px(112, 58, SSD1306_WHITE);
  px(113, 59, SSD1306_WHITE);

  // ---- seagull gliding over the water ----
  if (now > nextBird) {
    birdX += 0.75f;
    birdY = 10 + sinf(t * 1.3f) * 2.5f;
    int bx = (int)birdX, by = (int)birdY;
    int flap = (sinf(t * 6.0f) > 0) ? 1 : -1;  // wings up / down
    for (int i = 1; i <= 3; i++) {
      px(bx - i, by - flap * i + (flap < 0 ? 2 : 0), SSD1306_WHITE);
      px(bx + i, by - flap * i + (flap < 0 ? 2 : 0), SSD1306_WHITE);
    }
    px(bx, by + (flap < 0 ? 1 : 0), SSD1306_WHITE);
    if (birdX > SCREEN_W + 20) {
      birdX = -20;
      nextBird = now + random(5000, 12000);
    }
  }
}

}  // namespace beach
// ======================= STARRY NIGHT SCENE =======================
// My own take on Van Gogh's "The Starry Night" (painted 1889, public
// domain), redrawn for a one-colour screen: the sky is made of flowing
// brush-stroke lines that swirl and drift, stars pulse inside their
// halos, a cypress sways on the left and the village sleeps below.

namespace starry {

struct Star {
  int8_t x, y, r;
  float phase;
};
const Star STARS[] = {
  { 26, 10, 3, 0.0f },
  { 52, 20, 2, 1.7f },
  { 70, 8, 3, 3.1f },
  { 92, 26, 2, 4.4f },
  { 112, 14, 2, 2.2f },
  { 40, 33, 2, 5.5f },
};
const int STAR_N = sizeof(STARS) / sizeof(STARS[0]);

float t0 = 0;

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < display.width() && y < display.height()) display.drawPixel(x, y, c);
}

void reset() {
  t0 = millis() / 1000.0f;
}
void action() {
  t0 -= 1.5f;
}  // button: hurry the sky along

// one curling brush stroke
void stroke(float cx, float cy, float r, float spin, float len, float squash) {
  for (float a = 0; a < len; a += 0.22f) {
    float rr = r + a * 1.15f;
    int x = (int)(cx + cosf(a + spin) * rr);
    int y = (int)(cy + sinf(a + spin) * rr * squash);
    px(x, y, SSD1306_WHITE);
  }
}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f - t0;

  // ---- flowing bands across the sky ----
  for (int b = 0; b < 4; b++) {
    float base = 8 + b * 13;
    for (int x = 0; x < SCREEN_W; x += 2) {
      float y = base + sinf(x * 0.09f + t * 0.5f + b * 1.3f) * 3.2f
                + sinf(x * 0.031f - t * 0.25f) * 2.0f;
      if (((x >> 1) + b + (int)(t * 4)) % 3) px(x, (int)y, SSD1306_WHITE);
    }
  }

  // ---- the two big swirls ----
  stroke(52, 26, 2.0f, t * 0.9f, 7.0f, 0.75f);
  stroke(52, 26, 5.5f, t * 0.9f + 2.2f, 5.2f, 0.75f);
  stroke(88, 36, 1.5f, -t * 1.1f, 5.6f, 0.8f);

  // ---- stars, pulsing inside their halos ----
  for (int i = 0; i < STAR_N; i++) {
    const Star &s = STARS[i];
    float k = 0.5f + 0.5f * sinf(t * 1.6f + s.phase);
    display.fillCircle(s.x, s.y, s.r, SSD1306_WHITE);
    int halo = s.r + 2 + (int)(k * 2);
    for (int a = 0; a < 12; a++) {  // dashed halo ring
      if ((a + (int)(t * 3) + i) % 2) continue;
      float ang = a * PI / 6;
      px(s.x + (int)(cosf(ang) * halo), s.y + (int)(sinf(ang) * halo), SSD1306_WHITE);
    }
  }

  // ---- crescent moon, top right ----
  display.fillCircle(115, 12, 7, SSD1306_WHITE);
  display.fillCircle(111, 9, 7, SSD1306_BLACK);

  // ---- cypress tree, swaying ----
  for (int y = 6; y < 58; y++) {
    float k = (58 - y) / 52.0f;  // 0 at the base, 1 at the tip
    float sway = sinf(t * 1.1f + k * 1.6f) * 2.6f * k * k;
    int w = (int)(1 + 8 * (1 - k) * (1 - k * 0.15f));  // flame shape
    int cx = (int)(14 + sway);
    for (int x = cx - w; x <= cx + w; x++) {
      bool gap = ((x * 3 + y * 5 + (int)(t * 2)) % 9) == 0;  // brushy texture
      px(x, y, gap ? SSD1306_BLACK : SSD1306_WHITE);
    }
  }

  // ---- village below ----
  // each house: left edge, width, height, roof peak
  const uint8_t HOUSE[][3] = { { 0, 13, 9 }, { 13, 10, 12 }, { 23, 8, 8 }, { 31, 14, 11 }, { 45, 9, 14 }, { 54, 12, 9 }, { 66, 10, 13 }, { 76, 13, 9 }, { 89, 9, 12 }, { 98, 14, 10 }, { 112, 16, 8 } };
  const int HOUSE_N = sizeof(HOUSE) / sizeof(HOUSE[0]);
  for (int i = 0; i < HOUSE_N; i++) {
    int hx = HOUSE[i][0], hw = HOUSE[i][1], hh = HOUSE[i][2];
    int top = SCREEN_H - hh;
    for (int y = top; y < SCREEN_H; y++)
      for (int x = hx; x < hx + hw; x++) px(x, y, SSD1306_WHITE);
    for (int r = 1; r <= hw / 2; r++)  // pitched roof
      for (int x = hx + r; x < hx + hw - r; x++) px(x, top - r, SSD1306_WHITE);
    // a window, dark, lighting up now and then
    int wx = hx + hw / 2, wy = top + 3;
    if (((int)(t * 0.6f) + i * 3) % 5) {
      px(wx, wy, SSD1306_BLACK);
      px(wx + 1, wy, SSD1306_BLACK);
      px(wx, wy + 1, SSD1306_BLACK);
      px(wx + 1, wy + 1, SSD1306_BLACK);
    }
  }
  // church spire
  for (int y = SCREEN_H - 26; y < SCREEN_H; y++) px(61, y, SSD1306_WHITE), px(62, y, SSD1306_WHITE),
                                                 px(63, y, SSD1306_WHITE);
  for (int r = 0; r < 4; r++) px(62, SCREEN_H - 27 - r, SSD1306_WHITE);
}

}  // namespace starry
// ========================== ROBOT EYES SCENE ==========================
// Two glowing rounded eyes that glance around, blink and change mood:
// happy, angry, sad, surprised, sleepy, love, wink, suspicious and
// curious. Every mood is the same two rounded rectangles, reshaped,
// with black shapes cut out of them (an arch from below for happy, a
// slanted brow from above for angry and sad). Sizes and positions
// ease toward their targets every frame, so every change is smooth.

namespace eyes {

enum Mood { NORMAL,
            HAPPY,
            ANGRY,
            SAD,
            SURPRISED,
            SLEEPY,
            LOVE,
            WINK,
            SQUINT,
            CURIOUS };

const int EW = 34, EH = 38, GAP = 14;  // eye size and the space between them

int mood = NORMAL, wantMood = NORMAL;
float amt = 0;  // how far into the current mood's shape, 0..1
float lookX = 0, lookY = 0, tLookX = 0, tLookY = 0;
float w[2] = { EW, EW }, h[2] = { EH, EH };
unsigned long moodUntil = 0, nextLook = 0, nextBlink = 0, blinkStart = 0;
bool blinking = false, secondBlink = false;

void setMood(int m, unsigned long len) {
  wantMood = m;
  moodUntil = millis() + len;
}

void reset() {
  unsigned long now = millis();
  mood = wantMood = NORMAL;
  amt = 0;
  lookX = lookY = tLookX = tLookY = 0;
  w[0] = w[1] = EW;
  h[0] = h[1] = 2;  // eyes open up from closed
  moodUntil = now + 2500;
  nextLook = now + 700;
  nextBlink = now + 2000;
  blinking = false;
}

void action() {
  setMood(SURPRISED, 1200);
}

// a heart made of two circles and a triangle
void heart(int cx, int cy, float s) {
  int r = (int)(s * 0.5f);
  if (r < 1) return;
  display.fillCircle(cx - r + 1, cy - r / 2, r, SSD1306_WHITE);
  display.fillCircle(cx + r - 1, cy - r / 2, r, SSD1306_WHITE);
  display.fillTriangle(cx - 2 * r + 1, cy - r / 4 + 1, cx + 2 * r - 1, cy - r / 4 + 1, cx, cy + 2 * r, SSD1306_WHITE);
}

// a little "z" for sleepy
void zee(int x, int y, int s) {
  display.drawLine(x, y, x + s, y, SSD1306_WHITE);
  display.drawLine(x + s, y, x, y + s, SSD1306_WHITE);
  display.drawLine(x, y + s, x + s, y + s, SSD1306_WHITE);
}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;

  // ---- pick the next mood ----
  if (now > moodUntil) {
    if (wantMood == SLEEPY) setMood(SURPRISED, 1300);  // wakes up with a start
    else if (wantMood != NORMAL) setMood(NORMAL, random(2500, 4500));
    else {
      static const uint8_t PICK[] = { HAPPY, HAPPY, ANGRY, SAD, SURPRISED, SLEEPY, LOVE, WINK, SQUINT, CURIOUS, CURIOUS };
      int m = PICK[random(sizeof(PICK))];
      setMood(m, m == SLEEPY ? 5000 : m == LOVE ? 3500
                                                : random(2500, 4000));
    }
  }
  // old mood's shape fades out, then the new one fades in
  if (wantMood != mood) {
    amt -= 0.14f;
    if (amt <= 0) {
      amt = 0;
      if (mood == LOVE) h[0] = h[1] = 2;  // hearts gone: eyes open up again
      mood = wantMood;
    }
  } else if (amt < 1) amt = min(1.0f, amt + 0.1f);

  // ---- where to look ----
  if (now > nextLook) {
    nextLook = now + random(700, 2600);
    if (random(3) == 0) {
      tLookX = 0;
      tLookY = 0;
    } else {
      tLookX = random(-20, 21);
      tLookY = random(-9, 10);
    }
  }
  float lx = tLookX, ly = tLookY;
  if (mood == SLEEPY) {
    lx = 0;
    ly = 8;
  }  // head drooping
  if (mood == SQUINT) {
    lx = sinf(t * 1.6f) * 18;
    ly = 0;
  }  // suspicious side-eye
  if (mood == LOVE || mood == SURPRISED) {
    lx *= 0.3f;
    ly *= 0.3f;
  }
  lookX += (lx - lookX) * 0.3f;
  lookY += (ly - lookY) * 0.3f;
  float hop = mood == HAPPY ? -fabsf(sinf(t * 7)) * 3 * amt : 0;  // happy bounce

  // ---- eye sizes for this mood ----
  float tw[2] = { EW, EW }, th[2] = { EH, EH };
  if (mood == ANGRY) th[0] = th[1] = EH - 6;
  if (mood == SAD) th[0] = th[1] = EH - 4;
  if (mood == SURPRISED) {
    tw[0] = tw[1] = EW + 4;
    th[0] = th[1] = EH + 8;
  }
  if (mood == SLEEPY) th[0] = th[1] = 5;
  if (mood == WINK) th[1] = 5;
  if (mood == SQUINT) th[0] = th[1] = 15;
  if (mood == CURIOUS) {  // the eye it looks toward grows
    if (lookX < -5) {
      th[0] = EH + 6;
      th[1] = EH - 6;
    }
    if (lookX > 5) {
      th[0] = EH - 6;
      th[1] = EH + 6;
    }
  }
  for (int i = 0; i < 2; i++) {
    w[i] += (tw[i] - w[i]) * 0.25f;
    h[i] += (th[i] - h[i]) * 0.25f;
  }

  // ---- blinking (sometimes twice) ----
  if (!blinking && now > nextBlink && mood != SLEEPY) {
    blinking = true;
    blinkStart = now;
  }
  float blink = 1;
  if (blinking) {
    float k = (now - blinkStart) / 170.0f;
    if (k >= 1) {
      blinking = false;
      if (!secondBlink && random(4) == 0) {
        secondBlink = true;
        nextBlink = now + 120;
      } else {
        secondBlink = false;
        nextBlink = now + random(2000, 5000);
      }
    } else blink = 1 - 0.9f * sinf(k * PI);
  }

  // ---- draw both eyes ----
  for (int i = 0; i < 2; i++) {
    int side = i ? 1 : -1;
    float cx = 64 + side * (EW / 2 + GAP / 2) + lookX;
    float cy = 32 + lookY + hop;

    if (mood == LOVE) {
      float beat = powf(max(0.0f, sinf(t * 6)), 4);
      heart((int)cx, (int)cy, 17 * amt * (1 + 0.12f * beat));
      continue;
    }

    int W = (int)roundf(w[i]), H = (int)roundf(max(2.0f, h[i] * blink));
    int x = (int)roundf(cx - W / 2.0f), y = (int)roundf(cy - H / 2.0f);
    int r = min(9, min(W, H) / 2);
    if (mood == SURPRISED) r = min(W, H) * 2 / 5;  // rounder
    display.fillRoundRect(x, y, W, H, r, SSD1306_WHITE);

    int inner = i == 0 ? x + W + 1 : x - 2;  // side facing the other eye
    int outer = i == 0 ? x - 2 : x + W + 1;
    if (mood == HAPPY || (mood == WINK && i == 0)) {  // ^ ^ : arch cut from below
      int R = (int)(W * 0.62f);
      display.fillCircle((int)cx, y + H + R - (int)(H * 0.45f * amt), R, SSD1306_BLACK);
    }
    if (mood == ANGRY)  // brows slant down to the middle
      display.fillTriangle(outer, y - 1, inner, y - 1, inner, y + (int)(H * 0.55f * amt), SSD1306_BLACK);
    if (mood == SAD)  // brows droop to the outside
      display.fillTriangle(outer, y - 1, inner, y - 1, outer, y + (int)(H * 0.5f * amt), SSD1306_BLACK);
  }

  // ---- sleepy: z's float up from the right eye ----
  if (mood == SLEEPY && amt > 0.8f)
    for (int j = 0; j < 3; j++) {
      float p = fmodf(t * 0.5f + j / 3.0f, 1.0f);
      zee(96 + (int)(p * 16), 30 - (int)(p * 26), 2 + (int)(p * 4));
    }
}

}  // namespace eyes
// ===================== SHARED DRAWING HELPERS =====================
// Shapes the Adafruit library doesn't have: ellipses, any-shape polygons
// and thick lines. pattern: 0 = solid, 1 = checkerboard (half lit),
// 2 = sparse dots (a quarter lit).

namespace art {

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H) display.drawPixel(x, y, c);
}

inline bool skip(int x, int y, int pattern) {
  if (pattern == 1) return (x + y) & 1;
  if (pattern == 2) return (x & 1) || (y & 1);
  return false;
}

void ellipse(float cx, float cy, float rx, float ry, uint16_t c, int pattern) {
  for (int y = (int)ceilf(cy - ry); y <= (int)floorf(cy + ry); y++) {
    float dy = (y - cy) / ry;
    float hw = rx * sqrtf(max(0.0f, 1 - dy * dy));
    for (int x = (int)ceilf(cx - hw); x <= (int)floorf(cx + hw); x++)
      if (!skip(x, y, pattern)) px(x, y, c);
  }
}

// filled polygon, one row at a time (fast enough for big wings at 45 fps)
void poly(const float *xs, const float *ys, int n, uint16_t c, int pattern) {
  float y0 = ys[0], y1 = ys[0];
  for (int i = 1; i < n; i++) { y0 = min(y0, ys[i]); y1 = max(y1, ys[i]); }
  for (int y = (int)ceilf(y0); y <= (int)floorf(y1); y++) {
    float xi[8]; int k = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
      if (((ys[i] > y) != (ys[j] > y)) && k < 8) xi[k++] = xs[i] + (y - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
    for (int a = 1; a < k; a++) for (int b = a; b > 0 && xi[b] < xi[b - 1]; b--) { float s = xi[b]; xi[b] = xi[b - 1]; xi[b - 1] = s; }
    for (int m = 0; m + 1 < k; m += 2)
      for (int x = (int)ceilf(xi[m]); x <= (int)floorf(xi[m + 1]); x++)
        if (!skip(x, y, pattern)) px(x, y, c);
  }
}

// line drawn with round dots of radius r
void thick(float x0, float y0, float x1, float y1, float r, uint16_t c) {
  int steps = max(1, (int)max(fabsf(x1 - x0), fabsf(y1 - y0)));
  for (int i = 0; i <= steps; i++) {
    float k = (float)i / steps;
    display.fillCircle((int)roundf(x0 + (x1 - x0) * k), (int)roundf(y0 + (y1 - y0) * k), (int)r, c);
  }
}

} // namespace art
// ========================== TEA CUP SCENE ==========================
// My own 1-bit teacup: a cup with little hearts on a saucer, tea bag tag
// swinging on its string, and wisps of steam curling up and fading.

namespace tea {

const int CX = 60, RIM_Y = 25, BOT_Y = 50;

static const char HEART3[] PROGMEM = "#.####.#.";          // 3x3 heart
const int8_t HEARTS[][2] = { {-13, 32}, {-4, 30}, {7, 33}, {-9, 40}, {2, 39}, {13, 41}, {-3, 46} };

float cupHW(int y) {                                      // cup half-width at row y
  float k = (float)(y - RIM_Y) / (BOT_Y - RIM_Y);
  return 12 + 13 * cosf(k * PI / 2);
}

void reset() {}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;

  // ---- steam: three wisps, puffs travelling up, thinning as they rise ----
  for (int w = 0; w < 3; w++) {
    float bx = CX - 9 + w * 9, ph = w * 2.1f;
    for (int y = RIM_Y - 4; y >= 0; y--) {
      float rise = RIM_Y - 4 - y;
      if (fmodf(y + t * 12 + ph * 5, 16) > 11) continue;          // gaps move upward
      if (rise > 11 && beach::hash2(w * 64 + y, (int)(t * 8)) < (rise - 11) * 22) continue;   // fades near the top
      int x = (int)roundf(bx + sinf(y * 0.28f - t * 2.4f + ph) * (1.5f + rise * 0.22f));
      art::px(x, y, SSD1306_WHITE);
      art::px(x + 1, y, SSD1306_WHITE);
      if (rise > 3 && rise < 12) art::px(x - 1, y, SSD1306_WHITE);   // puffs widen as they rise
    }
  }

  // ---- saucer ----
  art::ellipse(CX + 3, 55, 54, 8.5f, SSD1306_WHITE, 0);
  art::ellipse(CX + 3, 54, 46, 5.5f, SSD1306_BLACK, 0);
  art::ellipse(CX + 3, 54, 44, 4.8f, SSD1306_WHITE, 2);

  // ---- handle ----
  display.fillCircle(CX - 25, 36, 9, SSD1306_BLACK);
  display.fillCircle(CX - 25, 36, 8, SSD1306_WHITE);
  display.fillCircle(CX - 25, 36, 5, SSD1306_BLACK);

  // ---- cup: black outline, white body, shaded right side, foot ----
  for (int y = RIM_Y; y <= BOT_Y + 2; y++) {
    float hw = y <= BOT_Y ? cupHW(y) : 10;
    display.drawFastHLine(CX - (int)hw - 1, y, (int)(2 * hw) + 3, SSD1306_BLACK);
    display.drawFastHLine(CX - (int)hw, y, (int)(2 * hw) + 1, SSD1306_WHITE);
    for (int x = CX + (int)hw - 4; x <= CX + (int)hw; x++)        // shadow
      if ((x + y) & 1) art::px(x, y, SSD1306_BLACK);
  }
  for (auto &h : HEARTS)
    for (int i = 0; i < 9; i++)
      if (pgm_read_byte(&HEART3[i]) == '#') art::px(CX + h[0] + i % 3, h[1] + i / 3, SSD1306_BLACK);

  // ---- rim and the tea inside ----
  art::ellipse(CX, RIM_Y, 26, 5.5f, SSD1306_WHITE, 0);
  art::ellipse(CX, RIM_Y, 23, 3.6f, SSD1306_BLACK, 0);
  art::ellipse(CX - 3, RIM_Y - 1, 6, 1.6f, SSD1306_WHITE, 1);     // tea bag corner poking out
  int glint = CX + (int)(sinf(t * 1.1f) * 13);                    // light on the tea
  art::px(glint, RIM_Y + 1, SSD1306_WHITE); art::px(glint + 1, RIM_Y + 1, SSD1306_WHITE);

  // ---- tea bag: string from the rim, tag swinging over the saucer ----
  float ang = sinf(t * 1.8f) * 0.13f;
  int ax = CX + 20, ay = RIM_Y + 1;
  int tx = ax + 4 + (int)roundf(sinf(ang) * 24), ty = ay + (int)roundf(cosf(ang) * 24);
  for (int i = 0; i <= 24; i++) {                                 // string shows on white and black
    float k = i / 24.0f;
    int sx = (int)roundf(ax + (tx - ax) * k + sinf(k * PI) * 2), sy = (int)roundf(ay + (ty - ay) * k);
    uint16_t c = display.getPixel(sx, sy) ? SSD1306_BLACK : SSD1306_WHITE;
    art::px(sx, sy, c);
  }
  display.fillRect(tx - 6, ty - 1, 13, 14, SSD1306_BLACK);
  display.fillRect(tx - 5, ty, 11, 12, SSD1306_WHITE);
  display.drawFastVLine(tx, ty + 2, 8, SSD1306_BLACK);           // little leaf on the tag
  for (int i = 1; i <= 3; i++) { art::px(tx - i, ty + 2 + i, SSD1306_BLACK); art::px(tx + i, ty + 4 + i, SSD1306_BLACK); }
}

} // namespace tea
// ========================== DRAGON SCENE ==========================
// My own 1-bit dragon on a rocky ledge: wings flap, the tail swishes,
// and every few seconds it rears back, opens its jaws and breathes fire.

namespace dragon {

struct Flame { float x, y, vx, vy; unsigned long born; bool alive; };
Flame flames[36];
unsigned long fireAt = 0;                 // when the next breath starts

void reset() {
  for (auto &f : flames) f.alive = false;
  fireAt = millis() + 2500;
}

void action() { fireAt = millis(); }

// one wing, drawn for the left side; mirror = true draws the right one
void wing(float f, bool mirror) {
  float xs[6] = { 60, 34, 4, 6, 20, 42 };
  float ys[6] = { 31, 13 - f * 7, 5 - f * 13, 30 - f * 5, 37 - f * 2, 40 };
  if (mirror) for (int i = 0; i < 6; i++) xs[i] = 138 - xs[i];
  art::poly(xs, ys, 6, SSD1306_WHITE, 1);                        // membrane
  for (int i = 3; i <= 5; i++) {                                 // scalloped trailing edge
    int j = i - 1;
    display.fillCircle((int)((xs[i] + xs[j]) / 2), (int)((ys[i] + ys[j]) / 2) + 3, 3, SSD1306_BLACK);
  }
  art::thick(xs[0], ys[0], xs[1], ys[1], 1, SSD1306_WHITE);        // arm bone
  display.drawLine(xs[1], ys[1], xs[2], ys[2], SSD1306_WHITE);   // fingers
  display.drawLine(xs[1], ys[1], xs[3], ys[3], SSD1306_WHITE);
  display.drawLine(xs[1], ys[1], xs[4], ys[4], SSD1306_WHITE);
  display.drawLine(xs[2], ys[2], xs[3], ys[3], SSD1306_WHITE);   // leading edge
}

// neck + head + body + legs + tail: pass 0 = black outline, pass 1 = white
void body(float t, float hx, float hy, float jaw, int pass) {
  uint16_t c = pass ? SSD1306_WHITE : SSD1306_BLACK;
  int o = pass ? 0 : 1;
  // tail curling off to the right
  for (int i = 0; i <= 20; i++) {
    float k = i / 20.0f;
    float x = 82 + k * 32, y = 47 + sinf(k * 3.2f) * 7 + sinf(t * 2 + k * 3) * k * 3;
    display.fillCircle((int)x, (int)y, (int)(4 - k * 3) + o, c);
  }
  art::ellipse(70, 45, 15 + o, 8 + o, c, 0);                     // body
  art::thick(60, 48, 58, 55, 2 + o, c);                          // legs
  art::thick(80, 48, 83, 55, 2 + o, c);
  // S-shaped neck up to the head
  for (int i = 0; i <= 24; i++) {
    float k = i / 24.0f, a = 1 - k;
    float x = a * a * a * 64 + 3 * a * a * k * 70 + 3 * a * k * k * 40 + k * k * k * hx;
    float y = a * a * a * 40 + 3 * a * a * k * 24 + 3 * a * k * k * 26 + k * k * k * (hy + 3);
    display.fillCircle((int)x, (int)y, (int)(4.5f - k * 2) + o, c);
  }
  // head: skull, snout, lower jaw, horns
  art::ellipse(hx, hy, 6 + o, 4 + o, c, 0);
  float sx[4] = { hx - 3, hx - 14 - o, hx - 13 - o, hx - 2 };
  float sy[4] = { hy - 3 - o, hy - 1 - o, hy + 1 + o, hy + 2 };
  art::poly(sx, sy, 4, c, 0);
  float jx[4] = { hx - 2, hx - 12 - o, hx - 11 - o, hx + 1 };
  float jy[4] = { hy + 1, hy + 2 + jaw - o, hy + 4 + jaw + o, hy + 4 + o };
  art::poly(jx, jy, 4, c, 0);
  art::thick(hx + 2, hy - 3, hx + 9, hy - 10, pass ? 1 : 2, c);
  art::thick(hx + 4, hy - 1, hx + 13, hy - 5, pass ? 1 : 2, c);
}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;

  // ---- a few stars ----
  static const int8_t STARS[][2] = { {12, 50}, {30, 44}, {100, 40}, {122, 46}, {90, 2}, {20, 2} };
  for (int i = 0; i < 6; i++)
    if (sinf(t * 1.7f + i * 2.3f) > -0.2f) art::px(STARS[i][0], STARS[i][1], SSD1306_WHITE);

  // ---- fire timing: rear back, then breathe for 1.6 s ----
  long since = (long)(now - fireAt);
  bool breathing = since >= 0 && since < 1600;
  float rear = 0;
  if (since >= -500 && since < 0) rear = 1 + since / 500.0f;       // head pulls back
  if (breathing) rear = -0.6f;                                     // and lunges forward
  if (since >= 1600) fireAt = now + random(4500, 7500);
  float hx = 46 + rear * 4, hy = 12 - rear * 2 + (breathing ? 2 : 0) + sinf(t * 1.3f) * 0.8f;   // dips its head to breathe
  float jaw = breathing ? 3 : 0;

  // ---- wings ----
  float flap = sinf(t * 2.4f) * (breathing ? 0.4f : 1.0f);
  wing(flap, false);
  wing(flap, true);

  // ---- body, with a black outline so it stands out from the wings ----
  body(t, hx, hy, jaw, 0);
  body(t, hx, hy, jaw, 1);
  for (int i = 0; i < 6; i++) {                                   // belly scales up the neck
    float k = i / 6.0f;
    display.drawFastHLine(58 + (int)(k * 2) - (int)(sinf(k * 3) * 6), 42 - i * 4, 4, SSD1306_BLACK);
  }
  art::px((int)hx - 3, (int)hy - 1, SSD1306_BLACK);                // eye
  art::px((int)hx - 2, (int)hy - 1, SSD1306_BLACK);
  if (breathing) art::px((int)hx - 3, (int)hy - 1, SSD1306_WHITE);  // eye glints while breathing

  // ---- rocky ledge ----
  for (int x = 0; x < SCREEN_W; x++) {
    int top = 55 + (int)(sinf(x * 0.21f) * 1.5f + sinf(x * 0.07f + 1) * 1.5f);
    for (int y = top; y < SCREEN_H; y++) {
      bool lit = y == top || ((x + y) & 1 && beach::hash2(x / 7 + (y / 4) * 13, y / 4) > 60);
      if (y > top && (y - top) % 4 == 0 && beach::hash2(x / 9, y) < 150) lit = false;   // cracks between stones
      art::px(x, y, lit ? SSD1306_WHITE : SSD1306_BLACK);
    }
  }

  // ---- fire: puffs shoot out of the mouth, grow, then break up ----
  float mx = hx - 14, my = hy + 2 + jaw / 2;                      // mouth
  if (breathing && since < 1400)
    for (int n = 0; n < 3; n++)
      for (auto &f : flames)
        if (!f.alive) {
          f.alive = true; f.born = now;
          f.x = mx - 4; f.y = my + 2;                              // aimed down at the ground
          f.vx = -(1.7f + random(0, 90) / 100.0f); f.vy = 0.7f + random(0, 90) / 100.0f;
          break;
        }
  for (int pass = 0; pass < 2; pass++)                            // black halos first, so fire stands out
    for (auto &f : flames) {
      if (!f.alive) continue;
      float age = (now - f.born) / 800.0f;
      int r = 1 + (int)(age * 6);
      if (pass == 0) { display.fillCircle((int)f.x, (int)f.y, r + 2, SSD1306_BLACK); continue; }
      if (age < 0.5f) display.fillCircle((int)f.x, (int)f.y, r, SSD1306_WHITE);
      else art::ellipse(f.x, f.y, r, r, SSD1306_WHITE, age < 0.78f ? 1 : 2);   // breaking up
    }
  for (auto &f : flames) {
    if (!f.alive) continue;
    f.x += f.vx; f.y += f.vy; f.vy -= 0.03f;                       // flames lift a little as they go
    if (now - f.born >= 800 || f.x < -8 || f.y > SCREEN_H + 6) f.alive = false;
  }
  if (breathing && since < 1400) {                                // solid jet right at the mouth
    float len = 15 + sinf(t * 40) * 3, dx = -0.8f, dy = 0.6f;       // cone pointing down-left
    for (int pass = 0; pass < 2; pass++) {
      float w = pass ? 5 : 7, l = pass ? len : len + 2;
      display.fillTriangle((int)mx, (int)my, (int)(mx + dx * l + dy * w), (int)(my + dy * l - dx * w),
                           (int)(mx + dx * l - dy * w), (int)(my + dy * l + dx * w), pass ? SSD1306_WHITE : SSD1306_BLACK);
    }
  }
}

} // namespace dragon
// ========================== ROSES SCENE ==========================
// My own 1-bit rose bush: roses on long stems swaying in gusts of wind,
// leaves fluttering, clouds drifting past and now and then a petal
// blowing away.

namespace roses {

struct Rose { int8_t bx, y, r; float ph; };            // stem base x, flower height, size, phase
const Rose ROSES[] = { {14, 34, 8, 0.0f}, {36, 18, 9, 1.3f}, {60, 36, 8, 2.1f}, {78, 14, 9, 0.7f},
                       {102, 30, 8, 2.8f}, {120, 10, 7, 1.9f} };
const int ROSE_N = sizeof(ROSES) / sizeof(ROSES[0]);

struct Petal { float x, y, vy, ph; bool alive; };
Petal petals[5];

void reset() { for (auto &p : petals) p.alive = false; }

// flower centre for rose i at time t (stems bend more near the top)
void head(int i, float t, float wind, float &x, float &y) {
  const Rose &r = ROSES[i];
  float h = (64 - r.y) / 50.0f;
  x = r.bx + (wind * 3 + sinf(t * 1.9f + r.ph) * 2.2f * wind) * h;
  y = r.y + fabsf(sinf(t * 1.9f + r.ph)) * 0.6f;
}

// pointed leaf from (x,y) out to one side, with a black outline and midrib
void leaf(float x, float y, float dir, float flutter) {
  float xs[5] = { x, x + dir * 3, x + dir * 11, x + dir * 7, x + dir * 2 };
  float ys[5] = { y, y - 4 - flutter, y - 3 - flutter * 2, y + 1, y + 1.5f };
  float ox[5], oy[5];
  for (int i = 0; i < 5; i++) { ox[i] = x + (xs[i] - x) * 1.2f; oy[i] = y + (ys[i] - y) * 1.35f; }
  art::poly(ox, oy, 5, SSD1306_BLACK, 0);
  art::poly(xs, ys, 5, SSD1306_WHITE, 0);
  display.drawLine((int)x, (int)y, (int)(x + dir * 9), (int)(y - 2.5f - flutter * 2), SSD1306_BLACK);
}

// a rose seen from the side: lumpy petal cup, curl in the middle, petal edges
void rose(int x, int y, int r, float spin) {
  // sepals hanging under the flower
  display.fillTriangle(x - 2, y + r - 2, x - r - 2, y + r + 3, x - 1, y + r + 2, SSD1306_WHITE);
  display.fillTriangle(x + 2, y + r - 2, x + r + 2, y + r + 3, x + 1, y + r + 2, SSD1306_WHITE);
  const float B[][3] = { {0, 0, 1.0f}, {-0.55f, -0.55f, 0.55f}, {0.55f, -0.55f, 0.55f}, {0, -0.75f, 0.55f} };
  for (auto &b : B) display.fillCircle(x + (int)(b[0] * r), y + (int)(b[1] * r), (int)(b[2] * r) + 1, SSD1306_BLACK);
  for (auto &b : B) display.fillCircle(x + (int)(b[0] * r), y + (int)(b[1] * r), (int)(b[2] * r), SSD1306_WHITE);
  // the curl of petals at the top centre
  for (float a = 0; a < 3 * PI; a += 0.12f) {
    float d = a / (3 * PI) * r * 0.62f;
    art::px(x + (int)roundf(cosf(a + spin) * d), y - (int)(r * 0.3f) + (int)roundf(sinf(a + spin) * d * 0.75f), SSD1306_BLACK);
  }
  // outer petals folding round the front, meeting low in the middle
  display.drawLine(x - r + 1, y - r / 3, x - 1, y + r - 2, SSD1306_BLACK);
  display.drawLine(x + r - 1, y - r / 3, x + 1, y + r - 2, SSD1306_BLACK);
}

void cloud(float cx, float cy) {
  static const int8_t C[][3] = { {-9, 2, 4}, {-3, -1, 6}, {5, 0, 5}, {11, 3, 3} };
  for (auto &c : C) display.fillCircle((int)cx + c[0], (int)cy + c[1], c[2], SSD1306_WHITE);
  for (auto &c : C) display.fillCircle((int)cx + c[0], (int)cy + c[1], c[2] - 1, SSD1306_BLACK);
  display.drawFastHLine((int)cx - 13, (int)cy + 6, 27, SSD1306_BLACK);   // flat bottom
}

void frame(unsigned long now) {
  display.clearDisplay();
  float t = now / 1000.0f;
  float wind = 0.6f + 0.3f * sinf(t * 0.45f) + 0.25f * max(0.0f, sinf(t * 1.3f));   // gusts

  // ---- clouds drifting with the wind ----
  cloud(fmodf(t * 3, 170) - 20, 9);
  cloud(fmodf(t * 3 + 90, 170) - 20, 4);

  // ---- stems, leaves, then the roses on top ----
  float fx[ROSE_N], fy[ROSE_N];
  for (int i = 0; i < ROSE_N; i++) {
    head(i, t, wind, fx[i], fy[i]);
    const Rose &r = ROSES[i];
    float px0 = r.bx, py0 = 64;
    for (int s = 1; s <= 12; s++) {                                // curved stem
      float k = s / 12.0f, a = 1 - k;
      float x = a * a * r.bx + 2 * a * k * r.bx + k * k * fx[i];
      float y = a * a * 64 + 2 * a * k * ((64 + fy[i]) / 2) + k * k * fy[i];
      display.drawLine((int)px0, (int)py0, (int)x, (int)y, SSD1306_WHITE);
      if (s == 5) leaf(x, y, (i & 1) ? 1 : -1, sinf(t * 5 + i) * wind);
      px0 = x; py0 = y;
    }
  }
  for (int i = 0; i < ROSE_N; i++) {
    rose((int)fx[i], (int)fy[i], ROSES[i].r, ROSES[i].ph + sinf(t * 1.9f + ROSES[i].ph) * 0.3f);
  }

  // ---- bush along the bottom ----
  for (int j = 0; j < 20; j++) {
    float lx = j * 7 - 4 + sinf(t * 2 + j) * wind * 1.5f, ly = 57 + (j % 3) * 3;
    leaf(lx, ly, (j & 1) ? 1 : -1, sinf(t * 4 + j * 1.7f) * wind);
  }

  // ---- a petal now and then, carried off by the wind ----
  for (auto &p : petals) {
    if (!p.alive) {
      if (random(160) == 0) {
        int i = random(ROSE_N);
        p.alive = true; p.x = fx[i] + 3; p.y = fy[i]; p.vy = 0.1f + random(0, 20) / 100.0f; p.ph = random(0, 628) / 100.0f;
      }
      continue;
    }
    p.x += 0.5f + wind * 0.9f;
    p.y += p.vy + sinf(t * 4 + p.ph) * 0.3f;
    if (p.x > SCREEN_W + 3 || p.y > SCREEN_H) { p.alive = false; continue; }
    display.fillRect((int)p.x - 1, (int)p.y - 1, 4, 3, SSD1306_BLACK);
    art::px((int)p.x, (int)p.y, SSD1306_WHITE); art::px((int)p.x + 1, (int)p.y, SSD1306_WHITE);
    art::px((int)p.x + (sinf(t * 6 + p.ph) > 0 ? 2 : -1), (int)p.y + 1, SSD1306_WHITE);
  }
}

} // namespace roses
// =========================== PLAYLIST ===========================
// What the keychain plays, in order: up to PLAY_MAX items, each either
// one of the animations above or a picture sent from the phone.
// Saved in LittleFS as /play.bin, pictures as /p<id>.bin (1024 bytes,
// already in screen-buffer layout, so showing one is a memcpy).

namespace play {

const char *ANIM_NAMES[] = { "Mermaid tail", "Shell", "Beach", "Starry night", "Robot eyes", "Tea", "Dragon", "Roses" };
const int ANIM_COUNT = SCENE_PHOTO;  // the animations come before SCENE_PHOTO in the enum

uint8_t n = 0;
uint8_t kind[PLAY_MAX];  // KIND_ANIM or KIND_PIC
uint8_t id[PLAY_MAX];    // animation number, or picture file number
uint8_t fx[PLAY_MAX];    // pictures: which Effect plays on it
uint8_t seconds = SCENE_SECONDS;
bool fsOk = false;

int pos = 0;               // item playing now
int jumpTo = -1;           // set by the web page: play this item now
uint8_t pic[PHOTO_BYTES];  // the picture on screen
unsigned long picShownAt = 0;

String picPath(int p) {
  return String("/p") + p + ".bin";
}

void setDefault() {
  n = 0;
  for (int a = 0; a < ANIM_COUNT && n < PLAY_MAX; a++) {
    kind[n] = KIND_ANIM;
    id[n] = a;
    fx[n] = 0;
    n++;
  }
}

void save() {
  if (!fsOk) return;
  // write a new file, then swap it in: switching off halfway can't lose the list
  File f = LittleFS.open("/play.tmp", "w");
  if (!f) return;
    uint8_t hdr[5] = { 'K', 'P', '4', seconds, n };
  f.write(hdr, 5);
  for (int i = 0; i < n; i++) {
    f.write(kind[i]);
    f.write(id[i]);
    f.write(fx[i]);
  }
  f.close();
  LittleFS.rename("/play.tmp", "/play.bin");
}

bool picInList(int p) {
  for (int i = 0; i < n; i++)
    if (kind[i] == KIND_PIC && id[i] == p) return true;
  return false;
}

// lowest picture number that isn't used by the list or a file
int newPicId() {
  for (int p = 0; p < 200; p++)
    if (!picInList(p) && !LittleFS.exists(picPath(p))) return p;
  return -1;
}

bool writePic(int p, const uint8_t *data) {
  File f = LittleFS.open(picPath(p), "w");
  if (!f) return false;
  bool ok = f.write(data, PHOTO_BYTES) == PHOTO_BYTES;
  f.close();
  return ok;
}

// picture number from a file name like "p12.bin", or -1 for anything else.
// (It must be strict: "play.bin" also starts with p and ends with .bin, and
// once got deleted as "picture 0", which wiped the list.)
int picIdFromName(String name) {
  if (name.startsWith("/")) name.remove(0, 1);
  if (name.length() < 6 || !name.startsWith("p") || !name.endsWith(".bin")) return -1;
  String num = name.substring(1, name.length() - 4);
  for (unsigned i = 0; i < num.length(); i++)
    if (!isDigit(num[i])) return -1;
  int v = num.toInt();
  return v < 200 ? v : -1;
}

// delete picture files that are no longer in the list
void cleanup() {
  int gone[64], g = 0;
  Dir d = LittleFS.openDir("/");
  while (d.next() && g < 64) {
    int p = picIdFromName(d.fileName());
    if (p >= 0 && !picInList(p)) gone[g++] = p;
  }
  for (int i = 0; i < g; i++) LittleFS.remove(picPath(gone[i]));
}

// picture files that exist but aren't in the list (the old bug above could
// lose the list): put them back at the end so nothing is lost
void adoptOrphans() {
  bool added = false;
  Dir d = LittleFS.openDir("/");
  while (d.next() && n < PLAY_MAX) {
    int p = picIdFromName(d.fileName());
    if (p >= 0 && !picInList(p)) {
      kind[n] = KIND_PIC;
      id[n] = p;
      fx[n] = FX_STILL;
      n++;
      added = true;
    }
  }
  if (added) save();
}

// pictures from the first version of this sketch lived in the EEPROM sector
void importOldPictures() {
  const int OLD_SLOTS = 3, OLD_HDR = 16;
  EEPROM.begin(OLD_HDR + OLD_SLOTS * PHOTO_BYTES);
  uint32_t m = 0;
  EEPROM.get(0, m);
  if (m == 0x4B503031UL) {  // "KP01"
    uint8_t used = EEPROM.read(4);
    for (int s = 0; s < OLD_SLOTS; s++) {
      if (!((used >> s) & 1) || n >= PLAY_MAX) continue;
      int p = newPicId();
      if (p >= 0 && writePic(p, EEPROM.getConstDataPtr() + OLD_HDR + s * PHOTO_BYTES)) {
        kind[n] = KIND_PIC;
        id[n] = p;
        fx[n] = FX_STILL;
        n++;
      }
    }
    EEPROM.put(0, (uint32_t)0);  // only once
    EEPROM.commit();
    save();
  }
  EEPROM.end();
}

void load() {
  setDefault();
  fsOk = LittleFS.begin();
  if (!fsOk) return;  // Flash Size has no FS: animations only

  File f = LittleFS.open("/play.bin", "r");
  if (f && f.size() >= 5) {
    uint8_t hdr[5];
    f.read(hdr, 5);
        bool v3 = hdr[2] >= '3';               // v2 files have no effect byte
    if (hdr[0] == 'K' && hdr[1] == 'P' && hdr[2] >= '2' && hdr[2] <= '4') {
      seconds = max((int)hdr[3], 3);
      n = 0;
      for (int i = 0; i < hdr[4] && i < PLAY_MAX; i++) {
        int k = f.read(), v = f.read(), e = v3 ? f.read() : 0;
        bool ok = (k == KIND_ANIM && v >= 0 && v < ANIM_COUNT) || (k == KIND_PIC && LittleFS.exists(picPath(v)));
        if (ok) {
          kind[n] = k;
          id[n] = v;
          fx[n] = (e >= 0 && e < FX_COUNT) ? e : 0;
          n++;
        }
      }
            if (n == 0) setDefault();
      if (hdr[2] < '4') {                  // saved before tea, dragon and roses existed
        for (int a = SCENE_TEA; a <= SCENE_ROSES && n < PLAY_MAX; a++) { kind[n] = KIND_ANIM; id[n] = a; fx[n] = FX_STILL; n++; }
        save();
      }
    }
  }
  if (f) f.close();
  adoptOrphans();
  importOldPictures();
}

bool loadPic(int p) {
  File f = LittleFS.open(picPath(p), "r");
  bool ok = f && f.read(pic, PHOTO_BYTES) == PHOTO_BYTES;
  if (f) f.close();
  if (!ok) memset(pic, 0, PHOTO_BYTES);
  return ok;
}

// ---------- the picture scene with its live effect ----------
// Every effect is worked out from the stored picture each frame:
// shifting rows (sway, ripple, glitch), moving it (bob), scaling it
// (heartbeat), or drawing particles over it (sparkle, hearts, snow).

struct Part {
  float x, y, v, w;
  unsigned long born;
  uint16_t life;
  bool alive;
};
Part parts[24];
unsigned long glitchUntil = 0, nextGlitch = 0;

static const char HEART[] PROGMEM =
  ".##.##."
  "#######"
  "#######"
  ".#####."
  "..###.."
  "...#...";

inline uint8_t srcPx(int x, int y) {
  if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return 0;
  return (pic[(y >> 3) * SCREEN_W + x] >> (y & 7)) & 1;
}

inline void px(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H) display.drawPixel(x, y, c);
}

void fxReset(int e) {
  for (auto &p : parts) p.alive = false;
  nextGlitch = millis() + 1500;
  glitchUntil = 0;
  if (e == FX_SNOW)
    for (int i = 0; i < 22; i++) {
      Part &p = parts[i];
      p.alive = true;
      p.x = random(0, SCREEN_W);
      p.y = random(0, SCREEN_H);
      p.v = 0.25f + random(0, 45) / 100.0f;
      p.w = random(0, 628) / 100.0f;
    }
}

// copy the picture with every row moved sideways by dx[row], all moved down by dy
void blitRows(uint8_t *buf, const int8_t *dx, int dy) {
  memset(buf, 0, PHOTO_BYTES);
  for (int y = 0; y < SCREEN_H; y++) {
    uint8_t *row = buf + (y >> 3) * SCREEN_W, bit = 1 << (y & 7);
    int d = dx ? dx[y] : 0;
    for (int x = 0; x < SCREEN_W; x++)
      if (srcPx(x - d, y - dy)) row[x] |= bit;
  }
}

void heart(int cx, int cy) {
  for (int pass = 0; pass < 2; pass++)  // black outline first, then the heart
    for (int r = 0; r < 6; r++)
      for (int c = 0; c < 7; c++) {
        if (pgm_read_byte(&HEART[r * 7 + c]) != '#') continue;
        int x = cx - 3 + c, y = cy - 3 + r;
        if (pass == 0) {
          px(x - 1, y, SSD1306_BLACK);
          px(x + 1, y, SSD1306_BLACK);
          px(x, y - 1, SSD1306_BLACK);
          px(x, y + 1, SSD1306_BLACK);
        } else px(x, y, SSD1306_WHITE);
      }
}

void photoFrame(unsigned long now) {
  uint8_t *buf = display.getBuffer();
  float t = now / 1000.0f;
  int e = fx[pos];
  static int8_t dx[SCREEN_H];

  if (e == FX_SWAY) {  // like the mermaid tail: top moves most
    for (int y = 0; y < SCREEN_H; y++) {
      float h = (float)(SCREEN_H - 1 - y) / (SCREEN_H - 1);
      dx[y] = (int8_t)roundf(2.6f * h * h * sinf(t * 3.2f - 1.7f * h));
    }
    blitRows(buf, dx, 0);
  } else if (e == FX_BOB) {
    blitRows(buf, NULL, (int)roundf(sinf(t * 2.2f) * 2));
  } else if (e == FX_RIPPLE) {
    for (int y = 0; y < SCREEN_H; y++) dx[y] = (int8_t)roundf(sinf(y * 0.35f + t * 5) * 1.6f);
    blitRows(buf, dx, 0);
  } else if (e == FX_GLITCH) {
    if (now > nextGlitch) {
      glitchUntil = now + 350;
      nextGlitch = now + random(1500, 4000);
    }
    if (now < glitchUntil) {
      memset(dx, 0, sizeof(dx));
      for (int b = 0; b < 4; b++) {
        int s = random(0, SCREEN_H), l = random(2, 8), sh = random(-10, 11);
        for (int y = s; y < s + l && y < SCREEN_H; y++) dx[y] = sh;
      }
      blitRows(buf, dx, 0);
      if (random(6) == 0)
        for (int i = 0; i < PHOTO_BYTES; i++) buf[i] = ~buf[i];
    } else memcpy(buf, pic, PHOTO_BYTES);
  } else if (e == FX_PULSE) {  // lub-dub heartbeat
    float p = fmodf(t, 1.3f);
    float bump = expf(-sq((p - 0.10f) / 0.06f)) + 0.6f * expf(-sq((p - 0.33f) / 0.07f));
    int32_t inv = (int32_t)(65536 / (1 + 0.08f * bump));
    memset(buf, 0, PHOTO_BYTES);
    for (int y = 0; y < SCREEN_H; y++) {
      int sy = 32 + (((y - 32) * inv) >> 16);
      uint8_t *row = buf + (y >> 3) * SCREEN_W, bit = 1 << (y & 7);
      for (int x = 0; x < SCREEN_W; x++)
        if (srcPx(64 + (((x - 64) * inv) >> 16), sy)) row[x] |= bit;
    }
  } else {
    memcpy(buf, pic, PHOTO_BYTES);
  }

  // ---- particles drawn over the picture ----
  if (e == FX_SPARKLE) {  // XOR stars show on black and white
    for (auto &p : parts) {
      if (!p.alive) {
        if (random(12) == 0) {
          p.alive = true;
          p.x = random(3, SCREEN_W - 3);
          p.y = random(3, SCREEN_H - 3);
          p.born = now;
          p.life = random(600, 1400);
        }
        continue;
      }
      float k = (float)(now - p.born) / p.life;
      if (k >= 1) {
        p.alive = false;
        continue;
      }
      float s = sinf(k * PI);
      int size = s > 0.8f ? 2 : (s > 0.35f ? 1 : 0);
      int x = p.x, y = p.y;
      px(x, y, SSD1306_INVERSE);
      for (int i = 1; i <= size; i++) {
        px(x + i, y, SSD1306_INVERSE);
        px(x - i, y, SSD1306_INVERSE);
        px(x, y + i, SSD1306_INVERSE);
        px(x, y - i, SSD1306_INVERSE);
      }
    }
  } else if (e == FX_HEARTS) {
    for (int i = 0; i < 6; i++) {
      Part &p = parts[i];
      if (!p.alive) {
        if (random(25) == 0) {
          p.alive = true;
          p.x = random(4, SCREEN_W - 4);
          p.y = SCREEN_H + 6;
          p.v = 0.35f + random(0, 35) / 100.0f;
          p.w = random(0, 628) / 100.0f;
        }
        continue;
      }
      p.y -= p.v;
      if (p.y < -8) {
        p.alive = false;
        continue;
      }
      heart((int)(p.x + sinf(t * 2 + p.w) * 2), (int)p.y);
    }
  } else if (e == FX_SNOW) {
    for (auto &p : parts) {
      if (!p.alive) continue;
      p.y += p.v;
      p.x += sinf(t + p.w) * 0.3f;
      if (p.y > SCREEN_H + 1) {
        p.y = -1;
        p.x = random(0, SCREEN_W);
      }
      int x = p.x, y = p.y;
      px(x, y, SSD1306_INVERSE);
      if (p.v > 0.5f) {
        px(x + 1, y, SSD1306_INVERSE);
        px(x - 1, y, SSD1306_INVERSE);
        px(x, y + 1, SSD1306_INVERSE);
        px(x, y - 1, SSD1306_INVERSE);
      }
    }
  }

  // ---- fade in with a random-dot dissolve when it first appears ----
  unsigned long k = now - picShownAt;
  if (k < 700) {
    uint8_t thr = k * 255 / 700;
    for (int i = 0; i < PHOTO_BYTES; i++) {
      uint8_t mask = 0;
      for (int b = 0; b < 8; b++)
        if (beach::hash2(i, b) < thr) mask |= 1 << b;
      buf[i] &= mask;
    }
  }
}

}  // namespace play
// ======================= PHONE WEB PAGE =======================
// Served by the keychain itself. All the image work (crop, dither,
// message text) happens on the phone; the keychain just gets 1024
// bytes as hex. (No "function" keyword in here: the Arduino IDE would
// mistake it for C++ and break the build. Arrow functions only.)

static const char PAGE[] PROGMEM = R"=====(<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name=theme-color content="#0b0d12"><title>Keychain</title>
<style>
:root{--bg:#0b0d12;--card:#151924;--line:#262c3b;--text:#eef1f7;--dim:#8a93a8;--acc:#7cc6ff;--bad:#ff8080;--ok:#7be0a6}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main,#ed{max-width:520px;margin:0 auto;padding:18px 16px 40px}
h1{font-size:23px;margin:2px 0 4px}
#conn{font-size:13px;color:var(--dim);display:flex;align-items:center;gap:7px}
#conn:before{content:"";width:8px;height:8px;border-radius:50%;background:var(--dim)}
#conn.ok:before{background:var(--ok)}#conn.bad:before{background:var(--bad)}
.warn{background:#2a1a1a;border:1px solid #5a2a2a;color:#ffb3b3;border-radius:12px;padding:10px 12px;font-size:14px;margin-top:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px;margin:14px 0}
.card h2{font-size:14px;font-weight:600;color:var(--dim);margin:2px 2px 10px;display:flex;justify-content:space-between}
.item{display:flex;align-items:center;gap:10px;padding:8px 0;border-top:1px solid var(--line)}
.item:first-of-type{border-top:0}
.item .th{width:96px;flex:none;cursor:pointer;position:relative}
.item canvas{display:block;width:96px;height:48px;image-rendering:pixelated;background:#000;border-radius:5px}
.item .an{width:96px;height:48px;border-radius:5px;background:#000;display:grid;place-items:center;font-size:24px}
.item.now .th:after{content:"on screen";position:absolute;left:4px;bottom:3px;font-size:10px;background:var(--acc);color:#06121d;border-radius:4px;padding:0 4px;font-weight:700}
.item .nm{flex:1;min-width:0;font-size:14px}
.item .nm small{display:block;color:var(--dim);font-size:12px}
.item .ctl{display:flex;gap:2px}
.ctl button{width:34px;height:34px;border-radius:9px;font-size:16px;color:var(--dim);border:1px solid var(--line)}
.ctl button:disabled{opacity:.25}
.ctl .x{color:var(--bad)}
canvas{display:block;width:100%;aspect-ratio:2/1;image-rendering:pixelated;background:#000;border-radius:6px}
button{font:inherit;color:inherit;background:none;border:0;cursor:pointer}
.seg{display:flex;background:#0e1119;border:1px solid var(--line);border-radius:10px;padding:3px;gap:3px}
.seg button{flex:1;padding:9px 4px;border-radius:8px;font-size:14px;color:var(--dim)}
.seg button.on{background:#232a3b;color:var(--text);font-weight:600}
.adds{display:flex;gap:8px;margin-top:12px}.adds button{flex:1}
.full{font-size:13px;color:var(--dim);text-align:center;margin-top:10px}
.foot{font-size:12.5px;color:var(--dim);margin-top:18px}
#ed .top{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.ghost{padding:10px 12px;border:1px solid var(--line);border-radius:10px;font-size:14px}
.ghost.on{border-color:var(--acc);color:var(--acc)}
.ghost:disabled,.primary:disabled{opacity:.4}
.bezel{background:#000;padding:10px;border-radius:12px;border:1px solid var(--line)}
#pv{touch-action:none;cursor:grab}
.tip{font-size:12.5px;color:var(--dim);text-align:center;margin:8px 0 14px}
label{display:flex;align-items:center;gap:12px;font-size:14px;color:var(--dim);margin:10px 2px}
label span{width:84px}
input[type=range]{flex:1;accent-color:var(--acc)}
.row{display:flex;gap:8px;margin:12px 0}.row button{flex:1}
#txt{width:100%;font:inherit;color:var(--text);background:#0e1119;border:1px solid var(--line);border-radius:10px;padding:11px 12px;margin:4px 0 14px}
.primary{padding:12px;border-radius:12px;background:var(--acc);color:#06121d;font-weight:700;font-size:15px}
#send{width:100%;padding:14px;font-size:16px}
#sheet{position:fixed;inset:0;background:#000a;display:flex;align-items:flex-end;justify-content:center}
#sheet>div{background:var(--card);border:1px solid var(--line);border-radius:16px 16px 0 0;width:100%;max-width:520px;padding:14px 16px 24px}
#sheet h2{font-size:16px;margin:4px 0 10px}
#sheet .opt{display:flex;align-items:center;gap:12px;width:100%;padding:12px 6px;border-top:1px solid var(--line);text-align:left;font-size:15px}
#sheet .opt span{font-size:22px;width:30px;text-align:center}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(120px);background:#232a3b;border:1px solid var(--line);padding:11px 16px;border-radius:12px;font-size:14px;transition:transform .25s;max-width:90vw;text-align:center}
#toast.show{transform:translateX(-50%)}
.lbl{font-size:14px;font-weight:600;color:var(--dim);margin:16px 2px 8px}
.fxg{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
.fxg button{padding:10px 4px;border-radius:10px;border:1px solid var(--line);font-size:14px;color:var(--dim)}
.fxg button.on{border-color:var(--acc);color:var(--acc);font-weight:600;background:#12202e}
.fxb{font-size:12px;color:var(--acc);border:1px solid #24405a;border-radius:7px;padding:2px 7px;margin-top:3px}
#sheet .opt.on{color:var(--acc);font-weight:600}
[hidden]{display:none!important}
</style></head><body>
<main id=home>
<h1>Keychain</h1><div id=conn>Connecting...</div>
<div id=fswarn class=warn hidden>This keychain can't save pictures: its firmware was built with Flash Size "FS:none". Ask whoever made it to re-flash with "4MB (FS:2MB OTA:~1019KB)".</div>
<div class=card>
<h2><span>Playing on the keychain</span><span id=cnt></span></h2>
<div id=list></div>
<div class=adds><button id=addp class=primary>+ Add a picture</button><button id=adda class=ghost>+ Add an animation</button></div>
<div id=full class=full hidden>The list is full. Remove something to add more.</div>
</div>
<div class=card>
<h2>Each one plays for</h2>
<div class=seg id=sec><button data-v=10>10 s</button><button data-v=20>20 s</button><button data-v=30>30 s</button><button data-v=60>1 min</button></div>
</div>
<p class=foot>Tap a picture or animation to show it on the keychain now. Use the arrows to change the order.<br><br>The keychain's Wi-Fi switches itself off 2 minutes after you stop using this page. To change things later, switch the keychain off and on again, then reconnect.</p>
</main>
<section id=ed hidden>
<div class=top><button id=cancel class=ghost>Cancel</button><b>New picture</b><span style="width:64px"></span></div>
<div class=bezel><canvas id=pv width=128 height=64></canvas></div>
<p class=tip>Drag to move. This is exactly what the keychain will show.</p>
<div class=seg id=style><button data-v=0>Photo</button><button data-v=1>Bold</button><button data-v=2>Outline</button></div>
<p class=lbl>Animation</p>
<div class=fxg id=fxg></div>
<p class=lbl>Adjust</p>
<label><span>Zoom</span><input type=range id=zoom min=-1.8 max=2 step=0.01 value=0></label>
<label><span>Brightness</span><input type=range id=bri min=-100 max=100 value=0></label>
<label><span>Contrast</span><input type=range id=con min=-100 max=100 value=0></label>
<div class=row><button id=fit class=ghost>Show whole picture</button><button id=inv class=ghost>Invert</button></div>
<input id=txt maxlength=22 placeholder="Add a short message (optional)" autocomplete=off>
<button id=send class=primary>Send to keychain</button>
</section>
<div id=sheet hidden><div><h2 id=sht></h2><div id=opts></div></div></div>
<input type=file id=file accept="image/*" hidden>
<div id=toast></div>
<script>
const W=128,H=64,PX=[230,244,255],ICON=['\u{1F9DC}','\u{1F41A}','\u{1F3D6}️','\u{1F30C}','\u{1F916}','\u2615','\u{1F409}','\u{1F339}'];
const $=s=>document.querySelector(s);
const FX=['Still','Sway','Bob','Sparkle','Hearts','Snow','Ripple','Heartbeat','Glitch'],FXI=['\u25A0','\u3030','\u2195','\u2728','\u2665','\u2744','\u224B','\u{1F493}','\u26A1'];
const HEART=['.##.##.','#######','#######','.#####.','..###..','...#...'];
const rnd=(a,b)=>a+Math.random()*(b-a);
let fxs={};
const fxReset=e=>{fxs={p:[],gUntil:0,gNext:performance.now()+1500};if(e==5)for(let i=0;i<22;i++)fxs.p.push({x:rnd(0,W),y:rnd(0,H),v:rnd(.25,.7),w:rnd(0,6.28)})};
const fxFrame=(src,e,now)=>{
 const t=now/1000,o=new Uint8Array(W*H),g=(x,y)=>x>=0&&x<W&&y>=0&&y<H?src[y*W+x]:0;
 const X=(x,y)=>{x|=0;y|=0;if(x>=0&&x<W&&y>=0&&y<H)o[y*W+x]^=1},S=(x,y,v)=>{x|=0;y|=0;if(x>=0&&x<W&&y>=0&&y<H)o[y*W+x]=v};
 let dx=null,dy=0,inv=false;
 if(e==1){dx=[];for(let y=0;y<H;y++){const h=(H-1-y)/(H-1);dx[y]=Math.round(2.6*h*h*Math.sin(t*3.2-1.7*h))}}
 else if(e==2)dy=Math.round(Math.sin(t*2.2)*2);
 else if(e==6){dx=[];for(let y=0;y<H;y++)dx[y]=Math.round(Math.sin(y*.35+t*5)*1.6)}
 else if(e==8){if(now>fxs.gNext){fxs.gUntil=now+350;fxs.gNext=now+rnd(1500,4000)}
  if(now<fxs.gUntil){dx=new Array(H).fill(0);for(let b=0;b<4;b++){const s=rnd(0,H)|0,l=rnd(2,8)|0,sh=Math.round(rnd(-10,10));for(let y=s;y<s+l&&y<H;y++)dx[y]=sh}inv=Math.random()<1/6}}
 if(e==7){const p=t%1.3,bump=Math.exp(-(((p-.1)/.06)**2))+.6*Math.exp(-(((p-.33)/.07)**2)),k=1+.08*bump;
  for(let y=0;y<H;y++)for(let x=0;x<W;x++)o[y*W+x]=g(Math.floor(64+(x-64)/k),Math.floor(32+(y-32)/k))}
 else for(let y=0;y<H;y++){const d=dx?dx[y]:0;for(let x=0;x<W;x++)o[y*W+x]=g(x-d,y-dy)}
 if(inv)for(let i=0;i<W*H;i++)o[i]^=1;
 if(e==3){if(fxs.p.length<6&&Math.random()<1/12)fxs.p.push({x:rnd(3,W-3),y:rnd(3,H-3),born:now,life:rnd(600,1400)});
  fxs.p=fxs.p.filter(q=>{const k=(now-q.born)/q.life;if(k>=1)return false;const s=Math.sin(k*Math.PI),z=s>.8?2:s>.35?1:0;X(q.x,q.y);for(let i=1;i<=z;i++){X(q.x+i,q.y);X(q.x-i,q.y);X(q.x,q.y+i);X(q.x,q.y-i)}return true})}
 else if(e==4){if(fxs.p.length<6&&Math.random()<1/25)fxs.p.push({x:rnd(4,W-4),y:H+6,v:rnd(.35,.7),w:rnd(0,6.28)});
  fxs.p=fxs.p.filter(q=>{q.y-=q.v;if(q.y<-8)return false;const cx=(q.x+Math.sin(t*2+q.w)*2)|0,cy=q.y|0;
   for(const pass of[0,1])HEART.forEach((r,ry)=>{for(let c=0;c<7;c++)if(r[c]=='#'){const x=cx-3+c,y=cy-3+ry;if(pass)S(x,y,1);else{S(x-1,y,0);S(x+1,y,0);S(x,y-1,0);S(x,y+1,0)}}});return true})}
 else if(e==5){for(const q of fxs.p){q.y+=q.v;q.x+=Math.sin(t+q.w)*.3;if(q.y>H+1){q.y=-1;q.x=rnd(0,W)}X(q.x,q.y);if(q.v>.5){X(q.x+1,q.y);X(q.x-1,q.y);X(q.x,q.y+1);X(q.x,q.y-1)}}}
 return o};
let st={name:'',items:[],fx:[],anims:[],max:6,sec:20,bat:0,fs:1,now:0},pics={},src=null,bits=new Uint8Array(W*H),ed={},busy=false;
const toast=m=>{const t=$('#toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2800)};
const api=async(p,o)=>{const r=await fetch(p,o);if(!r.ok)throw new Error(await r.text()||r.status);return r};
const hex2b=h=>{const b=new Uint8Array(h.length>>1);for(let i=0;i<b.length;i++)b[i]=parseInt(h.substr(i*2,2),16);return b};
const b2hex=b=>Array.from(b,v=>v.toString(16).padStart(2,'0')).join('');
const pack=bt=>{const o=new Uint8Array(W*H/8);for(let y=0;y<H;y++)for(let x=0;x<W;x++)if(bt[y*W+x])o[(y>>3)*W+x]|=1<<(y&7);return o};
const unpack=b=>{const bt=new Uint8Array(W*H);for(let y=0;y<H;y++)for(let x=0;x<W;x++)bt[y*W+x]=(b[(y>>3)*W+x]>>(y&7))&1;return bt};
const paint=(c,bt)=>{const x=c.getContext('2d'),d=x.createImageData(W,H);for(let i=0;i<W*H;i++){const on=bt[i];d.data[i*4]=on?PX[0]:0;d.data[i*4+1]=on?PX[1]:0;d.data[i*4+2]=on?PX[2]:0;d.data[i*4+3]=255}x.putImageData(d,0,0)};

/* ---------- the list ---------- */
const nameOf=it=>it[0]=='a'?st.anims[+it.slice(1)]:'Picture';
const drawList=()=>{
 const L=$('#list'),n=st.items.length;L.innerHTML='';
 st.items.forEach((it,i)=>{
  const r=document.createElement('div');r.className='item'+(i==st.now?' now':'');
  const th=document.createElement('div');th.className='th';
  if(it[0]=='p'){const c=document.createElement('canvas');c.width=W;c.height=H;th.appendChild(c);loadPic(it,c)}
  else th.innerHTML='<div class=an>'+ICON[+it.slice(1)]+'</div>';
  th.onclick=()=>showNow(i);
  r.appendChild(th);
  r.insertAdjacentHTML('beforeend','<div class=nm>'+nameOf(it)+(it[0]=='a'?'<small>Animation</small>':'<small><button class=fxb>'+FXI[st.fx[i]||0]+' '+FX[st.fx[i]||0]+'</button></small>')+'</div><div class=ctl><button aria-label="Move up">↑</button><button aria-label="Move down">↓</button><button class=x aria-label=Remove>✕</button></div>');
  const b=r.querySelectorAll('.ctl button');
  b[0].disabled=i==0;b[1].disabled=i==n-1;b[2].disabled=n<=1;
  const fb=r.querySelector('.fxb');if(fb)fb.onclick=()=>fxSheet(i);
  b[0].onclick=()=>move(i,-1);b[1].onclick=()=>move(i,1);b[2].onclick=()=>remove(i);
  L.appendChild(r)});
 $('#cnt').textContent=n+' of '+st.max;
 const full=n>=st.max;$('#addp').disabled=full||!st.fs;$('#adda').disabled=full||!st.anims.some((a,k)=>!st.items.includes('a'+k));$('#full').hidden=!full;
 $('#fswarn').hidden=!!st.fs;
 for(const b of $('#sec').children)b.classList.toggle('on',+b.dataset.v==st.sec)};
let picQ=Promise.resolve();
const loadPic=async(it,c)=>{if(!pics[it]){pics[it]=picQ=picQ.then(()=>api('/pic?id='+it.slice(1))).then(r=>r.text()).then(h=>unpack(hex2b(h))).catch(()=>{delete pics[it];return new Uint8Array(W*H)})}paint(c,await pics[it])};
const load=async()=>{const c=$('#conn');try{st=await (await api('/state')).json();c.className='ok';let t='Connected to '+st.name;if(st.bat)t+=' · battery '+Math.max(0,Math.min(100,Math.round((st.bat-3.4)/0.75*100)))+'%';c.textContent=t}catch(e){c.className='bad';c.textContent="Not connected. Join the keychain's Wi-Fi first."}drawList()};
const saveList=async(items,msg)=>{if(busy)return;busy=true;const old=st.items,oldFx=st.fx;st.items=items;st.fx=items.map(it=>oldFx[old.indexOf(it)]||0);drawList();
 try{const r=await (await api('/list?items='+items.join(','),{method:'POST'})).json();st.fx=r.fx;st.now=r.now;drawList();if(msg)toast(msg)}
 catch(e){st.items=old;st.fx=oldFx;drawList();toast("Couldn't reach the keychain")}busy=false};
const fxSheet=i=>{$('#sht').textContent='Animation for this picture';const o=$('#opts');o.innerHTML='';FX.forEach((f,e)=>{const b=document.createElement('button');b.className='opt'+(e==(st.fx[i]||0)?' on':'');b.innerHTML='<span>'+FXI[e]+'</span>'+f;
 b.onclick=async()=>{$('#sheet').hidden=true;try{await api('/fx?i='+i+'&e='+e,{method:'POST'});st.fx[i]=e;st.now=i;drawList();toast(f+' — look at the keychain')}catch(x){toast("Couldn't reach the keychain")}};o.appendChild(b)});$('#sheet').hidden=false};
const move=(i,d)=>{const a=st.items.slice();[a[i],a[i+d]]=[a[i+d],a[i]];saveList(a)};
const remove=i=>{const it=st.items[i];if(it[0]=='p'&&!confirm('Remove this picture? It will be deleted from the keychain.'))return;saveList(st.items.filter((x,k)=>k!=i),'Removed')};
const showNow=async i=>{try{await api('/show?i='+i,{method:'POST'});st.now=i;drawList();toast('Showing it now')}catch(e){toast("Couldn't reach the keychain")}};
$('#sec').onclick=async e=>{const v=e.target.dataset.v;if(v==null)return;const old=st.sec;st.sec=+v;drawList();try{await api('/set?sec='+v,{method:'POST'});toast('Saved')}catch(x){st.sec=old;drawList();toast("Couldn't reach the keychain")}};
$('#adda').onclick=()=>{$('#sht').textContent='Add an animation';const o=$('#opts');o.innerHTML='';st.anims.forEach((a,k)=>{if(st.items.includes('a'+k))return;const b=document.createElement('button');b.className='opt';b.innerHTML='<span>'+ICON[k]+'</span>'+a;
 b.onclick=async()=>{$('#sheet').hidden=true;try{const r=await (await api('/add?a='+k,{method:'POST'})).json();st.items=r.items;st.fx=r.fx;st.now=r.now;drawList();toast('Added')}catch(e){toast("Couldn't reach the keychain")}};o.appendChild(b)});$('#sheet').hidden=false};
$('#sheet').onclick=e=>{if(e.target.id=='sheet')$('#sheet').hidden=true};

/* ---------- picking a photo ---------- */
$('#addp').onclick=()=>$('#file').click();
$('#file').onchange=e=>{const f=e.target.files[0];e.target.value='';if(!f)return;const u=URL.createObjectURL(f),im=new Image();im.onload=()=>{const k=Math.min(1,700/Math.max(im.width,im.height)),c=document.createElement('canvas');c.width=Math.round(im.width*k);c.height=Math.round(im.height*k);const x=c.getContext('2d');x.imageSmoothingQuality='high';x.drawImage(im,0,0,c.width,c.height);URL.revokeObjectURL(u);src=c;openEd()};im.onerror=()=>toast("Couldn't open that picture");im.src=u};

/* ---------- editor ---------- */
const openEd=()=>{ed={zoom:0,px:0,py:0,bri:0,con:0,style:0,inv:false,fx:0};fxReset(0);requestAnimationFrame(tick);$('#zoom').value=0;$('#bri').value=0;$('#con').value=0;$('#txt').value='';$('#inv').classList.remove('on');edSegs();$('#home').hidden=true;$('#ed').hidden=false;scrollTo(0,0);draw()};
const closeEd=()=>{$('#ed').hidden=true;$('#home').hidden=false};
const edSegs=()=>{for(const b of $('#style').children)b.classList.toggle('on',+b.dataset.v==ed.style);for(const b of $('#fxg').children)b.classList.toggle('on',+b.dataset.v==ed.fx)};
FX.forEach((f,e)=>$('#fxg').insertAdjacentHTML('beforeend','<button data-v='+e+'>'+FXI[e]+' '+f+'</button>'));
$('#fxg').onclick=e=>{const b=e.target.closest('button');if(!b)return;ed.fx=+b.dataset.v;fxReset(ed.fx);edSegs()};
// the preview plays the chosen animation until the editor closes
const tick=now=>{if($('#ed').hidden)return;if(src)paint($('#pv'),ed.fx?fxFrame(bits,ed.fx,now):bits);requestAnimationFrame(tick)};
const cover=()=>Math.max(W/src.width,H/src.height),scale=()=>cover()*Math.pow(2,ed.zoom);
let raf=0;const draw=()=>{if(!raf)raf=requestAnimationFrame(()=>{raf=0;render()})};
const render=()=>{
 const c=document.createElement('canvas');c.width=W;c.height=H;const x=c.getContext('2d');
 x.imageSmoothingQuality='high';
 const s=scale();x.setTransform(s,0,0,s,W/2-(src.width/2+ed.px)*s,H/2-(src.height/2+ed.py)*s);x.drawImage(src,0,0);
 const d=x.getImageData(0,0,W,H).data,g=new Float32Array(W*H),hist=new Uint32Array(256);
 const pad=new Uint8Array(W*H);let n=0; // pad = black bars around a zoomed-out picture
 for(let i=0;i<W*H;i++){if(d[i*4+3]<128){pad[i]=1;continue}const v=d[i*4]*.299+d[i*4+1]*.587+d[i*4+2]*.114;g[i]=v;hist[v|0]++;n++}
 let lo=0,hi=255,a=0;for(;lo<255&&(a+=hist[lo])<n*.02;lo++);a=0;for(;hi>0&&(a+=hist[hi])<n*.02;hi--);if(hi-lo<24){lo=0;hi=255}
 const cf=Math.pow(2,ed.con/50);
 for(let i=0;i<W*H;i++)g[i]=pad[i]?0:((g[i]-lo)*255/(hi-lo)-128)*cf+128+ed.bri*1.2;
 bits.fill(0);
 if(ed.style==0){ // Atkinson dithering: keeps faces clean on a tiny screen
  for(let y=0;y<H;y++)for(let x0=0;x0<W;x0++){const i=y*W+x0,o=g[i],n=o<128?0:255;bits[i]=n?1:0;const e=(o-n)/8;
   if(x0+1<W)g[i+1]+=e;if(x0+2<W)g[i+2]+=e;if(y+1<H){if(x0>0)g[i+W-1]+=e;g[i+W]+=e;if(x0+1<W)g[i+W+1]+=e}if(y+2<H)g[i+2*W]+=e}
 }else if(ed.style==1){for(let i=0;i<W*H;i++)bits[i]=g[i]>=128?1:0}
 else{for(let y=1;y<H-1;y++)for(let x0=1;x0<W-1;x0++){const i=y*W+x0,
  gx=-g[i-W-1]-2*g[i-1]-g[i+W-1]+g[i-W+1]+2*g[i+1]+g[i+W+1],gy=-g[i-W-1]-2*g[i-W]-g[i-W+1]+g[i+W-1]+2*g[i+W]+g[i+W+1];
  bits[i]=Math.hypot(gx,gy)>180&&!(pad[i-W-1]|pad[i-W+1]|pad[i+W-1]|pad[i+W+1]|pad[i-1]|pad[i+1]|pad[i-W]|pad[i+W])?1:0}}
 if(ed.inv)for(let i=0;i<W*H;i++)if(!pad[i])bits[i]^=1;
 const t=$('#txt').value.trim();
 if(t){const tc=document.createElement('canvas');tc.width=W;tc.height=H;const tx=tc.getContext('2d');let fs=13;
  do{tx.font='bold '+fs+'px system-ui,sans-serif'}while(tx.measureText(t).width>122&&--fs>7);
  tx.textAlign='center';tx.fillStyle='#fff';tx.fillText(t,W/2,H-4);
  const ta=tx.getImageData(0,0,W,H).data,m=new Uint8Array(W*H);for(let i=0;i<W*H;i++)m[i]=ta[i*4+3]>110;
  for(let y=0;y<H;y++)for(let x0=0;x0<W;x0++){const i=y*W+x0;if(m[i]){bits[i]=1;continue}
   for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){const yy=y+dy,xx=x0+dx;if(yy>=0&&yy<H&&xx>=0&&xx<W&&m[yy*W+xx])bits[i]=0}}}
 paint($('#pv'),bits)};
$('#style').onclick=e=>{const v=e.target.dataset.v;if(v!=null){ed.style=+v;edSegs();draw()}};
for(const id of['zoom','bri','con'])$('#'+id).oninput=e=>{ed[id]=+e.target.value;draw()};
$('#txt').oninput=draw;
$('#inv').onclick=()=>{ed.inv=!ed.inv;$('#inv').classList.toggle('on',ed.inv);draw()};
$('#fit').onclick=()=>{ed.zoom=Math.log2(Math.min(W/src.width,H/src.height)/cover());ed.px=ed.py=0;$('#zoom').value=ed.zoom;draw()};
$('#cancel').onclick=closeEd;
const pv=$('#pv');let drag=null;
pv.onpointerdown=e=>{drag={x:e.clientX,y:e.clientY};pv.setPointerCapture(e.pointerId)};
pv.onpointermove=e=>{if(!drag)return;const k=W/pv.clientWidth/scale();ed.px-=(e.clientX-drag.x)*k;ed.py-=(e.clientY-drag.y)*k;drag={x:e.clientX,y:e.clientY};draw()};
pv.onpointerup=pv.onpointercancel=()=>drag=null;
$('#send').onclick=async()=>{const b=$('#send');b.disabled=true;b.textContent='Sending...';
 try{const r=await (await api('/add?fx='+ed.fx,{method:'POST',headers:{'Content-Type':'text/plain'},body:b2hex(pack(bits))})).json();
  st.items=r.items;st.fx=r.fx;st.now=r.now;pics[st.items[st.now]]=Promise.resolve(bits.slice());closeEd();drawList();toast('Sent! Look at the keychain ✨')}
 catch(e){toast(e.message=='full'?'The list is full. Remove something first.':"Couldn't reach the keychain. Is the phone still on its Wi-Fi?")}
 b.disabled=false;b.textContent='Send to keychain'};
load();
</script></body></html>)=====";

// ======================= KEYCHAIN WI-FI =======================
// The keychain is its own access point, so it works anywhere with no
// router. A tiny DNS server sends every address to the page, which makes
// most phones pop it up by themselves after joining. The Arduino IDE can
// also upload to it on this network (laptop joins Keychain-Pet).

#if ENABLE_PHOTO_WIFI
namespace portal {

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updater;
DNSServer dns;
bool on = false;
unsigned long lastUse = 0;     // last time the page did something (not phone background checks)
unsigned long startedAt = 0;

int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c |= 0x20;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// "a0,p3,a2"-style list, as the page uses
String itemsJson() {
  String j = "[";
  for (int i = 0; i < play::n; i++) {
    if (i) j += ',';
    j += '"';
    j += play::kind[i] == KIND_ANIM ? 'a' : 'p';
    j += play::id[i];
    j += '"';
  }
  return j + "]";
}

// the answer to every list change: the new list and what's on screen
String fxJson() {
  String j = "[";
  for (int i = 0; i < play::n; i++) {
    if (i) j += ',';
    j += play::fx[i];
  }
  return j + "]";
}

void sendList(int now) {
  server.send(200, "application/json", String("{\"items\":") + itemsJson() + ",\"fx\":" + fxJson() + ",\"now\":" + now + "}");
}

void sendState() {
  lastUse = millis();
  String j = F("{\"name\":\"" AP_NAME "\",\"max\":");
  j += PLAY_MAX;
  j += F(",\"fs\":");
  j += play::fsOk ? 1 : 0;
  j += F(",\"sec\":");
  j += play::seconds;
  j += F(",\"now\":");
  j += play::pos;
  j += F(",\"items\":");
  j += itemsJson();
  j += F(",\"fx\":");
  j += fxJson();
  j += F(",\"anims\":[");
  for (int a = 0; a < play::ANIM_COUNT; a++) {
    if (a) j += ',';
    j += '"';
    j += play::ANIM_NAMES[a];
    j += '"';
  }
  j += F("],\"bat\":");
#if BATTERY_MONITOR
  j += String(ESP.getVcc() / 1000.0f, 2);
#else
  j += '0';
#endif
  j += '}';
  server.send(200, "application/json", j);
}

void sendPicture() {
  lastUse = millis();
  int p = server.arg("id").toInt();
  File f = play::fsOk && play::picInList(p) ? LittleFS.open(play::picPath(p), "r") : File();
  if (!f) {
    server.send(404, "text/plain", "no picture");
    return;
  }
  const char *digits = "0123456789abcdef";
  String h;
  h.reserve(PHOTO_BYTES * 2);
  for (int i = 0; i < PHOTO_BYTES; i++) {
    int b = f.read();
    h += digits[(b >> 4) & 15];
    h += digits[b & 15];
  }
  f.close();
  server.send(200, "text/plain", h);
}

// POST /add?a=N adds an animation; POST /add with a hex body adds a picture
void add() {
  lastUse = millis();
  if (play::n >= PLAY_MAX) {
    server.send(400, "text/plain", "full");
    return;
  }

  if (server.hasArg("a")) {
    int a = server.arg("a").toInt();
    if (a < 0 || a >= play::ANIM_COUNT) {
      server.send(400, "text/plain", "bad animation");
      return;
    }
    for (int i = 0; i < play::n; i++)
      if (play::kind[i] == KIND_ANIM && play::id[i] == a) {
        server.send(400, "text/plain", "already there");
        return;
      }
    play::kind[play::n] = KIND_ANIM;
    play::id[play::n] = a;
    play::fx[play::n] = FX_STILL;
  } else {
    if (!play::fsOk) {
      server.send(500, "text/plain", "no filesystem");
      return;
    }
    String body = server.arg("plain");
    bool ok = body.length() == PHOTO_BYTES * 2;
    for (unsigned i = 0; ok && i < body.length(); i++) ok = hexv(body[i]) >= 0;
    if (!ok) {
      server.send(400, "text/plain", "bad picture");
      return;
    }
    for (int i = 0; i < PHOTO_BYTES; i++) play::pic[i] = hexv(body[2 * i]) << 4 | hexv(body[2 * i + 1]);
    int p = play::newPicId();
    if (p < 0 || !play::writePic(p, play::pic)) {
      server.send(500, "text/plain", "flash full");
      return;
    }
    int e = server.arg("fx").toInt();
    play::kind[play::n] = KIND_PIC;
    play::id[play::n] = p;
    play::fx[play::n] = (e >= 0 && e < FX_COUNT) ? e : FX_STILL;
  }
  play::n++;
  play::save();
  play::jumpTo = play::n - 1;  // main loop shows the new one right away
  sendList(play::n - 1);
}

// POST /list?items=a0,p3,a2 : the whole new order (moves and removals)
void setList() {
  lastUse = millis();
  String s = server.arg("items");
  uint8_t k[PLAY_MAX], v[PLAY_MAX];
  int n = 0;
  bool ok = s.length() > 0;
  int start = 0;
  while (ok && start <= (int)s.length()) {
    int comma = s.indexOf(',', start);
    if (comma < 0) comma = s.length();
    String t = s.substring(start, comma);
    start = comma + 1;
    if (n >= PLAY_MAX || t.length() < 2) {
      ok = false;
      break;
    }
    uint8_t kk = t[0] == 'a' ? KIND_ANIM : t[0] == 'p' ? KIND_PIC
                                                       : 255;
    int vv = t.substring(1).toInt();
    if (kk == KIND_ANIM) ok = vv >= 0 && vv < play::ANIM_COUNT;
    else if (kk == KIND_PIC) ok = play::picInList(vv);  // only pictures it already has
    else ok = false;
    for (int i = 0; ok && i < n; i++) ok = !(k[i] == kk && v[i] == vv);  // no doubles
    k[n] = kk;
    v[n] = vv;
    n++;
  }
  if (!ok || n == 0) {
    server.send(400, "text/plain", "bad list");
    return;
  }

  // keep playing the same thing if it's still in the list
  int now = 0;
  for (int i = 0; i < n; i++)
    if (k[i] == play::kind[play::pos] && v[i] == play::id[play::pos]) now = i;
  bool same = n > 0 && k[now] == play::kind[play::pos] && v[now] == play::id[play::pos];

  uint8_t e[PLAY_MAX];  // each item keeps its effect when it moves
  for (int i = 0; i < n; i++) {
    e[i] = FX_STILL;
    for (int j = 0; j < play::n; j++)
      if (play::kind[j] == k[i] && play::id[j] == v[i]) e[i] = play::fx[j];
  }
  memcpy(play::kind, k, n);
  memcpy(play::id, v, n);
  memcpy(play::fx, e, n);
  play::n = n;
  play::save();
  play::cleanup();
  if (same) play::pos = now;
  else play::jumpTo = now;
  sendList(now);
}

void showNow() {
  lastUse = millis();
  int i = server.arg("i").toInt();
  if (i < 0 || i >= play::n) {
    server.send(400, "text/plain", "bad item");
    return;
  }
  play::jumpTo = i;
  server.send(200, "text/plain", "ok");
}

// POST /fx?i=N&e=M : change the animation on picture item N
void setEffect() {
  lastUse = millis();
  int i = server.arg("i").toInt(), e = server.arg("e").toInt();
  if (i < 0 || i >= play::n || play::kind[i] != KIND_PIC || e < 0 || e >= FX_COUNT) {
    server.send(400, "text/plain", "bad");
    return;
  }
  play::fx[i] = e;
  play::save();
  play::jumpTo = i;  // show it with the new animation
  server.send(200, "text/plain", "ok");
}

void settings() {
  lastUse = millis();
  if (server.hasArg("sec")) play::seconds = constrain(server.arg("sec").toInt(), 3, 255);
  play::save();
  server.send(200, "text/plain", "ok");
}

void start() {
  WiFi.forceSleepWake();
  delay(1);
  WiFi.persistent(false);  // don't wear the flash with Wi-Fi settings
  WiFi.mode(WIFI_AP);
  WiFi.setOutputPower(AP_POWER_DBM);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_NAME, AP_PASS, 6, false, 2);

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    lastUse = millis();
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/state", HTTP_GET, sendState);
  server.on("/pic", HTTP_GET, sendPicture);
  server.on("/add", HTTP_POST, add);
  server.on("/list", HTTP_POST, setList);
  server.on("/show", HTTP_POST, showNow);
  server.on("/set", HTTP_POST, settings);
  server.on("/fx", HTTP_POST, setEffect);
  updater.setup(&server, "/update", "admin", OTA_PASSWORD);
  server.onNotFound([]() {  // phone "is there internet?" checks land here
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

#if ENABLE_OTA
  // Arduino IDE upload on this network too (laptop joins AP_NAME)
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() { lastUse = millis(); otaScreen("Updating...", NULL, 0); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    otaScreen("Updating...", NULL, total ? (done * 100 / total) : 0);
  });
  ArduinoOTA.onEnd([]()   { otaScreen("Done!", "restarting", 100); });
  ArduinoOTA.onError([](ota_error_t e) { otaScreen("Update failed", "switch off/on", -1); });
  ArduinoOTA.begin();
#endif

  on = true;
  lastUse = startedAt = millis();
}

void stop() {
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
  on = false;
}

void handle() {
  if (!on) return;
  dns.processNextRequest();
  server.handleClient();
#if ENABLE_OTA
  ArduinoOTA.handle();
#endif
    unsigned long limit = lastUse == startedAt ? AP_BOOT_SECONDS * 1000UL     // nobody came
                                             : AP_IDLE_MINUTES * 60000UL;   // page was used
  if (millis() - lastUse > limit) stop();
}

}  // namespace portal
#endif

// =========================== MAIN ===========================
int scene = SCENE_TAIL;
unsigned long sceneStart = 0, lastFrame = 0;

void drawBatteryIcon() {
#if BATTERY_MONITOR
  static float vbat = 4.0f;
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000 || lastRead == 0) {
    lastRead = millis();
    vbat = ESP.getVcc() / 1000.0f;
  }
  if (vbat < LOW_VCC && (millis() / 500) % 2) {
    int x = display.width() - 14, y = 1;
    display.drawRect(x, y, 11, 6, SSD1306_WHITE);
    display.fillRect(x + 11, y + 2, 2, 2, SSD1306_WHITE);
    display.fillRect(x + 2, y + 2, 2, 2, SSD1306_WHITE);
  }
#endif
}

// little Wi-Fi sign top-left while the phone page can be reached
static const char WIFI_ICON[] PROGMEM =
  ".#######."
  "#.......#"
  "..#####.."
  ".#.....#."
  "...###..."
  "........."
  "....#....";

void drawWifiIcon() {
#if ENABLE_PHOTO_WIFI
  if (!portal::on) return;
  display.fillRect(0, 0, 11, 9, SSD1306_BLACK);
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 9; c++)
      if (pgm_read_byte(&WIFI_ICON[r * 9 + c]) == '#') display.drawPixel(1 + c, 1 + r, SSD1306_WHITE);
#endif
}

void splash() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.print(F("Send me a picture!"));
  display.drawFastHLine(4, 12, 120, SSD1306_WHITE);
  display.setCursor(4, 18);
  display.print(F("WiFi " AP_NAME));
  display.setCursor(4, 30);
  display.print(F("Pass " AP_PASS));
  display.setCursor(4, 44);
  display.print(F("then open"));
  display.setCursor(4, 54);
  display.print(F("192.168.4.1"));
  display.display();
}

// start playlist item i (an animation or a picture)
void startItem(int i) {
  if (play::n == 0) play::setDefault();
  if (i < 0 || i >= play::n) i = 0;
  play::pos = i;
  sceneStart = millis();
  if (play::kind[i] == KIND_PIC) {
    scene = SCENE_PHOTO;
    play::loadPic(play::id[i]);
    play::picShownAt = millis();
    play::fxReset(play::fx[i]);
    return;
  }
  scene = play::id[i];
  if (scene == SCENE_TAIL) mermaid::reset();
  if (scene == SCENE_SHELL) shell::reset();
  if (scene == SCENE_BEACH) beach::reset();
  if (scene == SCENE_STARRY) starry::reset();
    if (scene == SCENE_EYES)   eyes::reset();
  if (scene == SCENE_TEA)    tea::reset();
  if (scene == SCENE_DRAGON) dragon::reset();
  if (scene == SCENE_ROSES)  roses::reset();
}

// ---------------------- Wi-Fi update mode ----------------------
#if ENABLE_OTA
bool otaMode = false;

bool startOTA() {
  otaScreen("WiFi update mode", "connecting...", -1);
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 20000) {
      WiFi.mode(WIFI_OFF);
      WiFi.forceSleepBegin();
      delay(1);
      otaScreen("WiFi failed", "running normally", -1);
      delay(1500);
      return false;
    }
    otaScreen("WiFi update mode", "connecting...", ((millis() - t0) / 100) % 100);
    delay(120);
  }

  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    otaScreen("Updating...", NULL, 0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    otaScreen("Updating...", NULL, total ? (done * 100 / total) : 0);
  });
  ArduinoOTA.onEnd([]() {
    otaScreen("Done!", "restarting", 100);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    otaScreen("Update failed", "switch off/on", -1);
  });
  ArduinoOTA.begin();

  otaScreen("Ready for upload", WiFi.localIP().toString().c_str(), -1);
  return true;
}
#endif

// ---------------------------------------------------------------
void setup() {
  WiFi.mode(WIFI_OFF);  // Wi-Fi off saves ~50 mA
  WiFi.forceSleepBegin();
  delay(1);

  pinMode(OTA_PIN, INPUT_PULLUP);

#if ENABLE_OTA
  bool wantOTA = (digitalRead(OTA_PIN) == LOW);  // wire from GPIO14 to GND
#endif

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(700000);  // faster screen updates = smoother animation

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) delay(1000);  // OLED not found: check wiring / address
  }
  display.setRotation(ROTATION);
  display.clearDisplay();
  display.display();

#if ENABLE_OTA
  if (wantOTA) {
    otaMode = startOTA();
    if (otaMode) return;
  }
#endif

  randomSeed(RANDOM_REG32);
  play::load();

#if ENABLE_PHOTO_WIFI
  portal::start();
  splash();
  unsigned long s0 = millis();
  while (millis() - s0 < SPLASH_MS) {
    portal::handle();
    delay(2);
  }
#endif

  startItem(0);
}

void loop() {
#if ENABLE_OTA
  if (otaMode) {
    ArduinoOTA.handle();
    delay(5);
    return;
  }
#endif

#if ENABLE_PHOTO_WIFI
  portal::handle();
#endif

  unsigned long now = millis();
  if (now - lastFrame < 22) {
    yield();
    return;
  }  // up to ~45 fps
  lastFrame = now;

  // the phone asked for something to be shown now
  if (play::jumpTo >= 0) {
    int j = play::jumpTo;
    play::jumpTo = -1;
    startItem(j);
  }

  // next item when this one has had its turn (a single item just stays)
  if (play::n > 1 && now - sceneStart > play::seconds * 1000UL) startItem((play::pos + 1) % play::n);

  if (scene == SCENE_TAIL) mermaid::frame(now);
  if (scene == SCENE_SHELL) shell::frame(now);
  if (scene == SCENE_BEACH) beach::frame(now);
  if (scene == SCENE_STARRY) starry::frame(now);
    if (scene == SCENE_EYES)   eyes::frame(now);
  if (scene == SCENE_TEA)    tea::frame(now);
  if (scene == SCENE_DRAGON) dragon::frame(now);
  if (scene == SCENE_ROSES)  roses::frame(now);
  if (scene == SCENE_PHOTO) play::photoFrame(now);

  drawWifiIcon();
  drawBatteryIcon();
  display.display();
}
