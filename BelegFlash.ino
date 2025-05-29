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
// - mehrere verschiedene Kakteen als neue Bilder, vielleicht auch mehrere Kakteen auf einmal / zusätzlich Vogel OK, aber möglicherweise mehrere Obstacles gleichzeitig auf Bildschirm
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
  int padding;
};

DinoSprite dino;
bool wasDucking = false;
#define DinoDuckYOffset 8;  // Offset für geduckten Dino
int8_t Dinopadding = 4;

enum SpriteType {
  SPRITE_CACTUS,
  SPRITE_CACTUS2,
  SPRITE_CACTUS3,
  SPRITE_CACTUS4,
  SPRITE_BIRD,
  SPRITE_CLOUD
};

struct Sprite {
  SpriteType type;
  int x, y;
  int width, height;
  int dx;
  int frame;
};

bool newObstacle = true; // Flag ob neues Obstacles random ausgewählt werden soll
Sprite* obstacle; // hält Zeiger auf aktuelles Obstacle

Sprite cloud;
Sprite cactus;
Sprite cactus2;
Sprite cactus3;
Sprite cactus4;
Sprite bird;

// Array mit möglichen Obstacles
Sprite* validObstacles[5] = {&cactus, &cactus2, &cactus3, &cactus4, &bird};
const int validObstaclesLength = sizeof(validObstacles) / sizeof(validObstacles[0]);

#define HLLineY 111

#define ScoreX 5
#define ScoreY 5

bool DarkMode = true;

int fps = 0;
unsigned long framecount = 0;
unsigned long lastFramecount = 0;
unsigned long lastFPSUpdate = 0;

int targetFrameTime = 1000 / 20;  // Ziel Frametime in ms, wird reduziert durch Zeichenaufwand
bool animate = false; // Flag ob animiert und score aktualisiert wird, gesetzt jeden anderen Frame

double gameSpeed = 0.0;

#define EEPROMHighscore 0  // Highscore permant in EEPROM speichern
int highscore = 0;
int score = 0;

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
  dino.padding = 4;
}

void initCloud() {
  cloud.type = SPRITE_BIRD;
  cloud.x = tft.width() + random(-100, 70);
  cloud.y = 30;
  cloud.width = 46;
  cloud.height = 13;
  cloud.dx = 2;
  cloud.frame = 0;
}

void initCactus() {
  cactus.type = SPRITE_CACTUS;
  cactus.width = 23;
  cactus.height = 48;
  cactus.x = tft.width() + random(0, 60);
  cactus.y = 80;
  cactus.dx = 5;
  cactus.frame = 0;
}

void initCactus2() {
  cactus2.type = SPRITE_CACTUS2;
  cactus2.width = 27;
  cactus2.height = 46;
  cactus2.x = tft.width() + random(0, 60);
  cactus2.y = 82;
  cactus2.dx = 5;
  cactus2.frame = 0;
}

void initCactus3() {
  cactus3.type = SPRITE_CACTUS3;
  cactus3.width = 23;
  cactus3.height = 48;
  cactus3.x = tft.width() + random(0, 60);
  cactus3.y = 80;
  cactus3.dx = 5;
  cactus3.frame = 0;
}

void initCactus4() {
  cactus4.type = SPRITE_CACTUS4;
  cactus4.width = 15;
  cactus4.height = 32;
  cactus4.x = tft.width() + random(0, 60);
  cactus4.y = 90;
  cactus4.dx = 5;
  cactus4.frame = 0;
}

void initBird() {
  bird.type = SPRITE_BIRD;
  bird.width = 42;
  bird.height = 36;
  bird.x = tft.width() + random(0, 60);
  int randompos = random(0,2);
  if (randompos == 0){
    bird.y = 60;
  }
  else{
    bird.y = 80;
  }
  
  bird.dx = 6;
  bird.frame = 0;
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
  gameSpeed = 0.0;

  fps = 0;
  framecount = 0;
  lastFramecount = 0;
  lastFPSUpdate = 0;

  jumpProgress = 0;

  wasDucking = false;

  newObstacle = true;
  dead = 0;
  animate = true;

  initDino();
  initCactus();
  initCactus2();
  initCactus3();
  initCactus4();
  initBird();
  initCloud();
}

// https://kishimotostudios.com/articles/aabb_collision/
bool checkAABBCollision(int AX, int AY, int AWidth, int AHeight, int BX, int BY, int BWidth, int BHeight) {
  int ALeft = AX;
  int ARight = AX + AWidth;
  int ATop = AY;
  int ABottom = AY + AHeight;

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
  tft.setCursor(ScoreX, ScoreY);
  tft.print(score);
}

void drawGround() {
  tft.drawFastHLine(0, HLLineY, tft.width(), tft.color565(50, 50, 50));  // horizontale Linie
}

