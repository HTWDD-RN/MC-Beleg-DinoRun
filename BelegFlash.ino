//#include <Adafruit_GFX.h>
//#include <Adafruit_ST7735.h>
// New Lib: https://github.com/andrey-belokon/PDQ_GFX_Libs
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
// - Geschwindigkeit langsam erhöhen bei Score Meilensteinen (alle 100)
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

PDQ_ST7735 tft; // Pins sind im Header

#define DinoX 5
#define DinoYStart 90
int DinoY = DinoYStart;
#define DinoWidth 20
#define DinoHeight 21
#define DinoDuckWidth 28
#define DinoDuckHeight 13
int DinoFrame = 0; // AnimationsFrame
#define DinoDuckYOffset 8; // Offset für geduckten Dino
bool wasDucking = false;

int CloudX = tft.width() + random(-100, 70); // random Offset
#define CloudY 30
#define CloudWidth 46
#define CloudHeight 13

int CactusX = tft.width() + random(20, 100); // random Offset
#define CactusY 80
#define CactusWidth 23
#define CactusHeight 48

#define HLLineY 111

#define ScoreX 5
#define ScoreY 5

bool DarkMode = true;

int targetFrameDelay = 50; // Ziel Frametime in ms, wird reduziert durch Zeichenaufwand
// Delta X (px pro Frame)
int CloudDX = 2;
int CactusDX = 5;

#define EEPROMHighscore 0 // Highscore permant in EEPROM speichern
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
const int jumpLength = sizeof(jumpHeights) / sizeof(jumpHeights[0]); // Länge = ArrayLänge in Byte / Größe ArrayElement
bool jump = false;
int jumpProgress = 0;

bool duck = false;

int dead = 0;

/*
//Sd2Card card;
//SdVolume volume;
//SdFile root;
SdFat SD;
File myFile;
*/

void jumpButtonFunc(){
  jump = true;
}

/*
void initSD(){
  Serial.print("Init SD card...");
  tft.setCursor(40, 50);
  tft.print("SD init...");

  if (!SD.begin(SD_CS)){
    Serial.println("FAIL!");
    tft.print("FAIL!");
    return;
  }
  Serial.println("OK!");
  tft.print("OK!");


  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("FAIL!");
    tft.print("FAIL!");
    return;
  }
  Serial.println("OK!");
  tft.print("OK!");

  Serial.print("Card type: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:  Serial.println("SD1");  break;
    case SD_CARD_TYPE_SD2:  Serial.println("SD2");  break;
    case SD_CARD_TYPE_SDHC: Serial.println("SDHC"); break;
    default:                Serial.println("Unknown");
  }

  
  if (!volume.init(card)) {
    Serial.println("Volume init fail");
    tft.setCursor(40, 70);
    tft.print("No FAT?");
    return;
  }

   uint32_t volumeSize = volume.clusterCount() * volume.blocksPerCluster() / 2; // in KB
  Serial.print("Volume size (MB): ");
  Serial.println(volumeSize / 1024.0, 2);

  tft.setCursor(40, 70);
  tft.print("Size: ");
  tft.print(volumeSize / 1024.0, 2);
  tft.print("MB");
  

  myFile = SD.open("/test.txt", FILE_WRITE);
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");
    // close the file:
    myFile.close();
  }
  else{
    Serial.println(String("Return Code: ") + myFile);
  }
}
*/

void setup() {
  pinMode(JMP_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(JMP_BUTTON_PIN), jumpButtonFunc, FALLING);
  pinMode(DUCK_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);

  #ifdef ST7735_RST_PIN	// Reset wie Adafruit
	FastPin<ST7735_RST_PIN>::setOutput();
	FastPin<ST7735_RST_PIN>::hi();
	FastPin<ST7735_RST_PIN>::lo();
	delay(1);
	FastPin<ST7735_RST_PIN>::hi();
  #endif


  // Initialisiere das TFT-Display
  //tft.initR(INITR_BLACKTAB);
  tft.begin();
  tft.invertDisplay(DarkMode); // "DarkMode"
  
  tft.setRotation(1);
  tft.fillScreen(ST7735_WHITE);
  tft.setTextColor(ST7735_BLACK);

  //initSD();
  Serial.print("");
  delay(1000);

  reset();
}

void reset(){
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

  jump = false;
  jumpProgress = 0;

  duck = false;
  wasDucking = false;

  dead = 0;

  CloudX = tft.width() + random(-100, 70);
  CactusX = tft.width() + random(20, 100);
  DinoY = DinoYStart;
}

