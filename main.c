/*
 * UPD gateway for DS18B20
 */

#include "main.h"

char app_class[NAME_SIZE];
char pid_path[LINE_SIZE];
int app_state = APP_INIT;
int pid_file = -1;
int proc_id = -1;
int udp_port = -1;
int udp_fd = -1; //udp socket file descriptor
size_t udp_buf_size = 0;
Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};

char db_conninfo_settings[LINE_SIZE];
char db_conninfo_data[LINE_SIZE];
#ifndef FILE_INI
PGconn *db_conn_settings = NULL;
PGconn *db_conn_data = NULL;
#endif
DeviceList device_list = {NULL, 0};

I1List i1l = {NULL, 0};

#include "util.c"
#include "print.c"
#ifdef FILE_INI
#pragma message("MESSAGE: data from file")
#include "init_f.c"
#else
#pragma message("MESSAGE: data from DBMS")
#include "init_d.c"
#endif

#ifdef PLATFORM_ANY
#pragma message("MESSAGE: building for all platforms")
#else
#pragma message("MESSAGE: building for certain platform")
#endif

int checkDevice(DeviceList *dl) {
    size_t i, j;
    //valid pin address
#ifndef PLATFORM_ANY
    for (i = 0; i < dl->length; i++) {
        if (!checkPin(dl->item[i].pin)) {
            fprintf(stderr, "checkDevice: check device table: bad pin=%d where app_class='%s' and id=%d\n", dl->item[i].pin, app_class, dl->item[i].id);
            return 0;
        }
    }
#endif
    //unique id
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->item[i].id == dl->item[j].id) {
                fprintf(stderr, "checkDevice: check device table: ids should be unique, repetition found where id=%d and app_class='%s'\n", dl->item[i].id, app_class);
                return 0;
            }
        }
    }
    //unique addresses on certain pin
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->item[i].pin == dl->item[j].pin && memcmp(dl->item[i].addr, dl->item[j].addr, sizeof dl->item[i].addr) == 0) {
                fprintf(stderr, "checkDevice: check device table: addresses on certain pin should be unique, repetition found where id=%d and id=%d and app_class='%s'\n", dl->item[i].id, dl->item[j].id, app_class);
                return 0;
            }
        }
    }
    return 1;
}

void serverRun(int *state, int init_state) {
    char buf_in[udp_buf_size];
    char buf_out[udp_buf_size];
    uint8_t crc;
    size_t i, j;
    char q[LINE_SIZE];
    crc = 0;
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(udp_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
    }
#ifdef MODE_DEBUG
    dumpBuf(buf_in, sizeof buf_in);
#endif    
    if (!crc_check(buf_in, sizeof buf_in)) {
#ifdef MODE_DEBUG
        fputs("WARNING: serverRun: crc check failed\n", stderr);
#endif
        return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_APP_START:
            if (!init_state) {
                *state = APP_INIT_DATA;
            }
            return;
        case ACP_CMD_APP_STOP:
            if (init_state) {
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            *state = APP_EXIT;
            return;
        case ACP_CMD_APP_PING:
            if (init_state) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_BUSY);
            } else {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_IDLE);
            }
            return;
        case ACP_CMD_APP_PRINT:
            printData(&device_list);
            return;
        case ACP_CMD_APP_HELP:
            printHelp();
            return;
        default:
            if (!init_state) {
                return;
            }
            break;
    }
    switch (buf_in[0]) {
        case ACP_QUANTIFIER_BROADCAST:
        case ACP_QUANTIFIER_SPECIFIC:
            break;
        default:
            return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_GET_FTS:
        case ACP_CMD_DS18B20_GET_RESOLUTION:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &i1l, udp_buf_size); //id
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_FTS:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < device_list.length; i++) {
                        getTemperature(&device_list.item[i]);
                        snprintf(q, sizeof q, "%d_%f_%ld_%ld_%d\n", device_list.item[i].id, device_list.item[i].value, device_list.item[i].tm.tv_sec, device_list.item[i].tm.tv_nsec, device_list.item[i].value_state);
                        if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Device *device = getDeviceById(i1l.item[i], &device_list);
                        if (device != NULL) {
                            getTemperature(device);
                            snprintf(q, sizeof q, "%d_%f_%ld_%ld_%d\n", device->id, device->value, device->tm.tv_sec, device->tm.tv_nsec, device->value_state);
                            if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                return;
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_DS18B20_GET_RESOLUTION:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < device_list.length; i++) {
                        getResolution(&device_list.item[i]);
                        snprintf(q, sizeof q, "%d_%d_%d\n", device_list.item[i].id, device_list.item[i].resolution, device_list.item[i].resolution_state);
                        if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Device *device = getDeviceById(i1l.item[i], &device_list);
                        if (device != NULL) {
                            getResolution(device);
                            snprintf(q, sizeof q, "%d_%d_%d\n", device->id, device->resolution, device->resolution_state);
                            if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                return;
                            }
                        }
                    }
                    break;
            }
            break;

    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_FTS:
        case ACP_CMD_DS18B20_GET_RESOLUTION:
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
    }
}

void initApp() {
#ifndef FILE_INI
    if (!readConf(CONFIG_FILE_DB, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }
#endif
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
#ifndef PLATFORM_ANY
    if (!gpioSetup()) {
        exit_nicely_e("initApp: failed to initialize GPIO\n");
    }
#endif
}

int initData() {
#ifndef FILE_INI
    if (!initDB(&db_conn_data, db_conninfo_data)) {
        return 0;
    }
#endif
    if (!initDevice(&device_list)) {
        FREE_LIST(&device_list);
#ifndef FILE_INI
        freeDB(db_conn_data);
#endif
        return 0;
    }
    if (!checkDevice(&device_list)) {
        FREE_LIST(&device_list);
#ifndef FILE_INI
        freeDB(db_conn_data);
#endif
        return 0;
    }
    i1l.item = (int *) malloc(udp_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        FREE_LIST(&device_list);
#ifndef FILE_INI
        freeDB(db_conn_data);
#endif
        return 0;
    }
#ifndef PLATFORM_ANY
    size_t i, j;
    for (i = 0; i < device_list.length; i++) {
        int found = 0;
        for (j = 0; j < i; j++) {
            if (device_list.item[j].pin == device_list.item[i].pin) {
                found = 1;
                break;
            }
        }
        if (!found) {//pin mode is not set yet for this pin
            pinModeOut(device_list.item[i].pin);
        }
    }
    for (i = 0; i < device_list.length; i++) {
        setResolution(&device_list.item[i], device_list.item[i].resolution);
    }
    for (i = 0; i < device_list.length; i++) {
        getResolution(&device_list.item[i]);
    }
#endif
    return 1;
}

void freeData() {
    FREE_LIST(&i1l);
    FREE_LIST(&device_list);
#ifndef FILE_INI
    freeDB(db_conn_data);
#endif
}

void freeApp() {
    freeData();
#ifndef PLATFORM_ANY
    gpioFree();
#endif
    freeSocketFd(&udp_fd);
#ifndef FILE_INI
    freeDB(db_conn_settings);
#endif
    freePid(&pid_file, &proc_id, pid_path);
}

void exit_nicely() {
    freeApp();
    puts("\nBye...");
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}

