//
// PDQ_ST7735 configuration
//
enum{
	ST7735_INITB              = 0,        // 1.8" (128x160) ST7735B chipset (only one type)
	ST7735_INITR_GREENTAB     = 1,        // 1.8" (128x160) ST7735R chipset with green tab (same as ST7735_INITR_18GREENTAB)
	ST7735_INITR_REDTAB       = 2,        // 1.8" (128x160) ST7735R chipset with red tab (same as ST7735_INITR_18REDTAB)
	ST7735_INITR_BLACKTAB     = 3,        // 1.8" (128x160) ST7735S chipset with black tab (same as ST7735_INITR_18BLACKTAB)
	ST7735_INITR_144GREENTAB  = 4,        // 1.4" (128x128) ST7735R chipset with green tab
	ST7735_INITR_18GREENTAB   = ST7735_INITR_GREENTAB,  // 1.8" (128x160) ST7735R chipset with green tab
	ST7735_INITR_18REDTAB     = ST7735_INITR_REDTAB,    // 1.8" (128x160) ST7735R chipset with red tab
	ST7735_INITR_18BLACKTAB   = ST7735_INITR_BLACKTAB,  // 1.8" (128x160) ST7735S chipset with black tab
};

#define ST7735_CHIPSET ST7735_INITR_BLACKTAB	// <= Set ST7735 LCD chipset/variation here (from above list)

#define	ST7735_CS_PIN   10  // <= /CS pin (chip-select, LOW to get attention of ST7735, HIGH and it ignores SPI bus)
#define	ST7735_DC_PIN   9   // <= DC pin (1=data or 0=command indicator line) also called RS
#define	ST7735_RST_PIN  8

// other PDQ library options
#define	ST7735_SAVE_SPCR 0  // <= 0/1 with 1 to save/restore AVR SPI control register (to "play nice" when other SPI use)
