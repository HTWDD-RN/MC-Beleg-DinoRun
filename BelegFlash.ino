//#include <Adafruit_GFX.h>
//#include <Adafruit_ST7735.h>
// https://github.com/andrey-belokon/PDQ_GFX_Libs
#include <PDQ_GFX.h>
#include "PDQ_ST7735_config.h"
#include <PDQ_ST7735.h>
#include "Graphics.h"
#include <SPI.h>
#include <EEPROM.h>
//#include <SdFat.h>

// TODO:
// - bessere Sprungberechnung mit Parabel (Gravity und Sprungzeit)
// - Schauen wie lange ein Frame zum Zeichnen braucht und die delay entsprechend anpassen (Delta Time) -> Abziehen der Zeit zum Zeichnen/Logik OK
// - mehrere verschiedene Kakteen als neue Bilder, vielleicht auch mehrere Kakteen auf einmal / zusätzlich Vogel -> als Array?
// - Geschwindigkeit langsam erhöhen bei Score Meilensteinen (alle 100 oder mehr)
// - Grafik für Boden -> Pixel als Steine
// - vielleicht auch noch zweite Taste zum Ducken hinzufügen, wenn Vogel kommt OK
// - Titelbildschirm
// - GameOver Bildschirm OK, aber vielleicht erst nach Tastendruck fortsetzen?
// - Sound über Beeper
// - Landscape Orientation für das Display? OK
// - HighScore auf SDCard speichern? OK aber auf EEPROM, vielleicht später mal auf SDCard
// - wir haben Stand jetzt nur noch ~3.5K Flash Speicher!! -> nutzen jetzt Flash OK
// - -> Bilder müssen deshalb von SDCard geladen werden -> wie bekommt man das Blinken beim Zugriff auf SDCard weg??

// Pinbelegung für das TFT-Display
/*
#define TFT_CS     10
#define TFT_DC     9
#define TFT_RST    8
#define SD_CS      4  

VCC: 5V, LED: 3,3V!!
SDA: 11, SCK: 13 (für TFT/SD)
SD MISO : 12
SD MOSI: SDA (11)
SD CLK: SCK (13)
*/

#define JMP_BUTTON_PIN 2
#define DUCK_BUTTON_PIN 3

PDQ_ST7735 tft;  // Pins sind im Header

struct DinoSprite {
  int x;
  int y;
  int yStart;
  int width;
  int height;
  int duckWidth;
  int duckHeight;
  bool jumping;
  bool ducking;
  int frame;
};

DinoSprite dino;
bool wasDucking = false;
#define DinoDuckYOffset 8;  // Offset für geduckten Dino

enum SpriteType {
  SPRITE_CACTUS,
  SPRITE_CACTUS2,
  SPRITE_BIRD,
  SPRITE_CLOUD
};

struct Sprite {
  SpriteType type;
  int x, y;
  int width, height;
  int dx;
};

Sprite cloud;
Sprite cactus;
Sprite cactus2;
Sprite bird;

#define HLLineY 111

#define ScoreX 5
#define ScoreY 5

bool DarkMode = true;

int targetFrameTime = 1000 / 20;  // Ziel Frametime in ms, wird reduziert durch Zeichenaufwand

#define EEPROMHighscore 0  // Highscore permant in EEPROM speichern
int highscore = 0;
int score = 0;
// Score ist auch Framecounter
int lastScore = 0;
int fps = 0;
unsigned long lastFPSUpdate = 0;

const int jumpHeights[] = {
  -13, -10, -7, -5, -4, -3, -2, -1,
  0,
  1, 2, 3, 4, 5, 7, 10, 13
};
const int jumpLength = sizeof(jumpHeights) / sizeof(jumpHeights[0]);  // Länge = ArrayLänge in Byte / Größe ArrayElement
int jumpProgress = 0;

int dead = 0;

void jumpButtonFunc() {
  dino.jumping = true;
  //jump = true;
}

void setup() {
  pinMode(JMP_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(JMP_BUTTON_PIN), jumpButtonFunc, FALLING);
  pinMode(DUCK_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);

  #ifdef ST7735_RST_PIN  // Reset wie Adafruit
  FastPin<ST7735_RST_PIN>::setOutput();
  FastPin<ST7735_RST_PIN>::hi();
  FastPin<ST7735_RST_PIN>::lo();
  delay(1);
  FastPin<ST7735_RST_PIN>::hi();
  #endif

  // Initialisiere das TFT-Display
  //tft.initR(INITR_BLACKTAB);
  tft.begin();
  tft.invertDisplay(DarkMode);  // "DarkMode"

  tft.setRotation(1);
  tft.fillScreen(ST7735_WHITE);
  tft.setTextColor(ST7735_BLACK);

  //initSD();
  Serial.print("");
  delay(1000);

  reset();
}

