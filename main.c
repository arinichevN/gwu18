/*
 * UPD gateway for DS18B20
 */

#include "main.h"

char app_class[NAME_SIZE];
char pid_path[LINE_SIZE];
char hostname[HOST_NAME_MAX];
int app_state = APP_INIT;
int pid_file = -1;
int proc_id = -1;
struct timespec cycle_duration = {0, 0};
int data_initialized = 0;
int udp_port = -1;
int udp_fd = -1; //udp socket file descriptor
size_t udp_buf_size = 0;
Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};

char db_conninfo_settings[LINE_SIZE];
char db_conninfo_log[LINE_SIZE];
char db_conninfo_data[LINE_SIZE];
PGconn *db_conn_settings = NULL;
PGconn *db_conn_log = NULL;
PGconn *db_conn_data = NULL;
PGconn **db_connp_log = NULL;
PGconn **db_connp_data = NULL;
int db_init_data = 0, db_init_log = 0;
ThreadData thread_data = {.cmd = ACP_CMD_APP_NO, .on = 0, .i1l =
    {NULL, 0}, .i2l =
    {NULL, 0}, .i3l =
    {NULL, 0}};
DeviceList device_list = {NULL, 0};


#include "util.c"
#include "print.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);
    memset(db_conninfo_log, 0, sizeof db_conninfo_log);

    snprintf(q, sizeof q, "select db_public from %s.config where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        fputs("readSettings: need only one tuple (1)\n", stderr);
        return 0;
    }
    char db_conninfo_public[LINE_SIZE];
    PGconn *db_conn_public = NULL;
    PGconn **db_connp_public = NULL;
    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), LINE_SIZE);
    PQclear(r);
    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_connp_public = &db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            return 0;
        }
        db_connp_public = &db_conn_public;
    }

    snprintf(q, sizeof q, "select udp_port, udp_buf_size, pid_path, db_log, db_data, cycle_duration_us from %s.config where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        freeDB(&db_conn_public);
        return 0;
    }
    if (PQntuples(r) == 1) {
        int done = 1;
        done = done && config_getUDPPort(*db_connp_public, PQgetvalue(r, 0, 0), &udp_port);
        done = done && config_getBufSize(*db_connp_public, PQgetvalue(r, 0, 1), &udp_buf_size);
        done = done && config_getPidPath(*db_connp_public, PQgetvalue(r, 0, 2), pid_path, LINE_SIZE);
        done = done && config_getDbConninfo(*db_connp_public, PQgetvalue(r, 0, 3), db_conninfo_log, LINE_SIZE);
        done = done && config_getDbConninfo(*db_connp_public, PQgetvalue(r, 0, 4), db_conninfo_data, LINE_SIZE);
        done = done && config_getCycleDurationUs(*db_connp_public, PQgetvalue(r, 0, 5), &cycle_duration);
        if (!done) {
            PQclear(r);
            freeDB(&db_conn_public);
            fputs("readSettings: failed to read some fields\n", stderr);
            return 0;
        }
    } else {
        PQclear(r);
        freeDB(&db_conn_public);
        fputs("readSettings: need only one tuple (2)\n", stderr);
        return 0;
    }
    PQclear(r);
    freeDB(&db_conn_public);
    db_init_data = 0;
    db_init_log = 0;
    if (dbConninfoEq(db_conninfo_data, db_conninfo_settings)) {
        db_connp_data = &db_conn_settings;
        //  puts("readSettings: db_data will not be initialized (will be settings)");
    } else {
        db_connp_data = &db_conn_data;
        db_init_data = 1;
    }
    if (dbConninfoEq(db_conninfo_log, db_conninfo_settings)) {
        db_connp_log = &db_conn_settings;
        //   puts("readSettings: db_log will not be initialized (will be settings)");
    } else if (dbConninfoEq(db_conninfo_log, db_conninfo_data)) {
        db_connp_log = &db_conn_data;
        //   puts("readSettings: db_log will not be initialized (will be data)");
    } else {
        db_connp_log = &db_conn_log;
        db_init_log = 1;
    }
    if (!(db_connp_data == &db_conn_settings || db_connp_log == &db_conn_settings)) {
        freeDB(&db_conn_settings);
    }
    return 1;
}

