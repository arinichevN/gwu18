

Device *getDeviceById(int id, DeviceList *l) {
    int i;
    for (i = 0; i < l->length; i++) {
        if (id == l->device[i].id) {
            return &(l->device[i]);
        }
    }
    return NULL;
}

int deviceIdExists(int id) {
    int i, found;
    found = 0;
    for (i = 0; i < device_list.length; i++) {
        if (id == device_list.device[i].id) {
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
    for (i = 0; i < RETRY_NUM; i++) {
#ifdef P_A20
        if (ds18b20_set_resolution(item->pin, item->addr, res)) {
            item->resolution_set_state = 1;
            return;
        }
#endif
#ifdef P_ALL
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
    for (i = 0; i < RETRY_NUM; i++) {
#ifdef P_A20
        if (ds18b20_get_resolution(item->pin, item->addr, &item->resolution)) {
            item->resolution_state = 1;
            return;
        }
#endif
#ifdef P_ALL
        item->resolution_state = 1;
        return;
#endif
    }
}

void getTemperature(Device *item) {
    int i;
    item->value_state = 0;
    for (i = 0; i < RETRY_NUM; i++) {
#ifdef P_A20
        if (ds18b20_get_temp(item->pin, item->addr, &item->value)) {
            clock_gettime(LIB_CLOCK, &item->tm);
            item->value_state = 1;
            return;
        }
#endif
#ifdef P_ALL
        item->value = 0.0f;
        clock_gettime(LIB_CLOCK, &item->tm);
        item->value_state = 1;
        return;
#endif
    }
}

void saveResolution(Device *item) {
    if (item->resolution_set_state) {
        PGresult *r;
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "update %s.device set resolution=%d where app_class='%s' and id=%d", APP_NAME, item->resolution, app_class, item->id);
        if ((r = dbGetDataC(*db_connp_data, q, "backupDelProgById: delete from backup: ")) != NULL) {
            PQclear(r);
        }
        item->resolution_set_state = 0;
    }
}

int sendStrPack(char qnf, const char *cmd) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, udp_buf_size, &peer_client);
}

int sendBufPack(char *buf, char qnf, const char *cmd_str) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, udp_buf_size, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}
