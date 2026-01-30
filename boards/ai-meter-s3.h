// Kamera (OV2640)
#define XCLK_GPIO_NUM      14
#define PCLK_GPIO_NUM      13
#define VSYNC_GPIO_NUM     47
#define HREF_GPIO_NUM      21
#define SIOD_GPIO_NUM      41
#define SIOC_GPIO_NUM      40
#define Y2_GPIO_NUM        6
#define Y3_GPIO_NUM        7
#define Y4_GPIO_NUM        8
#define Y5_GPIO_NUM        9
#define Y6_GPIO_NUM        10
#define Y7_GPIO_NUM        11
#define Y0_GPIO_NUM        4
#define Y1_GPIO_NUM        5
#define Y9_GPIO_NUM        12

// SD-Karte (4-bit SDIO)
#define SD_MISO   42   // DAT0
#define SD_MOSI    2   // CMD
#define SD_SCLK    1   // CLK
#define SD_CS     48   // DAT3
#define SD_DAT1   39
#define SD_DAT2   38

// LEDs
#define LED_CTRL_PIN 12
#define RGB_LED_PIN  48

// Power Enable
#define PER_EN_PIN 3

// Ethernet W5500
#define ETH_CS    33
#define ETH_SCLK  36
#define ETH_MOSI  35
#define ETH_MISO  34
#define ETH_INT   37
#define ETH_EN    20
