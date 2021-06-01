#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_heap_caps_init.h"

#include <string.h>
#include <sys/time.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"

#include "dgx_screen.h"
#include "dgx_spi_screen.h"
#include "dgx_bw_screen.h"
#include "dgx_v_screen.h"
#include "dgx_interscreen.h"
#include "drivers/spi_st7735.h"
#include "dgx_draw.h"
#include "fonts/IosevkaMedium18.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/UbuntuCondensedRegular15.h"
#include "fonts/UbuntuCondensedRegular32.h"
#include "fonts/WeatherIconsRegular24.h"

#include "tzones.h"
#include "str.h"
#include "auto_location.h"
#include "weather.h"
#include "http_client_request.h"

#define PIN_NUM_MOSI 23 // SDA
#define PIN_NUM_CLK  18 // SCL
#define PIN_NUM_CS   17
#define PIN_NUM_DC   2 // A0
#define PIN_NUM_RST  27

#define DEFAULT_SSID CONFIG_ESP_WIFI_SSID
#define DEFAULT_PWD CONFIG_ESP_WIFI_PASSWORD
#define WEATHER_API_KEY "192834791823479238147123847"

/* Initialize Wi-Fi as sta and set scan method */

#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#define DEFAULT_RSSI -127

#define WHITE12 dgx_rgb_to_16(255,255,255)
#define RED12 dgx_rgb_to_16(255,0,64)

#include "math.h"

SemaphoreHandle_t xSemLocation = NULL;
WLocation_t Location;

static const char *TAG = "scan";

bool is_got_ip = false;

struct shared_screen {
    dgx_screen_t *src_scr, *dst_scr, *bak_scr;
    QueueHandle_t queue;
};

typedef enum {
    Backgroud, ClockHands, Weather, Writing, CityName, CurrentDate
} scr_type_t;

typedef struct task_message_ {
    scr_type_t type;
    int16_t dst_x, dst_y;
    dgx_screen_t *result_scr;
} task_message_t;

static inline int16_t symround(float f) {
    f += f < 0 ? 0.5f : -0.5f;
    return f;
}

//static inline float symroundf(float f) {
//    if (f > 0) f += 0.5f;
//    return floorf(f);
//}

void drawClockHand(dgx_screen_t *scr, int16_t x, int16_t y, int16_t r, float rad, uint32_t color, int8_t thickness) {
    int16_t clsx = symround(r * cos(rad));
    int16_t clsy = symround(r * sin(rad));
    if (thickness > 1) {
        float lx = cos(rad - M_PI_2);
        float ly = sin(rad - M_PI_2);
        float rx = cos(rad + M_PI_2);
        float ry = sin(rad + M_PI_2);
        if (thickness > 2) {
            dgx_draw_line_float(scr, x + 3 * lx, y + 3 * ly, clsx + x, clsy + y, color & 0xbdf7);
            dgx_draw_line_float(scr, x + 3 * rx, y + 3 * ry, clsx + x, clsy + y, color & 0xbdf7);
        }
        dgx_draw_line_float(scr, x + 2 * lx, y + 2 * ly, clsx + x, clsy + y, color);
        dgx_draw_line_float(scr, x + 2 * rx, y + 2 * ry, clsx + x, clsy + y, color);
        dgx_draw_line_float(scr, x + lx, y + ly, clsx + x, clsy + y, color);
        dgx_draw_line_float(scr, x + rx, y + ry, clsx + x, clsy + y, color);
    }
    dgx_draw_line_float(scr, x, y, clsx + x, clsy + y, color);
    if (thickness > 1) {
        float clhx = clsx / 3.0;
        float clhy = clsy / 3.0;
        dgx_draw_line_float(scr, x, y, clhx + x, clhy + y, color & 0x7bef);

    }
}
void drawClockFace(dgx_screen_t *scr, int16_t x, int16_t y, int16_t r1, int16_t r2) {
    for (int angle = 0; angle < 360; angle += 30) {
        float rad = angle * M_PI / 180;
        int16_t clsx = symround(r1 * cos(rad));
        int16_t clsy = symround(r1 * sin(rad));
        int16_t clex = symround(r2 * cos(rad));
        int16_t cley = symround(r2 * sin(rad));
        dgx_draw_line(scr, clsx + x, clsy + y, clex + x, cley + y, WHITE12);
    }
}
void drawClockHands(dgx_screen_t *scr, int16_t x, int16_t y, int16_t r1) {
    struct timeval now;
    gettimeofday(&now, 0);
    struct tm timeinfo;
    localtime_r(&now.tv_sec, &timeinfo);
    float sec_angle = timeinfo.tm_sec * M_PI / 30;
    float min_angle = timeinfo.tm_min * M_PI / 30 + sec_angle / 60;
    float hour_angle = timeinfo.tm_hour * M_PI / 6 + min_angle / 12;
    uint32_t hours_color = dgx_rgb_to_16(255, 255, 192);
    drawClockHand(scr, x, y, r1 - 12, hour_angle - M_PI_2, hours_color, 3);
    drawClockHand(scr, x, y, r1 - 3, min_angle - M_PI_2, WHITE12, 2);
    drawClockHand(scr, x, y, r1 + 2, sec_angle - M_PI_2, RED12, 1);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_got_ip = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        is_got_ip = true;
    }
}

