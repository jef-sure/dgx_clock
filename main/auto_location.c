#include "http_client_request.h"
#include "cJSON.h"
#include "auto_location.h"

#define cpjs(field) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #field);\
        if (tmp) ret.field = str_new_pc(tmp->valuestring);\
    } while(0)

#define cpjss(field, source, str) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #source);\
        if (tmp) ret.field = str_new_pc(tmp->value##str);\
    } while(0)

#define cpjsn(field, source, str) \
    do {\
        cJSON *tmp = cJSON_GetObjectItem(json, #source);\
        if (tmp) ret.field = tmp->value##str;\
    } while(0)

WLocation_t auto_location() {
    WLocation_t ret;
    memset(&ret, 0, sizeof(ret));
    http_client_response_t *location_json = http_client_request_get("http://ip-api.com/json");
    // {"status":"success","country":"Germany","countryCode":"DE","region":"RP","regionName":"Rheinland-Pfalz","city":"Neustadt","zip":"67433","lat":49.3439,"lon":8.1509,"timezone":"Europe/Berlin","isp":"Deutsche Telekom AG","org":"Deutsche Telekom AG","as":"AS3320 Deutsche Telekom AG","query":"79.199.66.249"}
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
            cpjss(ip, query, string);
            cpjsn(lat, lat, double);
            cpjsn(lon, lon, double);
            cJSON_Delete(json);
        }
    }
    free(location_json->body);
    free(location_json);
    return ret;
}
