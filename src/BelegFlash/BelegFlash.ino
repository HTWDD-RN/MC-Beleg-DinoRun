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
#define BUZZER_PIN 5

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

bool newObstacle = true;  // Flag ob neues Obstacles random ausgewählt werden soll
Sprite* obstacle;         // hält Zeiger auf aktuelles Obstacle

Sprite cloud;
Sprite cactus;
Sprite cactus2;
Sprite cactus3;
Sprite cactus4;
Sprite bird;

// Array mit möglichen Obstacles
Sprite* validObstacles[5] = { &cactus, &cactus2, &cactus3, &cactus4, &bird };
const int validObstaclesLength = sizeof(validObstacles) / sizeof(validObstacles[0]);

#define HLLineY 111

#define ScoreX 5
#define ScoreY 5

bool DarkMode = false;

int fps = 0;
unsigned long framecount = 0;
unsigned long lastFramecount = 0;
unsigned long lastFPSUpdate = 0;

int targetFrameTime = 1000 / 31;  // Ziel Frametime in ms, wird reduziert durch Zeichenaufwand
bool animate = false;             // Flag ob animiert und score aktualisiert wird, gesetzt jeden 3. Frame

bool game_start_flag = true;

#define EEPROMHighscore 0  // Highscore permant in EEPROM speichern
int highscore = 0;
int score = 0;
const int MILESTONE = 100;

int lastScore = 0;
double gameSpeed = 0.0;

int dead = 0;

// initalize variables for the jump
const int JUMP_TOP_COORDINATE = 45;
int unsigned long jump_duration = 800; 
float jump_progress = 0.0;
bool jumping_flag = false;

// defines different melodies
int loss_melody[] = { 330, 311, 294, 277, 262, 247 };
int loss_melody_durations[] = { 125, 125, 125, 125, 1000, 500 };
int loss_melody_length = sizeof(loss_melody) / sizeof(loss_melody[0]);

int milestone_melody[] = { 349, 370, 392, 415 };
int milestone_melody_durations[] = { 125, 125, 125, 250 };
int milestone_melody_length = sizeof(milestone_melody) / sizeof(milestone_melody[0]);

int start_melody[] = { 392, 440, 587, 784 };
int start_melody_durations[] = { 200, 200, 200, 300, 600 };
int start_melody_length = sizeof(start_melody) / sizeof(start_melody[0]);

int jump_melody[] = {660, 880};
int jump_melody_durations[] = {20 , 20};
int jump_melody_length = sizeof(jump_melody) / sizeof(jump_melody[0]);

// init the variables that ensure that the jump melody is played only once during a jump
int jump_melody_aggregate_duration = 0;
long unsigned start_jump_melody_timestamp = 0;

void intializeJumpMelodyAggregateDuration() {
  for (int i = 0; i<jump_melody_length; i++) {
    jump_melody_aggregate_duration += jump_melody_durations[i];
  }
}

// intializes melody variables
int* current_melody;
int* current_melody_durations;
int current_melody_length = 0;
int current_note_index = 0;

bool melody_is_playing_flag = false;
bool jump_melody_flag = false;

unsigned long game_over_time = 0;
unsigned long prev_time = 0;
unsigned long jump_time_time = 0;

// timestamp + flag for the display of the highscore
long unsigned blink_intervall = 200;
long unsigned blink_timestamp = 0;
bool is_blinking = false;

// timestamp when jump started
long unsigned start_jump_timestamp = 0;

void jumpButtonFunc() {
  // Jump PIN is used to start the game as well 
  if (!game_start_flag) {
    dino.jumping = true;
  }
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

  Serial.print("");
  delay(1000);

  //init requirements for jump sound
  intializeJumpMelodyAggregateDuration();

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
  cactus.dx = 3;
  cactus.frame = 0;
}

void initCactus2() {
  cactus2.type = SPRITE_CACTUS2;
  cactus2.width = 27;
  cactus2.height = 46;
  cactus2.x = tft.width() + random(0, 60);
  cactus2.y = 82;
  cactus2.dx = 3;
  cactus2.frame = 0;
}

void initCactus3() {
  cactus3.type = SPRITE_CACTUS3;
  cactus3.width = 23;
  cactus3.height = 48;
  cactus3.x = tft.width() + random(0, 60);
  cactus3.y = 80;
  cactus3.dx = 3;
  cactus3.frame = 0;
}

