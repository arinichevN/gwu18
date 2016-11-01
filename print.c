
void printData(DeviceList *dl) {
    int i = 0;
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("+----------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                             device                                                 |\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|     id    |      addr      |    pin    |   value   | resolution|value_state|  r_state  |r_set_state|\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < dl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx|%11d|%11f|%11d|%11d|%11d|%11d|\n",
                dl->device[i].id,
                dl->device[i].addr[0], dl->device[i].addr[1], dl->device[i].addr[2], dl->device[i].addr[3], dl->device[i].addr[4], dl->device[i].addr[5], dl->device[i].addr[6], dl->device[i].addr[7],
                dl->device[i].pin,
                dl->device[i].value,
                dl->device[i].resolution,
                dl->device[i].value_state,
                dl->device[i].resolution_state,
                dl->device[i].resolution_set_state);
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
    snprintf(q, sizeof q, "%c\tset sensor resolution; resolution expected; program id expected if '.' quantifier is used\n", ACP_CMD_DS18B20_SET_RESOLUTION);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tstart temperature registration; intervalSec and maxRows expected; program id expected if '.' quantifier is used\n", ACP_CMD_LOG_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tstop temperature registration; program id expected if '.' quantifier is used\n", ACP_CMD_LOG_STOP);
    sendStr(q, &crc);
    sendFooter(crc);
}