void updateCloud() {
  tft.fillRect(cloud.x, cloud.y, cloud.width, cloud.height, ST7735_WHITE);
  cloud.x -= cloud.dx;
  if (cloud.x + cloud.width < 0) {
    cloud.x = tft.width() + random(30, 70);
  }
  tft.drawBitmap(cloud.x, cloud.y, epd_bitmap_cloud, cloud.width, cloud.height, tft.color565(150, 150, 150));
}

void updateSprite(Sprite &c) {
  tft.fillRect(c.x, c.y, c.width, c.height, ST7735_WHITE); // löschen

  // Neue Kaktusposition
  c.x -= c.dx + (int)gameSpeed;
  if (c.x <= -c.width) {
    newObstacle = true;
    c.x = tft.width() + random(20, 100);
  }
}

void drawObstacle(){
  switch (obstacle->type){
    case SPRITE_CACTUS: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS2: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus2, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS3: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus3, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS4: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus4, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_BIRD: {
      if (obstacle->frame == 0){
        if (animate == true){
          obstacle->frame = 1;
        }
        tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_bird, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
      }
      else if (obstacle->frame == 1){
        if (animate == true){
          obstacle->frame = 0;
        }
         tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_bird2, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
      }
    }
  }
}

int drawDino() {
  // Dino animieren
  
  if (dead != 1) {
    if (dino.frame == 0) {
      if (animate == true){ // nur neuen Frame setzen bei gewollter Animierung
        dino.frame = 1;
        animate = false;
      }

      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
      } 
      else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino, dino.width, dino.height, tft.color565(50, 50, 50));
      }
    }
    else if (dino.frame == 1) { // nur neuen Frame setzen bei gewollter Animierung
      if (animate == true){
        dino.frame = 0;
        animate = false;
      }
      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck2, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
      } 
      else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino2, dino.width, dino.height, tft.color565(50, 50, 50));
      }
    }
    return 0;
  } 
  else {  // tot
    if (dino.ducking == true) {
      dino.y -= DinoDuckYOffset;
    }
    drawObstacle();
    drawHitboxes();

    tft.drawBitmap(dino.x, dino.y, epd_bitmap_dead, dino.width, dino.height, ST7735_RED);
    
    return 1;
  }
}

void drawHitboxes(){
  tft.drawRect(obstacle->x, obstacle->y, obstacle->width, obstacle->height, ST7735_RED);  // Obstacle
  tft.drawRect(dino.x + dino.padding, dino.y + dino.padding, dino.width - 2 * dino.padding, dino.height - 2 * dino.padding, ST7735_BLUE); // Dino
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
      if (checkAABBCollision(dino.x + dino.padding, dino.y + dino.padding, dino.duckWidth - 2 * dino.padding, dino.duckHeight - 2 * dino.padding, obstacle->x, obstacle->y, obstacle->width, obstacle->height)) {
        Serial.println("Collison?");
        dead = 1;
      }
  }
  else {
    if (checkAABBCollision(dino.x + dino.padding, dino.y + dino.padding, dino.width - 2 * dino.padding, dino.height - 2 * dino.padding, obstacle->x, obstacle->y, obstacle->width, obstacle->height)) {
      Serial.println("Collison?");
      dead = 1;
    }
  }
}

int drawFrame() {
  checkDuck();

  drawScore();
  updateCloud();

  if (newObstacle == true){ // neues Obstacles setzen
    randomSeed((int)millis());
    int randomObs = random(0, validObstaclesLength);
    obstacle = validObstacles[randomObs];

    switch (obstacle->type){ // initialiseren des Objekts
      case SPRITE_CACTUS: initCactus(); break;
      case SPRITE_CACTUS2: initCactus2(); break;
      case SPRITE_CACTUS3: initCactus3(); break;
      case SPRITE_BIRD: initBird(); break;
    }
    newObstacle = false;
  }
  updateSprite(*obstacle);
  drawObstacle();

  //updateCactus3();
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
    fps = framecount - lastFramecount;
    lastFramecount = framecount;
    lastFPSUpdate = now;

    tft.fillRect(tft.width() - 40, 5, 40, 10, ST7735_WHITE);
    tft.setCursor(tft.width() - 40, 5);
    tft.print(String("FPS:") + fps);
    Serial.println(String("FPS: ") + fps);
  }
  if (gameSpeed <= 5){
    gameSpeed += 0.0025; // langsames erhöhen der Geschwindigkeit
  }
  framecount++;
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
    unsigned long frameEnd = millis();
    unsigned long frameTime = frameEnd - frameStart;

    // DeltaTime
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

    // animieren jeder anderen Frames
    if (framecount % 2 == 0){
      animate = true;
      score++;
    }
  }
}
