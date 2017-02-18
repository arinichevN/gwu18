int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);
    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        return 0;
    }
    snprintf(q, sizeof q, "select db_public from " APP_NAME_STR ".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        freeDB(db_conn_settings);
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        fputs("readSettings: need only one tuple (1)\n", stderr);
        freeDB(db_conn_settings);
        return 0;
    }
    char db_conninfo_public[LINE_SIZE];
    PGconn *db_conn_public = NULL;
    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), sizeof db_conninfo_public);
    PQclear(r);

    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_conn_public = db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            freeDB(db_conn_settings);
            return 0;
        }
    }

    snprintf(q, sizeof q, "select udp_port, udp_buf_size, pid_path, db_data from " APP_NAME_STR ".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, "readSettings: select: ")) == NULL) {
        freeDB(db_conn_public);
        freeDB(db_conn_settings);
        return 0;
    }
    if (PQntuples(r) == 1) {
        int done = 1;
        done = done && config_getUDPPort(db_conn_public, PQgetvalue(r, 0, 0), &udp_port);
        done = done && config_getBufSize(db_conn_public, PQgetvalue(r, 0, 1), &udp_buf_size);
        done = done && config_getPidPath(db_conn_public, PQgetvalue(r, 0, 2), pid_path, LINE_SIZE);
        done = done && config_getDbConninfo(db_conn_public, PQgetvalue(r, 0, 3), db_conninfo_data, LINE_SIZE);
        if (!done) {
            PQclear(r);
            freeDB(db_conn_public);
            freeDB(db_conn_settings);
            fputs("readSettings: failed to read some fields\n", stderr);
            return 0;
        }
    } else {
        PQclear(r);
        freeDB(db_conn_public);
        freeDB(db_conn_settings);
        fputs("readSettings: need only one tuple (2)\n", stderr);
        return 0;
    }
    PQclear(r);
    freeDB(db_conn_public);
    freeDB(db_conn_settings);
    return 1;
}

int initDevice(DeviceList *dl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select id, pin, addr, resolution from " APP_NAME_STR ".device where app_class='%s' order by pin", app_class);
    if ((r = dbGetDataT(db_conn_data, q, "initAppData: select pin: ")) == NULL) {
#ifdef MODE_DEBUG
        fputs("ERROR: initDevice: database error\n", stderr);
#endif
        return 0;
    }
    dl->length = PQntuples(r);
    if (dl->length > 0) {
        dl->item = (Device *) malloc(dl->length * sizeof *(dl->item));
        if (dl->item == NULL) {
            dl->length = 0;
#ifdef MODE_DEBUG
            fputs("ERROR: initDevice: failed to allocate memory for pins\n", stderr);
#endif
            PQclear(r);
            return 0;
        }
        for (i = 0; i < dl->length; i++) {
            dl->item[i].id = atoi(PQgetvalue(r, i, 0));
            dl->item[i].pin = atoi(PQgetvalue(r, i, 1));
            sscanf((char *) PQgetvalue(r, i, 2), "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", &dl->item[i].addr[0], &dl->item[i].addr[1], &dl->item[i].addr[2], &dl->item[i].addr[3], &dl->item[i].addr[4], &dl->item[i].addr[5], &dl->item[i].addr[6], &dl->item[i].addr[7]);
            dl->item[i].resolution = normalizeResolution(atoi(PQgetvalue(r, i, 3)));
            dl->item[i].value_state = 0;
            dl->item[i].resolution_state = 0;
            dl->item[i].resolution_set_state = 0;
        }
    }
    PQclear(r);
    return 1;
}