void initCactus4() {
  cactus4.type = SPRITE_CACTUS4;
  cactus4.width = 15;
  cactus4.height = 32;
  cactus4.x = tft.width() + random(0, 60);
  cactus4.y = 90;
  cactus4.dx = 3;
  cactus4.frame = 0;
}

void initBird() {
  bird.type = SPRITE_BIRD;
  bird.width = 42;
  bird.height = 36;
  bird.x = tft.width() + random(0, 60);
  int randompos = random(0, 2);
  if (randompos == 0) {
    bird.y = 60;
  } else {
    bird.y = 80;
  }

  bird.dx = 4;
  bird.frame = 0;
}

void reset() {
  tft.setTextColor(ST7735_BLACK);
  tft.fillScreen(ST7735_WHITE);
  tft.setCursor(55, 5);
  tft.print("HI:");

#if EEPROMHighscore == 1
  EEPROM.get(0, highscore);
#endif
  tft.print(highscore);

  score = 0;
  gameSpeed = 0.0;
  lastScore = 0;

  fps = 0;
  framecount = 0;
  lastFramecount = 0;
  lastFPSUpdate = 0;


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

void updateSprite(Sprite& c) {
  tft.fillRect(c.x, c.y, c.width, c.height, ST7735_WHITE);  // löschen

  // Neue Kaktusposition
  c.x -= c.dx + (int)gameSpeed;
  if (c.x <= -c.width) {
    newObstacle = true;
    c.x = tft.width() + random(20, 100);
  }
}

void drawObstacle() {
  switch (obstacle->type) {
    case SPRITE_CACTUS: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS2: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus2, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS3: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus3, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_CACTUS4: tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_cactus4, obstacle->width, obstacle->height, tft.color565(50, 50, 50)); break;
    case SPRITE_BIRD:
      {
        if (obstacle->frame == 0) {
          if (animate == true) {
            obstacle->frame = 1;
          }
          tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_bird, obstacle->width, obstacle->height, tft.color565(50, 50, 50));
          break;
        } else if (obstacle->frame == 1) {
          if (animate == true) {
            obstacle->frame = 0;
          }
          tft.drawBitmap(obstacle->x, obstacle->y, epd_bitmap_bird2, obstacle->width, obstacle->height, tft.color565(50, 50, 50));
          break;
        }
      }
  }
}

int drawDino() {
  // Dino animieren

  if (dead != 1) {
    if (dino.frame == 0) {
      if (animate == true) {  // nur neuen Frame setzen bei gewollter Animierung
        dino.frame = 1;
        animate = false;
      }

      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
      } else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino, dino.width, dino.height, tft.color565(50, 50, 50));
      }
    } else if (dino.frame == 1) {  // nur neuen Frame setzen bei gewollter Animierung
      if (animate == true) {
        dino.frame = 0;
        animate = false;
      }
      if (dino.ducking == true) {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_duck2, dino.duckWidth, dino.duckHeight, tft.color565(50, 50, 50));
      } else {
        tft.drawBitmap(dino.x, dino.y, epd_bitmap_dino2, dino.width, dino.height, tft.color565(50, 50, 50));
      }
    }
    return 0;
  } else {  // tot
    if (dino.ducking == true) {
      dino.y -= DinoDuckYOffset;
    }

    tft.fillRect(obstacle->x, obstacle->y, obstacle->width, obstacle->height, ST7735_WHITE);  // löschen
    drawGround();
    drawObstacle();
    drawHitboxes();

    if (DarkMode){  // invert colors
      tft.drawBitmap(dino.x, dino.y, epd_bitmap_dead, dino.width, dino.height, 0x07FF);
    } else {
      tft.drawBitmap(dino.x, dino.y, epd_bitmap_dead, dino.width, dino.height, ST7735_RED);
    }

    return 1;
  }
}

void drawHitboxes() {
  tft.drawRect(obstacle->x, obstacle->y, obstacle->width, obstacle->height, ST7735_RED);                                                   // Obstacle
  tft.drawRect(dino.x + dino.padding, dino.y + dino.padding, dino.width - 2 * dino.padding, dino.height - 2 * dino.padding, ST7735_BLUE);  // Dino
}

void deleteDino() {
  // Dino Löschen
  if (dino.ducking == true) {
    tft.fillRect(dino.x, dino.y, dino.duckWidth, dino.duckHeight, ST7735_WHITE);  // Duck Dino
    dino.jumping = false;
  } else {
    tft.fillRect(dino.x, dino.y, dino.width, dino.height, ST7735_WHITE);  // Normaler Dino
  }
}