void initDino() {
  dino.x = 5;
  dino.yStart = 90;
  dino.y = dino.yStart;
  dino.width = 20;
  dino.height = 21;
  dino.duckWidth = 28;
  dino.duckHeight = 13;
  dino.ducking = false;
  dino.jumping = false;
  dino.frame = 0;
}

void initCloud() {
  cloud.type = SPRITE_BIRD;
  cloud.x = tft.width() + random(-100, 70);
  cloud.y = 30;
  cloud.width = 46;
  cloud.height = 13;
  cloud.dx = 2;
}

void initCactus() {
  cactus.type = SPRITE_CACTUS;
  cactus.width = 23;
  cactus.height = 48;
  cactus.x = tft.width() + random(0, 60);
  cactus.y = 80;
  cactus.dx = 5;
}

void initCactus2() {
  cactus2.type = SPRITE_CACTUS2;
  cactus2.width = 23;
  cactus2.height = 48;
  cactus2.x = tft.width() + random(0, 60);
  cactus2.y = 80;
  cactus2.dx = 5;
}

void initBird() {
  bird.type = SPRITE_BIRD;
  bird.width = 42;
  bird.height = 26;
  bird.x = tft.width() + random(100, 200);
  bird.y = random(60, 85);
  bird.dx = 6;
}

void reset() {
  tft.fillScreen(ST7735_WHITE);

  tft.setCursor(55, 5);
  tft.print("HI:");

  #if EEPROMHighscore == 1
  EEPROM.get(0, highscore);
  #endif
  tft.print(highscore);

  score = 0;
  lastScore = 0;
  fps = 0;
  lastFPSUpdate = 0;

  jumpProgress = 0;

  wasDucking = false;

  dead = 0;

  initDino();
  initCactus();
  initCloud();
}

// https://kishimotostudios.com/articles/aabb_collision/
bool checkAABBCollision(int AX, int AY, int AWidth, int AHeight, int BX, int BY, int BWidth, int BHeight) {
  int8_t padding = 4;

  int ALeft = AX + padding;
  int ARight = AX + AWidth - padding;
  int ATop = AY + padding;
  int ABottom = AY + AHeight - padding;

  int BLeft = BX;
  int BRight = BX + BWidth;
  int BTop = BY;
  int BBottom = BY + BHeight;

  bool AisToTheRightOfB = ALeft > BRight;
  bool AisToTheLeftOfB = ARight < BLeft;
  bool AisAboveB = ABottom < BTop;
  bool AisBelowB = ATop > BBottom;

  return !(AisToTheRightOfB || AisToTheLeftOfB || AisAboveB || AisBelowB);
}

void checkDuck() {
  // Ducken -> Offset beim Umschalten setzen/entfernen
  bool isPressed = digitalRead(DUCK_BUTTON_PIN) == LOW;
  if (isPressed == true && wasDucking == false && dino.jumping == false) {  // ducken nicht erlaubt beim Springen
    // starten des ducken
    tft.fillRect(dino.x, dino.y, dino.width, dino.height, ST7735_WHITE);  // Normaler Dino
    dino.y += DinoDuckYOffset;
    wasDucking = true;
    dino.ducking = true;

  } else if (isPressed == false && wasDucking == true) {
    // aufhören zu ducken
    tft.fillRect(dino.x, dino.y, dino.duckWidth, dino.duckHeight, ST7735_WHITE);  // Duck Dino
    dino.y -= DinoDuckYOffset;
    wasDucking = false;
    dino.ducking = false;
  }
}

void drawScore() {
  tft.fillRect(ScoreX, ScoreY, 30, 10, ST7735_WHITE);  // Score löschen
  score++;
  tft.setCursor(ScoreX, ScoreY);
  tft.print(score);
}

void drawGround() {
  tft.drawFastHLine(0, HLLineY, tft.width(), tft.color565(50, 50, 50));  // horizontale Linie
}

void updateCactus() {
  tft.fillRect(cactus.x, cactus.y, cactus.width, cactus.height, ST7735_WHITE);  // Kaktus löschen
  // Neue Kaktusposition
  cactus.x -= cactus.dx;
  if (cactus.x <= -cactus.width) {
    cactus.x = tft.width() + random(20, 100);
  }
  tft.drawBitmap(cactus.x, cactus.y, epd_bitmap_cactus, cactus.width, cactus.height, tft.color565(50, 50, 50));
}

void updateCloud() {
  tft.fillRect(cloud.x, cloud.y, cloud.width, cloud.height, ST7735_WHITE);
  cloud.x -= cloud.dx;
  if (cloud.x + cloud.width < 0) {
    cloud.x = tft.width() + random(30, 70);
  }
  tft.drawBitmap(cloud.x, cloud.y, epd_bitmap_cloud, cloud.width, cloud.height, tft.color565(150, 150, 150));
}

