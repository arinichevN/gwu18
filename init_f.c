#include <string.h>

int readSettings() {
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: fopen", stderr);
#endif
        return 0;
    }

    int n;
    char s[LINE_SIZE];
    fgets(s, LINE_SIZE, stream);
    n = fscanf(stream, "%d\t%255s\t%d\t%d", &sock_port, pid_path, &sock_buf_size, &retry_count);
    if (n != 4) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: sock_port: %d, pid_path: %s, sock_buf_size: %d\n", sock_port, pid_path, sock_buf_size);
#endif
    return 1;
}

#define DEVICE_ROW_FORMAT "%d\t%d\t%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx\t%d\n"
#define DEVICE_FIELD_COUNT 11
int initDevice(DeviceList *list, int retry_count) {
    FILE* stream = fopen(DEVICE_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        fputs("ERROR: initDevice: fopen", stderr);
#endif
        return 0;
    }
        char s[LINE_SIZE];
    fgets(s, LINE_SIZE, stream);
    int rnum = 0;
    while (1) {
        int n = 0, x1, x2, x4;
        uint8_t x3[DS18B20_SCRATCHPAD_BYTE_NUM];
        n = fscanf(stream, DEVICE_ROW_FORMAT, &x1, &x2, &x3[0], &x3[1], &x3[2], &x3[3], &x3[4], &x3[5], &x3[6], &x3[7], &x4);
        if (n != DEVICE_FIELD_COUNT) {
            break;
        }
#ifdef MODE_DEBUG
        printf("initDevice: count: id = %d, pin = %d, resolution = %d\n", x1, x2, x4);
#endif
        rnum++;

    }
    rewind(stream);

    list->length = rnum;
    if (list->length > 0) {
        list->item = (Device *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
#ifdef MODE_DEBUG
            fputs("ERROR: initDevice: failed to allocate memory for pins\n", stderr);
#endif
            fclose(stream);
            return 0;
        }
        fgets(s, LINE_SIZE, stream);
        int done = 1;
        size_t i;
        FORL{
            int n, resolution;
            n = fscanf(stream, DEVICE_ROW_FORMAT,
            &LIi.id,
            &LIi.pin,
            &LIi.addr[0], &LIi.addr[1], &LIi.addr[2], &LIi.addr[3], &LIi.addr[4], &LIi.addr[5], &LIi.addr[6], &LIi.addr[7],
            &resolution
            );
            if (n != DEVICE_FIELD_COUNT) {
                done = 0;
            }
            LIi.resolution = normalizeResolution(resolution);
#ifdef MODE_DEBUG
            printf("initDevice: read: id = %d, pin = %d, resolution = %d\n", LIi.id, LIi.pin, LIi.resolution);
#endif
            LIi.value_state = 0;
            LIi.resolution_state = 0;
            LIi.resolution_set_state = 0;
            LIi.retry_count=retry_count;
        }
        if (!done) {
            FREE_LIST(list);
            fclose(stream);
#ifdef MODE_DEBUG
            fputs("ERROR: initDevice: failure while reading rows\n", stderr);
#endif
            return 0;
        }
    }
    fclose(stream);
    return 1;
}
