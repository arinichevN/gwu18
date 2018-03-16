#include <string.h>

#include "main.h"

int readSettings(int *sock_port, unsigned int *retry_count, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    int p = TSVgetis(r, 0, "port");
    unsigned int c = (unsigned int) TSVgetis(r, 0, "retry_count");
    if (TSVnullreturned(r)) {
        return 0;
    }
    TSVclear(r);
    *sock_port = p;
    *retry_count = c;
    return 1;
}

static int parseAddress(uint8_t *address, char *address_str) {
    int n = sscanf(address_str, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5], &address[6], &address[7]);
    if (n != 8) {
        return 0;
    }
    return 1;
}

int initDevice(DeviceList *list,unsigned int retry_count, LCorrectionList *lcl, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    int n = TSVntuples(r);
    if (n <= 0) {
        TSVclear(r);
        return 1;
    }
    RESIZE_M_LIST(list, n);
    NULL_LIST(list);
    if (LML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing list\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    for (int i = 0; i < LML; i++) {
        LIi.id = LIi.result.id =TSVgetis(r, i, "id");
        LIi.pin = TSVgetis(r, i, "pin");
        LIi.resolution = TSVgetis(r, i, "resolution");
        int lcorrection_id = TSVgetis(r, i, "lcorrection_id");
        LIi.lcorrection = getLCorrectionById(lcorrection_id, lcl);
        LIi.retry_count=retry_count;
        if (!parseAddress(LIi.addr, TSVgetvalues(r, i, "address"))) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): bad address where device_id=%d\n", F, LIi.id);
#endif
            break;
        }
        if (TSVnullreturned(r)) {
            break;
        }
        LL++;
    }
    TSVclear(r);
    if (LL != LML) {
#ifdef MODE_DEBUG

        fprintf(stderr, "%s(): failure while reading rows\n", F);
#endif
        return 0;
    }
    return 1;
}
