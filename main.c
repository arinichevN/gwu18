#include "main.h"

char pid_path[LINE_SIZE];
int app_state = APP_INIT;
int pid_file = -1;
int proc_id = -1;
int sock_port = -1;
int sock_fd = -1; //socket file descriptor
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

unsigned int retry_count = 0;
DeviceList device_list = {NULL, 0};

I1List i1l;

#include "util.c"
#include "print.c"
#include "init_f.c"

int checkDevice(DeviceList *dl) {
    size_t i, j;
    //valid pin address

    for (i = 0; i < dl->length; i++) {
        if (!checkPin(dl->item[i].pin)) {
            fprintf(stderr, "checkDevice: check device table: bad pin=%d where id=%d\n", dl->item[i].pin, dl->item[i].id);
            return 0;
        }
    }

    //retry_count
    for (i = 0; i < dl->length; i++) {
        if (dl->item[i].retry_count < 0) {
            fprintf(stderr, "checkDevice: check device table: bad retry_count=%d where id=%d\n", dl->item[i].retry_count, dl->item[i].id);
            return 0;
        }
    }
    //unique id
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->item[i].id == dl->item[j].id) {
                fprintf(stderr, "checkDevice: check device table: ids should be unique, repetition found where id=%d\n", dl->item[i].id);
                return 0;
            }
        }
    }
    //unique addresses on certain pin
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->item[i].pin == dl->item[j].pin && memcmp(dl->item[i].addr, dl->item[j].addr, sizeof dl->item[i].addr) == 0) {
                fprintf(stderr, "checkDevice: check device table: addresses on certain pin should be unique, repetition found where id=%d and id=%d\n", dl->item[i].id, dl->item[j].id);
                return 0;
            }
        }
    }
    return 1;
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
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
#ifdef MODE_DEBUG
    printf("initApp: \n\tCONFIG_FILE: %s, \n\tDEVICE_FILE: %s\n", CONFIG_FILE, DEVICE_FILE);
#endif
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize socket server\n");
    }
    if (!gpioSetup()) {
        exit_nicely_e("initApp: failed to initialize GPIO\n");
    }
}

int initData() {
    if (!initDevice(&device_list, retry_count)) {
        FREE_LIST(&device_list);
        return 0;
    }
    if (!checkDevice(&device_list)) {
        FREE_LIST(&device_list);
        return 0;
    }
    if (!initDeviceLCorrection(&device_list)) {
        ;
    }
    if (!initI1List(&i1l, device_list.length)) {
        FREE_LIST(&device_list);
        return 0;
    }
#ifndef CPU_ANY
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
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
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

