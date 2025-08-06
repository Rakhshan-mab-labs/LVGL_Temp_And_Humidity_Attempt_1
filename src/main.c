#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <math.h>
#include <lvgl.h>

#define DHT_NODE DT_PATH(dht11)

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#define NUM_TEXT_LABELS 2
#define NUM_DYNAMIC_LABELS 2

static const struct device *epaper;
static const struct device *dht_dev;

static int temp_c = 0;
static int humidity = 0;

void initialize_devices(void)
{
    epaper = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(epaper)) {
        LOG_ERR("E-Paper display device not ready");
        return;
    }

    dht_dev = DEVICE_DT_GET(DHT_NODE);
    if (!device_is_ready(dht_dev)) {
        LOG_ERR("DHT device not ready");
        return;
    }
}

bool read_dht11(void) 
{
    struct sensor_value temp, humid;

    if (sensor_sample_fetch(dht_dev) != 0) {
        LOG_ERR("Failed to fetch temperature sample");
        return false;
    }

    if (sensor_channel_get(dht_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) != 0 || sensor_channel_get(dht_dev, SENSOR_CHAN_HUMIDITY, &humid) != 0) {
        LOG_ERR("Failed to fetch temperature or humidity sample");
        return false;
    }

    temp_c = temp.val1;
    humidity = humid.val1;

    return true;
}

void update_label(lv_obj_t *label, const char *prev_text, const char *text) 
{
    if (strlen(prev_text) > 0) {
        lv_label_set_text(label, prev_text);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_invalidate(label);
        lv_refr_now(NULL);
    }

    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_invalidate(label);
    lv_refr_now(NULL);
}

void update_temp_and_humidity_labels(lv_obj_t *temp, lv_obj_t *humid, const char *new_temp, const char *new_humid)
{
    static char prev_temp[32] = "";
    static char prev_humid[32] = "";
    
    if (strcmp(prev_temp, new_temp) != 0) {
        update_label(temp, prev_temp, new_temp);
        strncpy(prev_temp, new_temp, sizeof(prev_temp));
    }

    if (strcmp(prev_humid, new_humid) != 0) {
        update_label(humid, prev_humid, new_humid);
        strncpy(prev_humid, new_humid, sizeof(prev_humid));
    }
}

int main(void)
{

    initialize_devices();

    lv_init();

    lv_obj_t *temp_text;
    lv_obj_t *humidity_text;
    lv_obj_t *temp_num_text_label;
    lv_obj_t *humidity_num_text_label;

    lv_obj_t *dynamic_labels[NUM_DYNAMIC_LABELS] = {NULL, NULL};
    lv_obj_t *text_labels[NUM_TEXT_LABELS] = {NULL, NULL};

    // Set background to be white
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    for (int i = 0; i < NUM_TEXT_LABELS; i++) {
        text_labels[i] = lv_label_create(lv_scr_act());
        lv_obj_set_style_bg_color(text_labels[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(text_labels[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_width(text_labels[i], 65);
        lv_obj_set_height(text_labels[i], 20);
        lv_label_set_long_mode(text_labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(text_labels[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_font(text_labels[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(text_labels[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_align(text_labels[i], LV_ALIGN_TOP_LEFT, 10, 10 + (i * 30));
    }

    for (int i = 0; i < NUM_DYNAMIC_LABELS; i++) {
        dynamic_labels[i] = lv_label_create(lv_scr_act());
        lv_obj_set_style_bg_color(dynamic_labels[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dynamic_labels[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_width(dynamic_labels[i], 50);
        lv_obj_set_height(dynamic_labels[i], 20);
        lv_label_set_long_mode(dynamic_labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(dynamic_labels[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_font(dynamic_labels[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(dynamic_labels[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_align_to(dynamic_labels[i], text_labels[i], LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    }

    temp_text = text_labels[0];
    humidity_text = text_labels[1];
    
    lv_label_set_text(temp_text,     "Temp: ");
    lv_label_set_text(humidity_text, "Humid: ");

    temp_num_text_label = dynamic_labels[0];
    humidity_num_text_label = dynamic_labels[1];

    lv_label_set_text(temp_num_text_label, "00");
    lv_label_set_text(humidity_num_text_label, "00");

    // Turn the epaper display on
    lv_timer_handler();
    display_blanking_off(epaper);

    int last_temp = -1000;
    int last_humid = -1000;

    bool first_run = true;

    while (1) {
        if (first_run) {
            lv_timer_handler();   // One-time initial draw
            lv_refr_now(NULL);    // Force refresh
            first_run = false;
        }

        if (read_dht11()) {
            char temp_str[32];
            char humid_str[32];
            snprintf(temp_str, sizeof(temp_str), "%d C", temp_c);
            snprintf(humid_str, sizeof(humid_str), "%d %%", humidity);

            update_temp_and_humidity_labels(temp_num_text_label, humidity_num_text_label, temp_str, humid_str);
        }

        last_temp = temp_c;
        last_humid = humidity;

        LOG_INF("Temperature: %d C", last_temp);
        LOG_INF("Humidity: %d %%", last_humid);

        k_sleep(K_MSEC(1000));
    }

    return 0;
}