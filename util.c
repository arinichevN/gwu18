
#include "main.h"



int checkResolution(int res) {
    if (!(res == 9 || res == 10 || res == 11 || res == 12)) {
        return 0;
    }
    return 1;
}

int checkDevice(DeviceList *dl) {
    int valid = 1;
    for (size_t i = 0; i < dl->length; i++) {
        if (!checkPin(dl->item[i].pin)) {
            fprintf(stderr, "%s(): check device table: bad pin=%d where id=%d\n", F, dl->item[i].pin, dl->item[i].id);
            valid = 0;
        }
        if (dl->item[i].retry_count < 0) {
            fprintf(stderr, "%s(): check device table: bad retry_count=%d where id=%d\n", F, dl->item[i].retry_count, dl->item[i].id);
            valid = 0;
        }
        //unique id
        for (size_t j = i + 1; j < dl->length; j++) {
            if (dl->item[i].id == dl->item[j].id) {
                fprintf(stderr, "%s(): check device table: ids should be unique, repetition found where id=%d\n", F, dl->item[i].id);
                valid = 0;
            }
        }
        //unique addresses on certain pin
        for (size_t j = i + 1; j < dl->length; j++) {
            if (dl->item[i].pin == dl->item[j].pin && memcmp(dl->item[i].addr, dl->item[j].addr, sizeof dl->item[i].addr) == 0) {
                fprintf(stderr, "%s(): check device table: addresses on certain pin should be unique, repetition found where id=%d and id=%d\n", F, dl->item[i].id, dl->item[j].id);
                valid = 0;
            }
        }
    }
    return valid;
}

int requestResolutionParamValid(I2List *list) {
    FORLi {
        if (!checkResolution(LIi.p1)) {
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
    item->resolution_set_state = 0;
    int res = ds18b20_parse_resolution(resolution);
    if (res == -1) {
        return;
    }
    for (int i = 0; i < item->retry_count; i++) {
        if (ds18b20_set_resolution(item->pin, item->addr, res)) {
            item->resolution_set_state = 1;
            return;
        }
    }
}

void setResolutionC(Device *item, int resolution) {
    if (resolution == item->resolution) {
        return;
    }
    setResolution(item, resolution);
}

void getResolution(Device *item) {
    item->resolution_state = 0;
    for (int i = 0; i < item->retry_count; i++) {
        if (ds18b20_get_resolution(item->pin, item->addr, &item->resolution)) {
            item->resolution_state = 1;
            return;
        }
    }
}


void getTemperature(Device *item) {
#ifdef CPU_ANY
    item->result.value = 0.0f;
    item->result.tm = getCurrentTime();
    item->result.state = 1;
    lcorrect(&item->result.value, item->lcorrection);
    return;
#endif
    item->result.state = 0;
    for (int i = 0; i < item->retry_count; i++) {
        if (ds18b20_get_temp(item->pin, item->addr, &item->result.value)) {
            item->result.tm = getCurrentTime();
            item->result.state = 1;
            lcorrect(&item->result.value, item->lcorrection);
            break;
        }
    }
}

int catTemperature(Device *item, ACPResponse *response) {
    return acp_responseFTSCat(item->id, item->result.value, item->result.tm, item->result.state, response);
}

int catResolution(Device *item, ACPResponse *response) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR, item->id, item->resolution, item->resolution_state);
    return acp_responseStrCat(response, q);
}

int initHardware(DeviceList *list){
	FORLi {
        int found = 0;
        for (size_t j = 0; j < i; j++) {
            if (LIj.pin == LIi.pin) {
                found = 1;
                break;
            }
        }
        if (!found) {//pin mode is not set yet for this pin
            pinModeOut(LIi.pin);
        }
    }
    FORLi {
        setResolution(&LIi, LIi.resolution);
    }
    FORLi {
        getResolution(&LIi);
    }
    return 1;
}
