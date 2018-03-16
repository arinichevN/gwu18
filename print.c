
void printData(ACPResponse *response) {
    DeviceList *list = &device_list;
    int i = 0;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONF_MAIN_FILE: %s\n", CONF_MAIN_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_DEVICE_FILE: %s\n", CONF_DEVICE_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_LCORRECTION_FILE: %s\n", CONF_LCORRECTION_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());

    SEND_STR(q)
    SEND_STR("+--------------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                   device                                                         |\n")
    SEND_STR("+----+----------------+----+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("| id |      addr      |pin |   value   | resolution| lcorr_ptr |value_state|  r_state  |r_set_state|\n")
    SEND_STR("+----+----------------+----+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    FORL {
        snprintf(q, sizeof q, "|%4d|%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx|%4d|%11f|%11d|%11p|%11d|%11d|%11d|\n",
                LIi.id,
                LIi.addr[0], LIi.addr[1], LIi.addr[2], LIi.addr[3], LIi.addr[4], LIi.addr[5], LIi.addr[6], LIi.addr[7],
                LIi.pin,
                LIi.result.value,
                LIi.resolution,
                (void *) LIi.lcorrection,
                LIi.result.state,
                LIi.resolution_state,
                LIi.resolution_set_state
                );
        SEND_STR(q)
    }
    SEND_STR("+----+----------------+----+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    acp_sendLCorrectionListInfo(&lcorrection_list, response, &peer_client);
    SEND_STR_L("-\n")
}

void printHelp(ACPResponse *response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget temperature in format: sensorId_temperature_timeSec_timeNsec_valid; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR_L(q)
}
