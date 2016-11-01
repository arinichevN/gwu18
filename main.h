
#ifndef REGULATOR_H
#define REGULATOR_H


#include "lib/db.h"
#include "lib/app.h"
#include "lib/gpio.h"
#include "lib/1w.h"
#include "lib/timef.h"
#include "lib/ds18b20.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/ds18b20.h"
#include "lib/udp.h"


#define APP_NAME ("gwu18")
#define ID_BROADCAST 0xff
#define RETRY_NUM 5
#ifndef MODE_DEBUG
#define CONF_FILE ("/etc/controller/gwu18.conf")
#endif
#ifdef MODE_DEBUG
#define CONF_FILE ("main.conf")
#endif

enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    WTIME
} StateAPP;

typedef struct {
    char state;
    struct timespec interval;
    Ton_ts timer;
    unsigned int max_row;
} Log;

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
    Log log;
} Device;

typedef struct {
    Device *device;
    size_t length;
} DeviceList;

typedef struct {
    pthread_t *thread;
    size_t length;
} ThreadList;

typedef struct {
    int thread_attr_init;
    int thread_attr_setdetachstate;
    int thread_create;
} ThreadInitSuccess;

typedef struct {
    pthread_attr_t thread_attr;
    pthread_t thread;
    ThreadInitSuccess init_success;
    I1List i1l;
    I2List i2l;
    I3List i3l; 
    char cmd;
    char qfr;
    int on;
    struct timespec cycle_duration;
} ThreadData;


extern int readSettings() ;

extern int initAppData(DeviceList *dl) ;

extern int checkAppData(DeviceList *dl) ;

extern void serverRun(int *state, int init_state) ;

extern int saveLog(int dev_id,float temp, int track_limit, int year, int month, int mday, int hour, int min, int sec);

extern void *threadFunction(void *arg);

extern int createThread(const DeviceList *dl, ThreadData *td, int buf_len);

extern void initApp() ;

extern void initData() ;

extern void freeThread() ;

extern void freeAppData() ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif /* REGULATOR_H */

