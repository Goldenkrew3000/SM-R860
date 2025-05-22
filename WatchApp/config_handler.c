#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config_handler.h"

const char* configFilename = "config.json";
static char buf_configFile[4096];

char* config_timezone;
char* config_timeFormat;
char* config_wifiStaSSID;
char* config_wifiStaPass;
char* config_wifiApSSID;
char* config_wifiApPass;
char* config_ntpServer;
char* config_ntpPort;

// Read config file (1 = Success, 2 = Failure)
int config_handler_read() {
    printf("%s +\n", __func__);

    // Open config file
    FILE *fp_configFile = fopen(configFilename, "r");
    if (fp_configFile == NULL) {
        printf("ERROR: Could not open config file.\n");
        fclose(fp_configFile);
        printf("%s -\n", __func__);
        return 2;
    }

    // Determine size of config file to avoid buffer overflow
    fseek(fp_configFile, 0, SEEK_END);
    long fs_configFile = ftell(fp_configFile);
    fseek(fp_configFile, 0, SEEK_SET);
    if (fs_configFile >= 4096) {
        printf("ERROR: Config file exceeds allowed size (%ld bytes).\n", sizeof(buf_configFile));
        fclose(fp_configFile);
        printf("%s -\n", __func__);
        return 2;
    }

    // Read config file into buffer
    fread(buf_configFile, 1, sizeof(buf_configFile), fp_configFile);
    fclose(fp_configFile);

    // Parse config json
    cJSON* jobj_configFile = cJSON_Parse(buf_configFile);
    if (jobj_configFile == NULL) {
        const char* jerr_ptr = cJSON_GetErrorPtr();
        if (jerr_ptr != NULL) {
            printf("ERROR: Could not parse config JSON (%s).\n", jerr_ptr);
        } else {
            printf("ERROR: Could not parse config JSON (Undefined error).\n");
        }
        cJSON_Delete(jobj_configFile);
        printf("%s -\n", __func__);
        return 2;
    }

    // Read in config information
    // Nested roots
    cJSON* jobj_wifi_root = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "wifi");
    cJSON* jobj_ntp_root = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "ntp");

    // Objects
    cJSON* jobj_chargeAnimOuterRing = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "charging_animation_outer_ring");
    cJSON* jobj_timezone = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "timezone");
    cJSON* jobj_timeFormat = cJSON_GetObjectItemCaseSensitive(jobj_configFile, "time_format");
   
    // NOTE: I am dynamically allocating the memory here so C doesn't wipe these from the stack (It was...)
    config_timezone = (char*)malloc(strlen(jobj_timezone->valuestring));
    memcpy(config_timezone, jobj_timezone->valuestring, strlen(jobj_timezone->valuestring));
    config_timeFormat = (char*)malloc(strlen(jobj_timeFormat->valuestring));
    memcpy(config_timeFormat, jobj_timeFormat->valuestring, strlen(jobj_timeFormat->valuestring));

    cJSON* jobj_wifiStaSSID = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "sta_ssid");
    cJSON* jobj_wifiStaPass = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "sta_pass");
    cJSON* jobj_wifiApSSID = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "ap_ssid");
    cJSON* jobj_wifiApPass = cJSON_GetObjectItemCaseSensitive(jobj_wifi_root, "ap_pass");
    config_wifiStaSSID = (char*)malloc(strlen(jobj_wifiStaSSID->valuestring));
    memcpy(config_wifiStaSSID, jobj_wifiStaSSID->valuestring, strlen(jobj_wifiStaSSID->valuestring));
    config_wifiStaPass = (char*)malloc(strlen(jobj_wifiStaPass->valuestring));
    memcpy(config_wifiStaPass, jobj_wifiStaPass->valuestring, strlen(jobj_wifiStaPass->valuestring));
    config_wifiApSSID = (char*)malloc(strlen(jobj_wifiApSSID->valuestring));
    memcpy(config_wifiApSSID, jobj_wifiApSSID->valuestring, strlen(jobj_wifiApSSID->valuestring));
    config_wifiApPass = (char*)malloc(strlen(jobj_wifiApPass->valuestring));
    memcpy(config_wifiApPass, jobj_wifiApPass->valuestring, strlen(jobj_wifiApPass->valuestring));

    cJSON* jobj_ntpServer = cJSON_GetObjectItemCaseSensitive(jobj_ntp_root, "server");
    cJSON* jobj_ntpPort = cJSON_GetObjectItemCaseSensitive(jobj_ntp_root, "port");
    config_ntpServer = (char*)malloc(strlen(jobj_ntpServer->valuestring));
    memcpy(config_ntpServer, jobj_ntpServer->valuestring, strlen(jobj_ntpServer->valuestring));
    config_ntpPort = (char*)malloc(strlen(jobj_ntpPort->valuestring));
    memcpy(config_ntpPort, jobj_ntpPort->valuestring, strlen(jobj_ntpPort->valuestring));

    // Cleanup json objects
    cJSON_Delete(jobj_configFile);

    printf("%s -\n", __func__);
}