int drawDino() {
  // Dino animieren
  if (dead != 1) {
    if (dino.frame == 0) {
      dino.frame = 1;
      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoDuckWidth - 2 * 4, DinoDuckHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      } 
      else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino, dino.width, dino.height, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
    } 
    else if (dino.frame == 1) {
      dino.frame = 0;
      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck2, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoDuckWidth - 2 * 4, DinoDuckHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      } 
      else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino2, dino.width, dino.height, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
    }
    return 0;
  } 
  else {  // tot
    if (dino.ducking == true) {
      dino.y -= DinoDuckYOffset;
    }
    tft.drawBitmap(cactus.x, cactus.y, epd_bitmap_cactus, cactus.width, cactus.height, tft.color565(50, 50, 50));
    tft.drawRect(cactus.x, cactus.y, cactus.width, cactus.height, ST7735_RED);  // Hitbox

    tft.drawBitmap(dino.x, dino.y, epd_bitmap_dead, dino.width, dino.height, ST7735_RED);
    tft.drawRect(dino.x + 4, dino.y + 4, dino.width - 2 * 4, dino.height - 2 * 4, ST7735_BLUE);
    return 1;
  }
}

void deleteDino() {
  // Dino Löschen
  if (dino.ducking == true) {
    tft.fillRect(dino.x, dino.y, dino.duckWidth, dino.duckHeight, ST7735_WHITE);  // Duck Dino
    dino.jumping = false;
  } 
  else {
    tft.fillRect(dino.x, dino.y, dino.width, dino.height, ST7735_WHITE);  // Normaler Dino
  }
}

void calcJump() {
  // Todo: Sprungpos. dynamisch berechnen
  if (dino.jumping == true && dino.ducking == false) {
    dino.y += jumpHeights[jumpProgress];
    jumpProgress++;

    if (jumpProgress >= jumpLength) {
      dino.jumping = false;
      jumpProgress = 0;
    }
  }
}

void checkDinoCollision() {
  // Kollision?
  if (dino.ducking == true) {
    if (checkAABBCollision(dino.x, dino.y, dino.duckWidth, dino.duckHeight, cactus.x, cactus.y, cactus.width, cactus.height)) {
      Serial.println("Collison?");
      dead = 1;
    }
  } 
  else {
    if (checkAABBCollision(dino.x, dino.y, dino.width, dino.height, cactus.x, cactus.y, cactus.width, cactus.height)) {
      Serial.println("Collison?");
      dead = 1;
    }
  }
}

int drawFrame() {
  checkDuck();

  drawScore();
  updateCloud();
  updateCactus();
  drawGround();

  deleteDino();
  calcJump();
  checkDinoCollision();

  int status = drawDino();
  return status;
}

void loop() {
  unsigned long now = millis();
  if (now - lastFPSUpdate >= 1000) {  // FPS jede Sek anzeigen
    fps = score - lastScore;
    lastScore = score;
    lastFPSUpdate = now;

    tft.fillRect(tft.width() - 40, 5, 40, 10, ST7735_WHITE);
    tft.setCursor(tft.width() - 40, 5);
    tft.print(String("FPS:") + fps);
    Serial.println(String("FPS: ") + fps);
  }

  unsigned long frameStart = millis();

  int status = drawFrame();
  if (status == 1) {  // tot -> GameOver
    tft.setTextSize(2);
    tft.setTextColor(ST7735_RED);
    tft.fillRect(25, 47, 112, 20, ST7735_WHITE);
    tft.drawRect(25, 47, 112, 20, ST7735_RED);
    tft.setCursor(28, 50);
    tft.print("GAME OVER");
    tft.setTextColor(ST7735_BLACK);
    tft.setTextSize(1);

    #if EEPROMHighscore == 1
    // neuer Highscore?
    Serial.println(String("EEPROM val: ") + EEPROM.get(0, highscore));
    EEPROM.get(0, highscore);
    if (score > highscore) {
      EEPROM.put(0, score);
    }
    #else
    if (score > highscore) {
      highscore = score;
    }
    #endif

    delay(2000);
    reset();
  } else if (status == 0) {  // geht weiter
    // DeltaTime
    unsigned long frameEnd = millis();
    unsigned long frameTime = frameEnd - frameStart;

    if (targetFrameTime - (int)frameTime <= 0) {  // FPS drop! -> mehr Zeit zum Zeichnen benötigt als TargetFrameTime
      delay(0);
    } 
    else {
      delay(targetFrameTime - round(frameTime));
    }

    tft.fillRect(tft.width() - 29, 17, 35, 10, ST7735_WHITE);
    tft.setCursor(tft.width() - 29, 17);
    tft.print(frameTime);
    tft.print("ms");
  }
}
