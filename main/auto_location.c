#include "http_client_request.h"
#include "cJSON.h"
#include "auto_location.h"
#include "esp_system.h"

#define cpjs(field) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #field);\
        if (tmp) ret.field = str_new_pc(tmp->valuestring);\
    } while(0)

#define cpjss(field, source) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #source);\
        if (tmp) ret.field = str_new_pc(tmp->valuestring);\
    } while(0)

#define cpjsn(field, source, str) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #source);\
        if (tmp) ret.field = tmp->value##str;\
    } while(0)

/*
 *
 *
 http_client_response_t *location_json = http_client_request_get("http://ip-api.com/json");
 if (location_json) {
 cJSON *json = cJSON_Parse(str_c(location_json->body));
 if (json) {
 cpjs(country);
 cpjs(countryCode);
 cpjs(region);
 cpjs(regionName);
 cpjs(city);
 cpjs(zip);
 cpjs(timezone);
 cpjs(isp);
 cpjss(ip, query);
 cpjsn(lat, lat, double);
 cpjsn(lon, lon, double);
 cJSON_Delete(json);
 }
 }
 *
 *
 */

#define IPINFO_URL(TOKEN) "https://ipinfo.io?token=" TOKEN

WLocation_t auto_location() {
    WLocation_t ret;
    memset(&ret, 0, sizeof(ret));
    http_client_response_t *location_json = http_client_request_get(IPINFO_URL(CONFIG_ESP_IPINFO_TOKEN));
    if (location_json) {
        cJSON *json = cJSON_Parse(str_c(location_json->body));
        if (json) {
            cpjss(countryCode, country);
            cpjss(regionName, region);
            cpjs(city);
            cpjss(zip, postal);
            cpjs(timezone);
            cpjss(isp, org);
            cpjs(ip);
            cJSON_Delete(json);
        }
    }
    free(location_json->body);
    free(location_json);
    return ret;
}
