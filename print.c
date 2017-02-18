
void printData(DeviceList *list) {
    int i = 0;
    char q[LINE_SIZE];
    uint8_t crc = 0;
    snprintf(q, sizeof q, "pid path: %s\n", pid_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    sendStr(q, &crc);
    snprintf(q, sizeof q, "PID: %d\n", proc_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "udp port: %d\n", udp_port);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db config file: %s\n", CONFIG_FILE_DB);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "tsv config file: %s\n", CONFIG_FILE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "tsv device file: %s\n", DEVICE_FILE);
    sendStr(q, &crc);
    sendStr("+----------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                             device                                                 |\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    |      addr      |    pin    |   value   | resolution|value_state|  r_state  |r_set_state|\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    FORL {
        snprintf(q, sizeof q, "|%11d|%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx|%11d|%11f|%11d|%11d|%11d|%11d|\n",
               LIi.id,
               LIi.addr[0],LIi.addr[1],LIi.addr[2],LIi.addr[3],LIi.addr[4],LIi.addr[5],LIi.addr[6],LIi.addr[7],
               LIi.pin,
               LIi.value,
               LIi.resolution,
               LIi.value_state,
               LIi.resolution_state,
               LIi.resolution_set_state);
        sendStr(q, &crc);
    }
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendFooter(crc);
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);
    
    snprintf(q, sizeof q, "%c\tget temperature in format: sensorId_temperature_timeSec_timeNsec_valid; program id expected if '.' quantifier is used\n", ACP_CMD_GET_FTS);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget sensor resolution in format: sensorId_resolution_valid; program id expected if '.' quantifier is used\n", ACP_CMD_DS18B20_GET_RESOLUTION);
    sendStr(q, &crc);
    sendFooter(crc);
}