// checked every frame
void calcJump() {
  // activate when the jumping interrupt is triggered and the dino is not ducking
  if (dino.jumping == true && dino.ducking == false) {
    // only once: initialize the melody and the jump when the jump has started
    if (!jumping_flag)
    {
      jumping_flag = true;
      jump_melody_flag = true; 
      start_jump_melody_timestamp = millis();
      start_jump_timestamp = millis();

    }
    // play the jump melody, stop playing after the melody duration, ensure that the melody is triggered only once per jump
    if (jump_melody_flag) {
      playMelody(jump_melody, jump_melody_durations, jump_melody_length);
      if (millis() - start_jump_melody_timestamp < jump_melody_aggregate_duration)
      jump_melody_flag = false;
    }

    // get the time passed since the jump started 
    int unsigned long delta_time = millis() - start_jump_timestamp;
    Serial.print(String(" delta_time:") + delta_time);
    Serial.print(String(" millis():") + millis());
    Serial.print(String(" start_jump_timestamp:") + start_jump_timestamp);

    // calculate the fraction of the jump progress -> value between 0 and 1
    jump_progress = ((float)delta_time / (float)jump_duration);
    Serial.print(String(" jump progress:") + jump_progress);

    // stop the jump if the jump duration is exceeded
    if (jump_progress >= 1.0f) {
      dino.jumping = false;
      dino.y = dino.yStart;
      start_jump_melody_timestamp = 0;
      jump_progress = 0.0;
      jumping_flag = false;
      jump_melody_flag = false;
      return;
    }

    // calculate the next y-coordinate
    float steepness = 0.6; // smaller -> sharper jump
    float base = sin(jump_progress * PI);
    float offset = pow(fabs(base), steepness) * JUMP_TOP_COORDINATE;

    // restore prefix (+/-)
    if (base < 0) offset = -offset;

    Serial.println(String(" offset:") + offset);

    // set the next y-coordinate
    dino.y = dino.yStart - (int)offset;

  }
}

