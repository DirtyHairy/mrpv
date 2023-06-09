#include "config_local.h"

// https://remotemonitoringsystems.ca/time-zone-abbreviations.php
#define TIMEZONE "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"

#define MONDAY "Montag"
#define TUESDAY "Dienstag"
#define WEDNESDAY "Mittwoch"
#define THURSDAY "Donnerstag"
#define FRIDAY "Freitag"
#define SATURDAY "Samstag"
#define SUNDAY "Sonntag"

#define FORMAT_TIME "%H:%M:%S"
#define FORMAT_DATE "%d.%m.%Y"
#define FORMAT_DATETIME "%s Uhr / %s %s"

#define LABEL_PV "PV"
#define LABEL_PV_ACCUMULATED "heute"
#define LABEL_LOAD "Last"
#define LABEL_LOAD_ACCUMULATED "heute"
#define LABEL_SURPLUS "Abgabe"
#define LABEL_NETWORK "Bezug"

#define HOSTNAME "mrpv"

#define NTP_SERVER "pool.ntp.org"

#define AESS_API_SERVER "https://openapi.alphaess.com"
#define AESS_API_CURRENT_POWER "/api/getLastPowerData"
#define AESS_API_ACCUMULATED_POWER "/api/getOneDateEnergyBySn"
#define AESS_API_ERROR_RATE_LIMIT 6053
#define AESS_API_OK 200

#define RATE_LIMIT_CURRENT_POWER_SEC 10
#define RATE_LIMIT_ACCUMULATED_POWER_SEC 300

#define MAX_TIME_WITHOUT_TIME_SYNC_MINUTES 25
#define TTL_CURRENT_POWER_MINUTES 5
#define TTL_ACCUMULATED_POWER_MINUTES 15
#define TTL_DHCP_LEASE_MINUTES 1440

#define WIFI_TIMEOUT_MSEC 10000
#define NTP_TIMEOUT_MSEC 10000
#define CONNECTION_TIMEOUT_MSEC 20000
#define REQUEST_TIMEOUT_MSEC 20000

#define FULL_REFRESH_EVERY_CYCLE 15

#define SLEEP_SECONDS 60

#define SPI_PIN_SCLK GPIO_NUM_13
#define SPI_PIN_MOSI GPIO_NUM_14

#define DISPLAY_SPI_SPEED SPI_MASTER_FREQ_20M
#define DISPLAY_PIN_CS GPIO_NUM_15
#define DISPLAY_PIN_RST GPIO_NUM_26
#define DISPLAY_PIN_DC GPIO_NUM_27
#define DISPLAY_PIN_BUSY GPIO_NUM_25
