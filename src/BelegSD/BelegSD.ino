#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ImageReader.h>
#include <SdFat.h>
#include <SPI.h>

// TODO: 
// - bessere Sprungberechnung mit Parabel (Gravity und Sprungzeit)
// - Schauen wie lange ein Frame zum Zeichnen braucht und die delay entsprechend anpassen (Delta Time) -> Abziehen der Zeit zum Zeichnen/Logik
// - mehrere verschiedene Kakteen als neue Bilder, vielleicht auch mehere Kakteen auf einmal
// - Grafik für Boden -> Pixel als Steine
// - vielleicht auch noch zweite Taste zum Ducken hinzufügen, wenn Vogel kommt
// - Titelbildschirm/GameOver Bildschirm
// - Sound über Beeper
// - Landscape Orientation für das Display?
// - HighScore auf SDCard speichern?
// - wir haben Stand jetzt nur noch ~3.5K Flash Speicher!!
// - -> Bilder müssen deshalb von SDCard geladen werden -> wie bekommt man das Blinken beim Zugriff auf SDCard weg??

// Pinbelegung für das TFT-Display
#define TFT_CS     10
#define TFT_DC     9
#define TFT_RST    8
#define SD_CS      4   

#define BUTTON_PIN 2

// VCC: 5V, LED: 3,3V!!
// SDA: 11, SCK: 13 (für TFT/SD)

// SD MISO : 12
// SD MOSI: SDA (11)
// SD CLK: SCK (13)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

SdFat SD;
Adafruit_ImageReader reader(SD);
//ImageReturnCode stat;

#define DinoX 5
#define DinoYStart 120
int DinoY = DinoYStart;
int32_t DinoWidth = 0; int32_t DinoHeight = 0;

int CloudX = tft.width() + random(-100, 70); // random Offset
#define CloudY 50
int32_t CloudWidth = 0; int32_t CloudHeight = 0;

int CactusX = tft.width() + random(20, 100); // random Offset
#define CactusY 110
int32_t CactusWidth = 0; int32_t CactusHeight = 0;

#define ScoreX 5
#define ScoreY 5

int dx = 5;  // Delta X (px pro Frame)

int framedelay = 50;

int score = 0;

const int jumpHeights[] = {
  -13, -10, -7, -5, -4, -3, -2,
   0,
   2, 3, 4, 5, 7, 10, 13
};
const int jumpLength = sizeof(jumpHeights) / sizeof(jumpHeights[0]); // Länge = ArrayLänge in Byte / Größe ArrayElement
bool jump = false;
int jumpProgress = 0;

int dead = 0;

void initSD() {
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS)) {
    Serial.println(F("FAIL!"));
    tft.setCursor(25, 70);
    tft.print("SD Card fail!");
    while(true){

    }
    return;
  }
  Serial.println(F("OK!"));

  Serial.print(F(" Size: "));
  Serial.print(SD.card()->cardSize() / 2048);
  Serial.println(F(" MB"));

  SD.ls(LS_SIZE);

}

void ButtonFunc(){
  //Serial.println("Hello from ButtonFunc");
  jump = true;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), ButtonFunc, FALLING);

  Serial.begin(115200);
  
  // Initialisiere das TFT-Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  tft.fillScreen(ST7735_WHITE);
  tft.setTextColor(ST7735_BLACK);

  initSD();

  reset();

  // Breite/Höhe der Bilder bekommen
  reader.bmpDimensions("/dino/dinohf.bmp", &DinoWidth, &DinoHeight);
  reader.bmpDimensions("/dino/cloud.bmp", &CloudWidth, &CloudHeight);
  reader.bmpDimensions("/dino/cactus.bmp", &CactusWidth, &CactusHeight);
}

void reset(){
  tft.fillScreen(ST7735_WHITE);
  tft.setCursor(40, 10);
  tft.print("DINO RUN!");
  score = 0;

  jump = false;
  jumpProgress = 0;

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

  return !(AisToTheRightOfB
    || AisToTheLeftOfB
    || AisAboveB
    || AisBelowB);
}

void loop() {
  // Löschen
  tft.fillRect(CloudX, CloudY, CloudWidth, CloudHeight, ST7735_WHITE); // Wolke
  tft.fillRect(CactusX, CactusY, CactusWidth, CactusHeight, ST7735_WHITE); // Kaktus
  tft.fillRect(DinoX, DinoY, DinoWidth, DinoHeight, ST7735_WHITE); // Dino
  tft.fillRect(ScoreX, ScoreY, 30, 10, ST7735_WHITE); // Score

  // Neue Wolkenposition
  CloudX -= dx;
  if (CloudX <= -CloudWidth) {
    CloudX = tft.width() + random(10,70);
  }

  // Neue Kaktusposition
  CactusX -= dx;
  if (CactusX <= -CactusWidth) {
    CactusX = tft.width() + random(20,100);
  }

  if (jump == true) {
    DinoY += jumpHeights[jumpProgress];
    jumpProgress++;

    if (jumpProgress >= jumpLength) {
      jump = false;
      jumpProgress = 0;
    }
  }

  score++;
  tft.setCursor(ScoreX, ScoreY);
  tft.print(score);

  // collision
  if (checkAABBCollision(DinoX, DinoY, DinoWidth, DinoHeight, CactusX, CactusY, CactusWidth, CactusHeight)){
    Serial.println("Collison?");
    dead = 1;
  }

  tft.drawFastHLine(0, 140 , tft.width(), ST7735_BLACK); // horizontale Linie

  reader.drawBMP("/dino/cloud.bmp", tft, CloudX, CloudY);   // Neue Wolke zeichnen
  reader.drawBMP("/dino/cactus.bmp", tft, CactusX, CactusY);    // Neuen Kaktus zeichnen

  tft.drawRect(CactusX, CactusY, CactusWidth, CactusHeight, ST77XX_RED);
  // Dino animieren
  if (dead != 1){
    reader.drawBMP("/dino/dinohf.bmp", tft, DinoX, DinoY);
    tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE);
    delay(framedelay/2);

    reader.drawBMP("/dino/dino2hf.bmp", tft, DinoX, DinoY);
    tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE);
    delay(framedelay/2);
  }
  else { // tot
    reader.drawBMP("/dino/deadhf.bmp", tft, DinoX, DinoY);
    tft.drawRect(DinoX + 4, DinoY + 4, DinoWidth - 2 * 4, DinoHeight - 2 * 4, ST77XX_BLUE);
    delay(5000);
    reset();
  }
}

