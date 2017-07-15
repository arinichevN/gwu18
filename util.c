
#include "main.h"

FUN_LIST_GET_BY_ID(Device)

int deviceIdExists(int id) {
    int i, found;
    found = 0;
    for (i = 0; i < device_list.length; i++) {
        if (id == device_list.item[i].id) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return 0;
    }

    return 1;
}

int checkResolution(int res) {
    if (!(res == 9 || res == 10 || res == 11 || res == 12)) {
        return 0;
    }
    return 1;
}

int requestResolutionParamValid(I2List *list) {
    int i;
    for (i = 0; i < list->length; i++) {
        if (!checkResolution(list->item[i].p1)) {
            return 0;
        }
    }
    return 1;
}

int normalizeResolution(int res) {
    if (!checkResolution(res)) {
        return 12;
    }
    return res;
}

int checkIntervalAndNum(int interval, int num) {
    if (interval < 0 || num < 0) {
        return 0;
    }
    return 1;
}

void setResolution(Device *item, int resolution) {
    int i;
    item->resolution_set_state = 0;
    int res = ds18b20_parse_resolution(resolution);
    if (res == -1) {
        return;
    }
    for (i = 0; i < item->retry_count; i++) {
#ifndef PLATFORM_ANY
        if (ds18b20_set_resolution(item->pin, item->addr, res)) {
            item->resolution_set_state = 1;
            return;
        }
#endif
#ifdef PLATFORM_ANY
        item->resolution_set_state = 1;
        return;
#endif
    }
}

void setResolutionC(Device *item, int resolution) {
    if (resolution == item->resolution) {
        return;
    }
    setResolution(item, resolution);
}

void getResolution(Device *item) {
    int i;
    item->resolution_state = 0;
    for (i = 0; i < item->retry_count; i++) {
#ifndef PLATFORM_ANY
        if (ds18b20_get_resolution(item->pin, item->addr, &item->resolution)) {
            item->resolution_state = 1;
            return;
        }
#endif
#ifdef PLATFORM_ANY
        item->resolution_state = 1;
        return;
#endif
    }
}

void getTemperature(Device *item) {
    int i;
    item->value_state = 0;
    for (i = 0; i < item->retry_count; i++) {
#ifndef PLATFORM_ANY
        if (ds18b20_get_temp(item->pin, item->addr, &item->value)) {
            item->tm = getCurrentTime();
            item->value_state = 1;
            return;
        }
#endif
#ifdef PLATFORM_ANY
        item->value = 0.0f;
        item->tm = getCurrentTime();
        item->value_state = 1;
        return;
#endif
    }
}

int sendStrPack(char qnf, char *cmd) {
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, &peer_client);
}

int sendBufPack(char *buf, char qnf, char *cmd_str) {
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}

int catTemperature(Device *item, char *buf, size_t buf_size) {
    if (!acp_catFTS(item->id, item->value, item->tm, item->value_state, buf, buf_size)) {
        sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
        return 0;
    }
    return 1;
}

int catResolution(Device *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR, item->id, item->resolution, item->resolution_state);
    if (bufCat(buf, q, buf_size) == NULL) {
        sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
        return 0;
    }
    return 1;
}
