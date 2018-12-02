
#ifndef GWU18_H
#define GWU18_H

#include "lib/app.h"

#include "lib/gpio.h"
#include "lib/1w.h"
#include "lib/tsv.h"
#include "lib/device/ds18b20.h"
#include "lib/lcorrection.h"
#include "lib/timef.h"

#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/prog.h"
#include "lib/acp/ds18b20.h"
#include "lib/udp.h"


#define APP_NAME gwu18
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./config/"
#endif

#define CONF_MAIN_FILE CONF_DIR "main.tsv"
#define CONF_DEVICE_FILE CONF_DIR "device.tsv"
#define CONF_LCORRECTION_FILE CONF_DIR "lcorrection.tsv"

typedef struct {
    int id;
    uint8_t addr[DS18B20_SCRATCHPAD_BYTE_NUM];
    int pin;
    FTS result;
    int resolution;
    int resolution_state; //0 if reading resolution from device failed
    int resolution_set_state; //0 if writing resolution to device failed
    unsigned int retry_count;
    LCorrection *lcorrection;
} Device;

DEC_LIST(Device)


extern int readSettings();

extern void serverRun(int *state, int init_state);

extern int initApp();

extern int initData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

#endif 