static void fast_scan() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    // Initialize default station as network interface instance (esp-netif)
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Initialize and start WiFi
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char*) wifi_config.sta.ssid, DEFAULT_SSID);
    strcpy((char*) wifi_config.sta.password, DEFAULT_PWD);
    wifi_config.sta.scan_method = DEFAULT_SCAN_METHOD;
    wifi_config.sta.sort_method = DEFAULT_SORT_METHOD;
    wifi_config.sta.threshold.rssi = DEFAULT_RSSI;
    wifi_config.sta.threshold.authmode = DEFAULT_AUTHMODE;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void vTaskDrawClockHands(void *pvParameters) {
    struct shared_screen *shscr = pvParameters;
    dgx_screen_t clock_scr;
    dgx_v_init(&clock_scr, 80, 80, shscr->src_scr->color_bits);
    dgx_screen_t date_scr;
    dgx_v_init(&date_scr, shscr->bak_scr->width, 30, shscr->src_scr->color_bits);
    int16_t src_x = shscr->src_scr->width / 2 - clock_scr.width / 2;
    int16_t src_y = shscr->src_scr->height / 2 - clock_scr.height / 2;
    task_message_t clock_message = { .type = ClockHands, .dst_x = src_x, .dst_y = src_y, .result_scr = &clock_scr };
    task_message_t date_message = { .type = CurrentDate, .dst_x = 0, .dst_y = 0, .result_scr = &date_scr };
    const uint32_t date_color = dgx_rgb_to_16(230, 230, 220);
    dgx_font_t *date_font = IosevkaMedium18();
    for (;;) {
        struct timeval now;
        gettimeofday(&now, 0);
        time_t delay = 1001 - now.tv_usec / 1000;
        dgx_copy_region_from_vscreen(&clock_scr, 0, 0, shscr->src_scr, src_x, src_y, clock_scr.width, clock_scr.height,
                                     0);
        drawClockHands(&clock_scr, clock_scr.width / 2, clock_scr.height / 2, 35);
        task_message_t *sndval = &clock_message;
        xQueueSendToFront(shscr->queue, &sndval, 0);
        dgx_copy_region_from_vscreen(&date_scr, 0, 0, shscr->bak_scr, 0, 0, date_scr.width, date_scr.height, 0);
        struct tm timeinfo;
        localtime_r(&now.tv_sec, &timeinfo);
        char datestr[50];
        strftime(datestr, sizeof(datestr), "%a, %e %B", &timeinfo);
        int16_t ycorner, height;
        int16_t width = dgx_font_string_bounds(datestr, date_font, &ycorner, &height);
        if (width > date_scr.width) width = date_scr.width;
        dgx_font_string_utf8_screen(&date_scr, date_scr.width / 2 - width / 2, date_scr.height / 2 + height / 2,
                                    datestr, date_color, date_color, LeftRight, TopBottom, false, date_font);
        task_message_t *snddate = &date_message;
        xQueueSendToFront(shscr->queue, &snddate, 0);
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

void vTaskGetWeather(void *pvParameters) {
    long delay = 60000;
    struct shared_screen *shscr = pvParameters;
    int16_t src_x = shscr->src_scr->width / 2 - shscr->dst_scr->width / 2;
    int16_t src_y = shscr->src_scr->height / 2 - shscr->dst_scr->height / 2;
    task_message_t message = { .type = Weather, .dst_x = src_x, .dst_y = src_y, .result_scr = shscr->dst_scr };
    dgx_font_t *weather_icon_font = WeatherIconsRegular24();
    for (;;) {
        if ( xSemaphoreTake( xSemLocation, 100 / portTICK_PERIOD_MS ) == pdTRUE) {
            str_t *url = 0;
            if (Location.zip) {
                url = str_new_pc("https://api.openweathermap.org/data/2.5/weather?q=");
                str_append_str(&url, Location.zip);
                str_append_pc(&url, ",");
                str_append_str(&url, Location.countryCode);
                str_append_pc(&url, "&appid=" WEATHER_API_KEY "&units=metric");
                printf("weather url request: %s\n", str_c(url));
            } else {
                delay = 100;
            }
            xSemaphoreGive(xSemLocation);
            if (url) {
                delay = 10000;
                ESP_LOGI("getWeather", "starting");
                heap_caps_print_heap_info(MALLOC_CAP_8BIT);
                http_client_response_t *weather_json = http_client_request_get(str_c(url));
                ESP_LOGI("getWeather", "got: %p", weather_json);
                if (weather_json) {
                    delay = 60000;
                    dgx_copy_region_from_vscreen(shscr->dst_scr, 0, 0, shscr->src_scr, src_x, src_y,
                                                 shscr->dst_scr->width, shscr->dst_scr->height, 0);
                    cJSON *json = weather_json->body ? cJSON_Parse(str_c(weather_json->body)) : 0;
                    if (json) {
                        printf("weather_json: %s\n", str_c(weather_json->body));
                        int cod = cJSON_GetObjectItem(json, "cod")->valueint;
                        if (cod == 200) {
                            cJSON *weather = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "weather"), 0);
                            cJSON *jmain = cJSON_GetObjectItem(json, "main");
                            cJSON *jsys = cJSON_GetObjectItem(json, "sys");
                            int temp = cJSON_GetObjectItem(jmain, "temp")->valueint;
                            int weather_id = cJSON_GetObjectItem(weather, "id")->valueint;
                            int dt = cJSON_GetObjectItem(json, "dt")->valueint;
                            int sunrise = cJSON_GetObjectItem(jsys, "sunrise")->valueint;
                            int sunset = cJSON_GetObjectItem(jsys, "sunset")->valueint;
                            uint8_t is_daytime = dt <= sunset && dt >= sunrise;
                            uint32_t wsym = findWeatherSymbol(weather_id, is_daytime);
                            uint32_t wc = weatherTempToRGB(temp);
                            int r, g, b;
                            WEATHERCOLOR24TORGB(r, g, b, wc);
                            uint32_t color = dgx_rgb_to_16(r, g, b);
                            int16_t xAdvance;
                            const glyph_t *wsym_glyph = dgx_font_find_glyph(wsym, weather_icon_font, &xAdvance);
                            int16_t wx = shscr->dst_scr->width / 2 - xAdvance / 2;
                            dgx_font_char_to_screen(shscr->dst_scr, wx,
                                                    shscr->dst_scr->height / 2 - wsym_glyph->yOffset + 3, wsym, color,
                                                    color, LeftRight, TopBottom, false, weather_icon_font);
                            int16_t ycorner, height;
                            char temp_str[10];
                            snprintf(temp_str, 9, "%d%c%c", temp, 0xb0, 'C');
                            int16_t width = dgx_font_string_bounds(temp_str, UbuntuCondensedRegular32(), &ycorner,
                                                                   &height);
                            dgx_font_string_utf8_screen(shscr->dst_scr, shscr->dst_scr->width / 2 - width / 2,
                                                        shscr->dst_scr->height / 2 - 2, temp_str, color, color,
                                                        LeftRight, TopBottom, false, UbuntuCondensedRegular32());
                            task_message_t *sndval = &message;
                            xQueueSendToFront(shscr->queue, &sndval, 0);
                        }
                        cJSON_Delete(json);
                    } else {
                        printf("url %s returns: %s\n", str_c(url), str_c(weather_json->body));
                    }
                    free(weather_json->body);
                    free(weather_json);
                }
            }
            free(url);
        } else {
            delay = 100;
        }
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

void write_city(dgx_screen_t *scr, const char *name) {
    const uint32_t city_color = dgx_rgb_to_16(230, 230, 230);
    dgx_font_t *tf[] = { UbuntuCondensedRegular32(), UbuntuCondensedRegular15() };
    for (size_t i = 0; i < sizeof(tf) / sizeof(tf[0]); ++i) {
        int16_t ycorner, height;
        int16_t width = dgx_font_string_bounds(name, tf[i], &ycorner, &height);
        int16_t x = scr->width / 2 - width / 2;
        if (x >= 0 || i + 1 == sizeof(tf) / sizeof(tf[0])) {
            if (x < 0) x = 0;
            dgx_font_string_utf8_screen(scr, x, scr->height - 3, name, city_color, city_color, LeftRight, TopBottom,
            false,
                                        tf[i]);
            return;
        }
    }
}

void vTaskGetLocation(void *pvParameters) {
    struct shared_screen *shscr = pvParameters;
    int16_t src_x = 0;
    int16_t src_y = shscr->src_scr->height - 30;
    dgx_screen_t *city_scr = dgx_new_vscreen_from_region(shscr->src_scr, 0, src_y, shscr->src_scr->width, 30);
    task_message_t message = { .type = CityName, .dst_x = src_x, .dst_y = src_y, .result_scr = city_scr };
    long delay = 600000;
    for (;;) {
        WLocation_t location = auto_location();
        if (location.timezone) {
            setupTZ(str_c(location.timezone));
            if ( xSemaphoreTake( xSemLocation, 100 / portTICK_PERIOD_MS ) == pdTRUE) {
                free(Location.country);
                free(Location.countryCode);
                free(Location.region);
                free(Location.regionName);
                free(Location.city);
                free(Location.zip);
                free(Location.timezone);
                free(Location.isp);
                free(Location.ip);
                Location = location;
                write_city(city_scr, str_c(Location.city));
                xSemaphoreGive(xSemLocation);
                task_message_t *sndval = &message;
                xQueueSendToFront(shscr->queue, &sndval, 0);
            } else {
                free(location.country);
                free(location.countryCode);
                free(location.region);
                free(location.regionName);
                free(location.city);
                free(location.zip);
                free(location.timezone);
                free(location.isp);
                free(location.ip);
            }
        }
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    xSemLocation = xSemaphoreCreateMutex();
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    fast_scan();
    dgx_screen_t scr_tft;
    dgx_spi_init(&scr_tft, HSPI_HOST, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS, PIN_NUM_DC, SPI_MASTER_FREQ_26M);
    dgx_st7735_init(&scr_tft, PIN_NUM_RST, 16, RGB);
    dgx_screen_t scr_vtft;
    dgx_v_init(&scr_vtft, scr_tft.width, scr_tft.height, scr_tft.color_bits);
    uint32_t mask = 0b1110000011001110;
    dgx_screen_t *scr = &scr_tft;
    dgx_screen_t *vscr = &scr_vtft;
    for (int16_t y = 0; y < vscr->height; ++y) {
        vscr->fill_hline(vscr, 0, y, 128, dgx_rgb_to_16(64, 64, 64), dgx_rgb_to_16(128, 128, 128),
                         (mask = rol_bits(mask, 16)), 16);
//        vscr->draw_pixel(vscr, y / 2, y, WHITE12);
    }
    dgx_vscreen_to_screen(scr, 0, 0, vscr);
//    dgx_screen_t scr_bw;
//    dgx_bw_init(&scr_bw, 100, 100);
    mask = 0b11110000;
    int i = 1000;
    bool sntp_inited = false;
    while ((!is_got_ip || !sntp_inited || sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) && i--) {
        int64_t sttime = esp_timer_get_time();
        uint32_t lm = mask;
        for (int r = 50; r > 45; --r) {
            lm = dgx_draw_circle_mask(vscr, 64, 80, r, dgx_rgb_to_16(255, 128, 128), dgx_rgb_to_16(128, 128, 255), lm,
                                      8);
        }
        dgx_vscreen_to_screen(scr, 0, 0, vscr);
        int64_t etime = esp_timer_get_time();
        int64_t draw_time = etime - sttime;
        mask = ror_bits(mask, 8);
        int64_t delay_time = (100000 - draw_time) / 1000;
//        printf("draw time: %lld; delay: %lld\n", draw_time, delay_time);
        if (!sntp_inited) {
            if (is_got_ip) {
                printf("start sntp\n");
                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                sntp_setservername(0, "pool.ntp.org");
                sntp_init();
                sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
                sntp_inited = true;
            }
        }
        if (delay_time < portTICK_PERIOD_MS) delay_time = portTICK_PERIOD_MS;
        vTaskDelay(delay_time / portTICK_PERIOD_MS);
    }
    setupTZ("Europe/Berlin");
    drawClockFace(vscr, vscr->width / 2, vscr->height / 2, 35, 45);
    dgx_vscreen_to_screen(scr, 0, 0, vscr);
    dgx_screen_t *weather_scr = dgx_new_vscreen_from_region(vscr, vscr->width / 2 - 40, vscr->height / 2 - 40, 80, 80);
    QueueHandle_t xque = xQueueCreate(10, sizeof(task_message_t*));
    struct shared_screen oclock_params = { .dst_scr = 0, .src_scr = weather_scr, .bak_scr = vscr, .queue = xque };
    struct shared_screen weather_params = { .dst_scr = weather_scr, .src_scr = vscr, .bak_scr = 0, .queue = xque };
    xTaskCreate(vTaskDrawClockHands, "oClock", 2048, &oclock_params, tskIDLE_PRIORITY, 0);
    xTaskCreate(vTaskGetLocation, "Location", 4096, &weather_params, tskIDLE_PRIORITY, 0);
    xTaskCreate(vTaskGetWeather, "Weather", 4096, &weather_params, tskIDLE_PRIORITY, 0);
    task_message_t *message;
    while (1) {
        if (xQueueReceive(xque, &message, 0)) {
            if (message->type == ClockHands) {
                dgx_vscreen_to_screen(scr, scr->width / 2 - 40, scr->height / 2 - 40, message->result_scr);
            } else if (message->type == CityName) {
                dgx_vscreen_to_screen(scr, message->dst_x, message->dst_y, message->result_scr);
            } else if (message->type == CurrentDate) {
                dgx_vscreen_to_screen(scr, message->dst_x, message->dst_y, message->result_scr);
            }
//            heap_caps_print_heap_info(MALLOC_CAP_8BIT);
        } else {
            vTaskDelay(1);
        }
    }
}