int initAppData(DeviceList *dl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select id, pin, addr, resolution from %s.device where app_class='%s' order by pin", APP_NAME, app_class);
    if ((r = dbGetDataT(*db_connp_data, q, "initAppData: select pin: ")) == NULL) {
        return 0;
    }
    dl->length = PQntuples(r);
    if (dl->length > 0) {
        dl->device = (Device *) malloc(dl->length * sizeof *(dl->device));
        if (dl->device == NULL) {
            dl->length = 0;
            fputs("initAppData: failed to allocate memory for pins\n", stderr);
            return 0;
        }
        for (i = 0; i < dl->length; i++) {
            dl->device[i].id = atoi(PQgetvalue(r, i, 0));
            dl->device[i].pin = atoi(PQgetvalue(r, i, 1));
            sscanf((char *) PQgetvalue(r, i, 2), "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", &dl->device[i].addr[0], &dl->device[i].addr[1], &dl->device[i].addr[2], &dl->device[i].addr[3], &dl->device[i].addr[4], &dl->device[i].addr[5], &dl->device[i].addr[6], &dl->device[i].addr[7]);
            dl->device[i].resolution = normalizeResolution(atoi(PQgetvalue(r, i, 3)));
            dl->device[i].value_state = 0;
            dl->device[i].resolution_state = 0;
            dl->device[i].resolution_set_state = 0;
            dl->device[i].log.state = OFF;
            dl->device[i].log.max_row = 0;
            timespecclear(&dl->device[i].log.interval);
            dl->device[i].log.timer.ready = 0;
        }
    }
    PQclear(r);
    return 1;
}