// https://kishimotostudios.com/articles/aabb_collision/
bool checkAABBCollision(int AX, int AY, int AWidth, int AHeight, int BX, int BY, int BWidth, int BHeight){
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

void checkDuck(){
  // Ducken -> Offset beim Umschalten setzen/entfernen
  bool isPressed = digitalRead(DUCK_BUTTON_PIN) == LOW;
  if (isPressed == true && wasDucking == false && jump == false) { // ducken nicht erlaubt beim Springen
    // starten des ducken
    tft.fillRect(DinoX, DinoY, DinoWidth, DinoHeight, ST7735_WHITE); // Normaler Dino
    DinoY += DinoDuckYOffset;
    wasDucking = true;
    duck = true;

  } else if (isPressed == false && wasDucking == true) {
    // aufhören zu ducken
    tft.fillRect(DinoX, DinoY, DinoDuckWidth, DinoDuckHeight, ST7735_WHITE); // Duck Dino
    DinoY -= DinoDuckYOffset;
    wasDucking = false;
    duck = false;
  }
}

void drawScore(){
  tft.fillRect(ScoreX, ScoreY, 30, 10, ST7735_WHITE); // Score löschen
  score++;
  tft.setCursor(ScoreX, ScoreY);
  tft.print(score);
}

void drawCloud(){
  tft.fillRect(CloudX, CloudY, CloudWidth, CloudHeight, ST7735_WHITE); // Wolke löschen
  // Neue Wolkenposition
  CloudX -= CloudDX;
  if (CloudX <= -CloudWidth) {
    CloudX = tft.width() + random(10,70);
  }
  tft.drawBitmap(CloudX, CloudY, epd_bitmap_cloud, CloudWidth, CloudHeight, tft.color565(150, 150, 150));
}

void drawCactus(){
  tft.fillRect(CactusX, CactusY, CactusWidth, CactusHeight, ST7735_WHITE); // Kaktus löschen
  // Neue Kaktusposition
  CactusX -= CactusDX;
  if (CactusX <= -CactusWidth) {
    CactusX = tft.width() + random(20,100);
  }
  tft.drawBitmap(CactusX, CactusY, epd_bitmap_cactus, CactusWidth, CactusHeight, tft.color565(50, 50, 50));
}

void drawGround(){
  tft.drawFastHLine(0, HLLineY, tft.width(), tft.color565(50, 50, 50)); // horizontale Linie
}

int drawDino(){
  // Dino animieren
  if (dead != 1){
    if (DinoFrame == 0){
      DinoFrame = 1;
      if (duck == true){
        tft.drawBitmap(DinoX, DinoY, epd_bitmap_duck, DinoDuckWidth, DinoDuckHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoDuckWidth - 2 * 4, DinoDuckHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
      else {
        tft.drawBitmap(DinoX, DinoY, epd_bitmap_dino, DinoWidth, DinoHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
    }
    else if (DinoFrame == 1){
      DinoFrame = 0;
      if (duck == true){
        tft.drawBitmap(DinoX, DinoY, epd_bitmap_duck2, DinoDuckWidth, DinoDuckHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoDuckWidth - 2 * 4, DinoDuckHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
      else{
        tft.drawBitmap(DinoX, DinoY, epd_bitmap_dino2, DinoWidth, DinoHeight, tft.color565(50, 50, 50));
        //tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE); // Hitbox
      }
    }
    return 0;
  }
  else { // tot
    if (duck == true){
      DinoY -= DinoDuckYOffset;
    }
    tft.drawBitmap(CactusX, CactusY, epd_bitmap_cactus, CactusWidth, CactusHeight, tft.color565(50, 50, 50));
    tft.drawRect(CactusX, CactusY, CactusWidth, CactusHeight, ST7735_RED); // Hitbox
    
    tft.drawBitmap(DinoX, DinoY, epd_bitmap_dead, DinoWidth, DinoHeight, ST7735_RED);
    tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST7735_BLUE);
    return 1;
  }
}

void deleteDino(){
  // Dino Löschen
  if (duck == true){
    tft.fillRect(DinoX, DinoY, DinoDuckWidth, DinoDuckHeight, ST7735_WHITE); // Duck Dino
    jump = false;
  }
  else {
    tft.fillRect(DinoX, DinoY, DinoWidth, DinoHeight, ST7735_WHITE); // Normaler Dino
  }
}

void calcJump(){
  // Todo: Sprungpos. dynamisch berechnen
  if (jump == true && duck == false) {
    DinoY += jumpHeights[jumpProgress];
    jumpProgress++;

    if (jumpProgress >= jumpLength) {
      jump = false;
      jumpProgress = 0;
    }
  }
}

void checkDinoCollision(){
  // Kollision?
  if (duck == true){
    if (checkAABBCollision(DinoX, DinoY, DinoDuckWidth, DinoDuckHeight, CactusX, CactusY, CactusWidth, CactusHeight)){
      Serial.println("Collison?");
      dead = 1;
    }
  }
  else{
    if (checkAABBCollision(DinoX, DinoY, DinoWidth, DinoHeight, CactusX, CactusY, CactusWidth, CactusHeight)){
      Serial.println("Collison?");
      dead = 1;
    }
  }
}

int drawFrame(){
  checkDuck();

  drawScore();
  drawCloud();
  drawCactus();
  drawGround();
  
  deleteDino();
  calcJump();
  checkDinoCollision();
  
  int status = drawDino();
  return status;
}

void loop() {
  unsigned long now = millis();
  if (now - lastFPSUpdate >= 1000) { // FPS jede Sek anzeigen
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
    if (status == 1){ // tot -> GameOver
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
      if (score > highscore){
        EEPROM.put(0, score);
      }
      #else 
      if (score > highscore){
        highscore = score;
      }
      #endif

      delay(2000);
      reset();
    }
    else if (status == 0){ // geht weiter
      // DeltaTime
      unsigned long frameEnd = millis();
      unsigned long frameTime = frameEnd - frameStart;

      //Serial.println(String("FrameTime: ") + round(frameTime));
      if (targetFrameDelay - (int)frameTime <= 0){ // FPS drop! -> mehr Zeit zum Zeichnen benötigt als TargetFrameTime
        delay(0);
      }
      else{
        delay(targetFrameDelay - round(frameTime));
      }

      tft.fillRect(tft.width() - 29, 17, 35, 10, ST7735_WHITE);
      tft.setCursor(tft.width() - 29, 17);
      tft.print(frameTime);
      tft.print("ms");
    }
}