void checkDinoCollision() {
  // Kollision?
  if (dino.ducking == true) {
    if (checkAABBCollision(dino.x + dino.padding, dino.y + dino.padding, dino.duckWidth - 2 * dino.padding, dino.duckHeight - 2 * dino.padding, obstacle->x, obstacle->y, obstacle->width, obstacle->height)) {
      Serial.println("Collison?");
      dead = 1;
    }
  } else {
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

  if (newObstacle == true) {  // neues Obstacles setzen
    randomSeed((int)millis());
    int randomObs = random(0, validObstaclesLength);
    obstacle = validObstacles[randomObs];

    switch (obstacle->type) {  // initialiseren des Objekts
      case SPRITE_CACTUS: initCactus(); break;
      case SPRITE_CACTUS2: initCactus2(); break;
      case SPRITE_CACTUS3: initCactus3(); break;
      case SPRITE_CACTUS4: initCactus4(); break;
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

void playMelody(int melody[], int durations[], int melody_length) {
  if (!melody_is_playing_flag){
    current_melody = melody;
    current_melody_durations = durations;
    current_melody_length = melody_length;
    current_note_index = 0;
    melody_is_playing_flag = true;
    prev_time = millis();
    tone(BUZZER_PIN, current_melody[current_note_index], current_melody_durations[current_note_index]);
  } else {
    if (millis() - prev_time >= current_melody_durations[current_note_index]) {
      if (current_melody_length-1 != current_note_index) {
        current_note_index += 1;
        tone(BUZZER_PIN, current_melody[current_note_index], current_melody_durations[current_note_index]);
      } else {
        melody_is_playing_flag = false;
        noTone(BUZZER_PIN);
      }
      prev_time = millis();
    }
  }
}

void updateMelody() {
  if (melody_is_playing_flag) {
    playMelody(current_melody, current_melody_durations, current_melody_length);
  }
}

void draw_start_screen() {
  tft.fillScreen(ST7735_WHITE);

  tft.setTextColor(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setCursor(35, 10);
  tft.print("Dino Run");
  tft.setTextSize(1);
  tft.setCursor(89,63);
  tft.print("left:  jump");
  tft.setCursor(89,78);
  tft.print("right: duck");
  tft.setCursor(89,110);
  if (DarkMode){ // invert colors
    tft.setTextColor(0xFFE0);
  } else {
    tft.setTextColor(ST7735_BLUE);
  }
  tft.print("left: start");

  tft.drawBitmap(5, 45, epd_bitmap_start_screen_dino, 75, 75, ST7735_WHITE, ST7735_BLACK);
}

bool handle_pressed_buttons(){
  if (digitalRead(JMP_BUTTON_PIN) == LOW) {
    game_start_flag = false;
    delay(50);
    reset();
    return true;
  } else if (digitalRead(DUCK_BUTTON_PIN) == LOW)  {
    DarkMode = !DarkMode;
    tft.invertDisplay(DarkMode);
    draw_start_screen();
    delay(50);
  }
  return false;
}

void blinkingHighScore() {
  if (!is_blinking) {
    is_blinking = !is_blinking;
    blink_timestamp = millis();
  }
  if (millis() - blink_timestamp > blink_intervall) {
    uint16_t color = tft.color565(random(0, 255), random(0, 255), random(0, 255));
    tft.setTextColor(color);

    int highscore_length = (String(highscore).length() + String("Highscore").length())*6;
    int start_drawing = (tft.width() - highscore_length)/2;
    tft.setCursor(start_drawing, 35);
    //Serial.print(String("Highscore") + highscore_length);
    tft.print("Highscore:" + String(highscore));
    is_blinking = false;
  }
}

void loop() { 
  // Start Screen + User Interaction
  if (game_start_flag) {
    draw_start_screen();
    playMelody(start_melody, start_melody_durations, start_melody_length);
    while (1) {
      updateMelody();
      blinkingHighScore();
      if (handle_pressed_buttons()) {
        break;
      }
    }
  } 
  framecount++;
  unsigned long frameStart = millis();

  unsigned long now = millis();
  // Play music if a melody is triggered
  updateMelody();
  if (now - lastFPSUpdate >= 1000) {  // FPS jede Sek anzeigen
    fps = framecount - lastFramecount;
    lastFramecount = framecount;
    lastFPSUpdate = now;

    tft.fillRect(tft.width() - 40, 5, 40, 10, ST7735_WHITE);
    tft.setCursor(tft.width() - 40, 5);
    tft.print(String("FPS:") + fps);
    Serial.println(String("FPS: ") + fps);
  }
  if (gameSpeed <= 4 && score % MILESTONE == 0 && score != 0 && score != lastScore) {
    gameSpeed += 0.5;
    lastScore = score; // only trigger once per score
    Serial.println(String("Speed: ") + gameSpeed);
    playMelody(milestone_melody, milestone_melody_durations, milestone_melody_length);
  }

  int status = drawFrame();
  if (status == 0) {  // geht weiter
    unsigned long frameEnd = millis();
    unsigned long frameTime = frameEnd - frameStart;

    // DeltaTime
    if (targetFrameTime - (int)frameTime <= 0) {  // FPS drop! -> mehr Zeit zum Zeichnen benötigt als TargetFrameTime
      delay(0);
    } else {
      delay(targetFrameTime - (int)frameTime);
    }

    tft.fillRect(tft.width() - 29, 17, 35, 10, ST7735_WHITE);
    tft.setCursor(tft.width() - 29, 17);
    tft.print(frameTime);
    tft.print("ms");

    // animieren jeden 4. Frame
    if (framecount % 4 == 0) {
      animate = true;
      score++;
    }
  } else if (status == 1) {  // tot -> GameOver
    tft.setTextSize(2);

    if (DarkMode){  // invert colors
      tft.setTextColor(0x07FF);
      tft.fillRect(25, 47, 112, 20, ST7735_WHITE);
      tft.drawRect(25, 47, 112, 20, 0x07FF);
    } else{
      tft.setTextColor(ST7735_RED);
      tft.fillRect(25, 47, 112, 20, ST7735_WHITE);
      tft.drawRect(25, 47, 112, 20, ST7735_RED);
    }
    tft.setCursor(28, 50);
    tft.print("GAME OVER");
    tft.setTextColor(ST7735_BLACK);
    tft.setTextSize(1);

    // ensure that the jumping melody is not overlapping with the game over melody
    melody_is_playing_flag = false;
    noTone(BUZZER_PIN);

    // play game over melody
    game_over_time = millis();
    playMelody(loss_melody, loss_melody_durations, loss_melody_length);
    while (millis() - game_over_time <= 2000) {
      updateMelody();
      delay(10);
    } 
    delay(1000);
    game_over_time = 0;
    game_start_flag = true;

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
    delay(1000);
    reset();
  }
}
