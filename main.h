
#ifndef GWU18_H
#define GWU18_H

#include "lib/app.h"

#ifndef PLATFORM_ANY
#include "lib/gpio.h"
#include "lib/1w.h"
#endif

#include "lib/ds18b20.h"

#include "lib/timef.h"

#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/ds18b20.h"
#include "lib/udp.h"


#define APP_NAME gwu18
#define APP_NAME_STR TOSTRING(APP_NAME)
#define ID_BROADCAST 0xff
#define RETRY_NUM 5

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif

#define DEVICE_FILE "" CONF_DIR "device.tsv"
#define CONFIG_FILE "" CONF_DIR "config.tsv"
#define CONFIG_FILE_DB "" CONF_DIR "main.conf"

enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    WTIME
} StateAPP;


typedef struct {
    int id;
    uint8_t addr[DS18B20_SCRATCHPAD_BYTE_NUM];
    int pin;
    struct timespec tm;
    float value;
    int resolution;
    int value_state;//0 if reading value from device failed
    int resolution_state;//0 if reading resolution from device failed
    int resolution_set_state;//0 if writing resolution to device failed
} Device;

DEF_LIST(Device)


extern int readSettings() ;

extern int initDevice(DeviceList *dl) ;

extern int checkDevice(DeviceList *dl) ;

extern void serverRun(int *state, int init_state) ;

extern void initApp() ;

extern int initData() ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif 

