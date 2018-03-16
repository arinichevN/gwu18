#include "lcorrection.h"

FUN_LIST_GET_BY_ID(LCorrection)


int initLCorrection(LCorrectionList *list, const char *config_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, config_path)) {
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
        LIi.id = TSVgetis(r, i, "id");
        LIi.factor = TSVgetfs(r, i, "factor");
        LIi.delta = TSVgetfs(r, i, "delta");
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

void lcorrect(float *out, LCorrection *x) {
    if (x == NULL) {
        return;
    }
    *out = *out * x->factor + x->delta;
}