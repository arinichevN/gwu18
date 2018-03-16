#include "main.h"

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

unsigned int retry_count = 0;
LCorrectionList lcorrection_list = LIST_INITIALIZER;
DeviceList device_list = LIST_INITIALIZER;

#include "util.c"
#include "print.c"
#include "init_f.c"

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
        acp_requestDataToI1List(&request, &i1l); //id
        if (i1l.length <= 0) {
            return;
        }
        for (int i = 0; i < i1l.length; i++) {
            Device *device = getDeviceById(i1l.item[i], &device_list);
            if (device != NULL) {
                getTemperature(device);
                if (!catTemperature(device, &response)) {
                    return;
                }
            }
        }
    }
    acp_responseSend(&response, &peer_client);
}

void initApp() {
    if (!readSettings(&sock_port, &retry_count, CONF_MAIN_FILE)) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize socket server\n");
    }
    if (!gpioSetup()) {
        exit_nicely_e("initApp: failed to initialize GPIO\n");
    }
}

int initData() {
    initLCorrection(&lcorrection_list, CONF_LCORRECTION_FILE);
    if (!initDevice(&device_list, retry_count, &lcorrection_list, CONF_DEVICE_FILE)) {
        FREE_LIST(&device_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
    if (!checkDevice(&device_list)) {
        FREE_LIST(&device_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
#ifdef CPU_ANY
    puts("return 1;");
    return 1;
#endif
    for (size_t i = 0; i < device_list.length; i++) {
        int found = 0;
        for (size_t j = 0; j < i; j++) {
            if (device_list.item[j].pin == device_list.item[i].pin) {
                found = 1;
                break;
            }
        }
        if (!found) {//pin mode is not set yet for this pin
            pinModeOut(device_list.item[i].pin);
        }
    }
    for (size_t i = 0; i < device_list.length; i++) {
        setResolution(&device_list.item[i], device_list.item[i].resolution);
    }
    for (size_t i = 0; i < device_list.length; i++) {
        getResolution(&device_list.item[i]);
    }
    return 1;
}

void freeData() {
    FREE_LIST(&device_list);
    FREE_LIST(&lcorrection_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
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
    if (geteuid() != 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s: root user expected\n", APP_NAME_STR);
#endif
        return (EXIT_FAILURE);
    }
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
#ifdef MODE_DEBUG
        printf("%s(): %s %d\n", F, getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
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