int checkAppData(DeviceList *dl) {
    size_t i, j;
    //valid pin address
    for (i = 0; i < dl->length; i++) {
        if (!checkPin(dl->device[i].pin)) {
            fprintf(stderr, "checkAppData: check device table: bad pin=%d where app_class='%s' and id=%d\n", dl->device[i].pin, app_class, dl->device[i].id);
            return 0;
        }
    }
    //unique id
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->device[i].id == dl->device[j].id) {
                fprintf(stderr, "checkAppData: check device table: ids should be unique, repetition found where id=%d and app_class='%s'\n", dl->device[i].id, app_class);
                return 0;
            }
        }
    }
    //unique addresses on certain pin
    for (i = 0; i < dl->length; i++) {
        for (j = i + 1; j < dl->length; j++) {
            if (dl->device[i].pin == dl->device[j].pin && memcmp(dl->device[i].addr, dl->device[j].addr, sizeof dl->device[i].addr) == 0) {
                fprintf(stderr, "checkAppData: check device table: addresses on certain pin should be unique, repetition found where id=%d and id=%d and app_class='%s'\n", dl->device[i].id, dl->device[j].id, app_class);
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
        fputs("serverRun: crc check failed\n", stderr);
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
                if (thread_data.on) {
                    waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
                }
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
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
        case ACP_CMD_LOG_STOP:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &thread_data.i1l, device_list.length); //id
                    if (thread_data.i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_DS18B20_SET_RESOLUTION:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    acp_parsePackI1(buf_in, &thread_data.i1l, device_list.length); //resolution
                    if (thread_data.i1l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI2(buf_in, &thread_data.i2l, device_list.length); //id, resolution
                    if (thread_data.i2l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_LOG_START:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    acp_parsePackI2(buf_in, &thread_data.i2l, device_list.length); //interval, max_num_row
                    if (thread_data.i2l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI3(buf_in, &thread_data.i3l, device_list.length); //id, interval, max_num_row
                    if (thread_data.i3l.length <= 0) {
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
        case ACP_CMD_DS18B20_GET_RESOLUTION:
        case ACP_CMD_LOG_STOP:
        case ACP_CMD_DS18B20_SET_RESOLUTION:
        case ACP_CMD_LOG_START:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            break;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_FTS:
        case ACP_CMD_DS18B20_GET_RESOLUTION:
            break;
        case ACP_CMD_DS18B20_SET_RESOLUTION:
            for (i = 0; i < device_list.length; i++) {
                saveResolution(&device_list.device[i]);
            }
            return;
        case ACP_CMD_LOG_STOP:
        case ACP_CMD_LOG_START:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_GET_FTS:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    for (i = 0; i < device_list.length; i++) {
                        snprintf(q, sizeof q, "%d_%f_%ld_%ld_%d\n", device_list.device[i].id, device_list.device[i].value, device_list.device[i].tm.tv_sec, device_list.device[i].tm.tv_nsec, device_list.device[i].value_state);
                        if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i1l.length; i++) {
                        Device *device = getDeviceById(thread_data.i1l.item[i], &device_list);
                        if (device != NULL) {
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
                        snprintf(q, sizeof q, "%d_%d_%d\n", device_list.device[i].id, device_list.device[i].resolution, device_list.device[i].resolution_state);
                        if (bufCat(buf_out, q, udp_buf_size) == NULL) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i1l.length; i++) {
                        Device *device = getDeviceById(thread_data.i1l.item[i], &device_list);
                        if (device != NULL) {
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

int saveLog(int dev_id, float temp, int track_limit, int year, int month, int mday, int hour, int min, int sec) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select log.do_real('%s', %d, %0.3f, %d, %d, %d, %d, %d, %d, %d)", app_class, dev_id, temp, track_limit, year, month, mday, hour, min, sec);
    int n = dbGetDataN(*db_connp_log, q, q);
    if (n != 0) {
        return 0;
    }
    return 1;
}

int clearLog(int dev_id) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "delete from log.v_real where app_class='%s' and dev_id=%d", app_class, dev_id);
    r = dbGetDataC(*db_connp_log, q, "clearLog: select logTemp(): ");
    if (r == NULL) {
        return 0;
    }
    PQclear(r);
    return 1;
}

void *threadFunction(void *arg) {
    ThreadData *data = (ThreadData *) arg;
    data->on = 1;
    size_t i, j;
    int r;
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &r) != 0) {
        perror("threadFunction: pthread_setcancelstate");
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &r) != 0) {
        perror("threadFunction: pthread_setcanceltype");
    }
    for (i = 0; i < device_list.length; i++) {
        int found = 0;
        for (j = 0; j < i; j++) {
            if (device_list.device[j].pin == device_list.device[i].pin) {
                found = 1;
                break;
            }
        }
        if (!found) {//pin mode is not set yet for this pin
            pinModeOut(device_list.device[i].pin);
        }
    }
    for (i = 0; i < device_list.length; i++) {
        setResolution(&device_list.device[i], device_list.device[i].resolution);
    }
    for (i = 0; i < device_list.length; i++) {
        getResolution(&device_list.device[i]);
    }
    while (1) {
        size_t i, j;
        struct timespec t1;
        clock_gettime(LIB_CLOCK, &t1);
        //temperature logger
        for (i = 0; i < device_list.length; i++) {
            switch (device_list.device[i].log.state) {
                case INIT:
                    device_list.device[i].log.state = WTIME;
                    clearLog(device_list.device[i].id);
                    break;
                case WTIME:
                    if (ton_ts(device_list.device[i].log.interval, &device_list.device[i].log.timer)) {
                        device_list.device[i].log.state = DO;
                    }
                    break;
                case DO:
                    getTemperature(&device_list.device[i]);
                    if (device_list.device[i].value_state) {
                        struct tm *now;
                        time_t now_u;
                        time(&now_u);
                        now = localtime(&now_u);
                        saveLog(device_list.device[i].id, device_list.device[i].value, device_list.device[i].log.max_row, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
                    }
                    device_list.device[i].log.state = WTIME;
                    break;
                case OFF:
                    break;
                default:
                    device_list.device[i].log.state = OFF;
                    break;
            }
        }
        //command checking
        switch (data->cmd) {
            case ACP_CMD_GET_FTS:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < device_list.length; i++) {
                            getTemperature(&device_list.device[i]);
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i1l.length; i++) {
                            Device *device = getDeviceById(data->i1l.item[i], &device_list);
                            if (device != NULL) {
                                getTemperature(device);
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_DS18B20_GET_RESOLUTION:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < device_list.length; i++) {
                            getResolution(&device_list.device[i]);
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i1l.length; i++) {
                            Device *device = getDeviceById(data->i1l.item[i], &device_list);
                            if (device != NULL) {
                                getResolution(device);
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_DS18B20_SET_RESOLUTION:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < device_list.length; i++) {
                            setResolutionC(&device_list.device[i], data->i1l.item[0]);
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i2l.length; i++) {
                            Device *device = getDeviceById(data->i2l.item[i].p0, &device_list);
                            if (device != NULL) {
                                setResolutionC(device, data->i2l.item[i].p1);
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_LOG_START:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        if (!checkIntervalAndNum(data->i2l.item[0].p0, data->i2l.item[0].p1)) {
                            break;
                        }
                        for (i = 0; i < device_list.length; i++) {
                            device_list.device[i].log.interval.tv_sec = data->i2l.item[0].p0;
                            device_list.device[i].log.max_row = data->i2l.item[0].p1;
                            device_list.device[i].log.timer.ready = 0;
                            device_list.device[i].log.state = INIT;
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i3l.length; i++) {
                            if (!checkIntervalAndNum(data->i3l.item[i].p1, data->i3l.item[i].p2)) {
                                break;
                            }
                            Device *device = getDeviceById(data->i3l.item[i].p0, &device_list);
                            if (device != NULL) {
                                device->log.interval.tv_sec = data->i3l.item[i].p1;
                                device->log.max_row = data->i3l.item[i].p2;
                                device->log.timer.ready = 0;
                                device->log.state = INIT;
                            }
                        }
                        break;
                }

                break;
            case ACP_CMD_LOG_STOP:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                        for (i = 0; i < device_list.length; i++) {
                            device_list.device[i].log.state = OFF;
                            device_list.device[i].log.timer.ready = 0;
                        }
                        break;
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i1l.length; i++) {
                            Device *device = getDeviceById(data->i1l.item[i], &device_list);
                            if (device != NULL) {
                                device->log.state = OFF;
                                device->log.timer.ready = 0;
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                data->cmd = ACP_CMD_APP_NO;
                data->on = 0;
                return (EXIT_SUCCESS);
        }
        data->cmd = ACP_CMD_APP_NO;
        sleepRest(data->cycle_duration, t1);
    }
}

int createThread(const DeviceList *dl, ThreadData *td, int buf_len) {

    //set attributes for each thread
    memset(&td->init_success, 0, sizeof td->init_success);
    if (pthread_attr_init(&td->thread_attr) != 0) {
        perror("createThread: pthread_attr_init");
        return 0;
    }
    td->init_success.thread_attr_init = 1;
    td->cycle_duration = cycle_duration;
    if (pthread_attr_setdetachstate(&td->thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("createThread: pthread_attr_setdetachstate");
        return 0;
    }
    td->init_success.thread_attr_setdetachstate = 1;
    td->i1l.item = malloc(buf_len * sizeof *(td->i1l.item));
    if (td->i1l.item == NULL) {
        perror("createThread: memory allocation for i1l failed");
        return 0;
    }
    td->i2l.item = malloc(buf_len * sizeof *(td->i2l.item));
    if (td->i2l.item == NULL) {
        perror("createThread: memory allocation for i2l failed");
        return 0;
    }
    td->i3l.item = malloc(buf_len * sizeof *(td->i3l.item));
    if (td->i3l.item == NULL) {
        perror("createThread: memory allocation for i3l failed");
        return 0;
    }
    //create a thread
    if (pthread_create(&td->thread, &td->thread_attr, threadFunction, (void *) td) != 0) {
        perror("createThread: pthread_create");
        return 0;
    }
    td->init_success.thread_create = 1;

    return 1;
}

void initApp() {
    readHostName(hostname);
    if (!readConf(CONF_FILE, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }
    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        exit_nicely_e("initApp: failed to initialize db\n");
    }
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

void initData() {
    data_initialized = 0;
    if (db_init_data) {
        if (!initDB(&db_conn_data, db_conninfo_data)) {
            return;
        }
    }
    if (db_init_log) {
        if (!initDB(&db_conn_log, db_conninfo_log)) {
            freeDB(&db_conn_data);
            return;
        }
    }
    if (!initAppData(&device_list)) {
        freeAppData();
        freeDB(&db_conn_log);
        freeDB(&db_conn_data);
        return;
    }
    if (!gpioSetup()) {
        freeAppData();
        freeDB(&db_conn_log);
        freeDB(&db_conn_data);
        return;
    }
    if (!checkAppData(&device_list)) {
        freeAppData();
        freeDB(&db_conn_log);
        freeDB(&db_conn_data);
        return;
    }

    if (!createThread(&device_list, &thread_data, device_list.length)) {
        freeThread();
        freeAppData();
        freeDB(&db_conn_log);
        freeDB(&db_conn_data);
        return;
    }
    data_initialized = 1;
}

void freeThread() {
    if (thread_data.init_success.thread_create) {
        if (thread_data.on) {
            char cmd[2];
            cmd[1] = ACP_CMD_APP_EXIT;
            waitThreadCmd(&thread_data.cmd, &thread_data.qfr, cmd);
        }
        if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
            perror("threadFunction: pthread_attr_destroy");
        }
    } else {
        if (thread_data.init_success.thread_attr_init) {
            if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
                perror("freeThread: pthread_attr_destroy");
            }
        }
    }
    free(thread_data.i1l.item);
    thread_data.i1l.item = NULL;
    free(thread_data.i2l.item);
    thread_data.i2l.item = NULL;
    free(thread_data.i3l.item);
    thread_data.i3l.item = NULL;
    thread_data.on = 0;
    thread_data.cmd = ACP_CMD_APP_NO;
}

void freeAppData() {
    free(device_list.device);
    device_list.device = NULL;
    device_list.length = 0;
}

void freeData() {
    freeThread();
    freeAppData();
    freeDB(&db_conn_log);
    freeDB(&db_conn_data);
    data_initialized = 0;
}

void freeApp() {
    freeData();
    freeSocketFd(&udp_fd);
    freeDB(&db_conn_settings);
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
    //#ifndef MODE_DEBUG
    daemon(0, 0);
    //#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    while (1) {
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
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

