#include <string.h>

#include "esp_heap_caps.h"
#include "dgx_screen.h"
#include "drivers/ssd1306.h"
// Following definitions are bollowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html

// SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define SSD1306_I2C_ADDRESS   0x3C

// Control byte
#define SSD1306_CONTROL_BYTE_CMD_SINGLE    0x80
#define SSD1306_CONTROL_BYTE_CMD_STREAM    0x00
#define SSD1306_CONTROL_BYTE_DATA_STREAM   0x40

// Fundamental commands (pg.28)
#define SSD1306_CMD_SET_CONTRAST           0x81    // follow with 0x7F
#define SSD1306_CMD_DISPLAY_RAM            0xA4
#define SSD1306_CMD_DISPLAY_ALLON          0xA5
#define SSD1306_CMD_DISPLAY_NORMAL         0xA6
#define SSD1306_CMD_DISPLAY_INVERTED       0xA7
#define SSD1306_CMD_DISPLAY_OFF            0xAE
#define SSD1306_CMD_DISPLAY_ON             0xAF

// Addressing Command Table (pg.30)
#define SSD1306_CMD_SET_MEMORY_ADDR_MODE   0x20    // follow with 0x00 = HORZ mode = Behave like a KS108 graphic LCD
#define SSD1306_CMD_SET_COLUMN_RANGE       0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define SSD1306_CMD_SET_PAGE_RANGE         0x22    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define SSD1306_CMD_SET_DISPLAY_START_LINE 0x40
#define SSD1306_CMD_SET_SEGMENT_REMAP      0xA1
#define SSD1306_CMD_SET_MUX_RATIO          0xA8    // follow with 0x3F = 64 MUX
#define SSD1306_CMD_SET_COM_SCAN_MODE      0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET     0xD3    // follow with 0x00
#define SSD1306_CMD_SET_COM_PIN_MAP        0xDA    // follow with 0x12
#define SSD1306_CMD_NOP                    0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV    0xD5    // follow with 0x80
#define SSD1306_CMD_SET_PRECHARGE          0xD9    // follow with 0xF1
#define SSD1306_CMD_SET_VCOMH_DESELCT      0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define SSD1306_CMD_SET_CHARGE_PUMP        0x8D    // follow with 0x14
esp_err_t dgx_i2c_ssd1306_driver_init(dgx_screen_t *scr, uint8_t is_ext_vcc) {
    esp_err_t espRc;
    uint8_t height = scr->height;
    if (height != 64 && height != 32 && height != 16) return ESP_ERR_INVALID_ARG;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    // Init sequence
    i2c_master_write_byte(cmd, (scr->i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_DISPLAY_OFF, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_DISPLAY_CLK_DIV, true);
    i2c_master_write_byte(cmd, 0x80, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_MUX_RATIO, true);
    i2c_master_write_byte(cmd, height - 1, true); //
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_DISPLAY_OFFSET, true);
    i2c_master_write_byte(cmd, 0x0, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_DISPLAY_START_LINE, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_CHARGE_PUMP, true);
    if (is_ext_vcc) {
        i2c_master_write_byte(cmd, 0x10, true);
    } else {
        i2c_master_write_byte(cmd, 0x14, true);
    }
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_SEGMENT_REMAP, true); // reverse left-right mapping | 0xA0
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_COM_SCAN_MODE, true); // reverse up-bottom mapping | 0xC0
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_MEMORY_ADDR_MODE, true);
    i2c_master_write_byte(cmd, 0x0, true); // horizontal
    if (height == 32) {
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_COM_PIN_MAP, true);
        i2c_master_write_byte(cmd, 0x02, true);
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_CONTRAST, true);
        i2c_master_write_byte(cmd, 0x8F, true);
    } else if (height == 64) {
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_COM_PIN_MAP, true);
        i2c_master_write_byte(cmd, 0x12, true);
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_CONTRAST, true);
        if (is_ext_vcc) {
            i2c_master_write_byte(cmd, 0x9f, true);
        } else {
            i2c_master_write_byte(cmd, 0xcf, true);
        }
    } else {
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_COM_PIN_MAP, true);
        i2c_master_write_byte(cmd, 0x02, true);
        i2c_master_write_byte(cmd, SSD1306_CMD_SET_CONTRAST, true);
        if (is_ext_vcc) {
            i2c_master_write_byte(cmd, 0x10, true);
        } else {
            i2c_master_write_byte(cmd, 0xaf, true);
        }
    }
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_PRECHARGE, true);
    if (is_ext_vcc) {
        i2c_master_write_byte(cmd, 0x22, true);
    } else {
        i2c_master_write_byte(cmd, 0xF1, true);
    }
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_VCOMH_DESELCT, true);
    i2c_master_write_byte(cmd, 0x40, true); // no idea
    i2c_master_write_byte(cmd, SSD1306_CMD_DISPLAY_RAM, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_DISPLAY_NORMAL, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_DISPLAY_ON, true);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(scr->i2c_num, cmd, 10 / portTICK_PERIOD_MS);
    return espRc;
    i2c_cmd_link_delete(cmd);
}

void dgx_i2c_set_area(struct dgx_screen_ *scr, uint16_t caset_lo, uint16_t caset_hi, uint16_t raset_lo,
        uint16_t raset_hi) {
    scr->fb_col_lo = caset_lo;
    scr->fb_col_hi = caset_hi;
    scr->fb_row_lo = raset_lo;
    scr->fb_row_hi = raset_hi;
    scr->fb_pointer_x = caset_lo;
    scr->fb_pointer_y = raset_lo;
}

void dgx_i2c_write_data(dgx_screen_t *scr, uint8_t *data, uint32_t lenbits) {
    uint8_t b = *data, mask = 0x80;
    uint16_t py = scr->direction_y == BottomTop ? scr->height - scr->fb_pointer_y - 1 : scr->fb_pointer_y;
    uint8_t page = py >> 3;
    uint8_t bit = py & 7;
    uint8_t rmask = 1 << bit;
    uint8_t *pb = scr->frame_buffer + page * scr->width + scr->fb_pointer_x;
    while (lenbits) {
        if (scr->fb_pointer_x > scr->fb_col_hi) {
            scr->fb_pointer_x = scr->fb_col_lo;
            scr->fb_pointer_y++;
            py = scr->direction_y == BottomTop ? scr->height - scr->fb_pointer_y - 1 : scr->fb_pointer_y;
            page = py >> 3;
            bit = py & 7;
            rmask = 1 << bit;
            pb = scr->frame_buffer + page * scr->width + scr->fb_pointer_x;
        }
        if (scr->fb_pointer_y > scr->fb_row_hi) {
            scr->fb_pointer_y = scr->fb_row_lo;
            py = scr->direction_y == BottomTop ? scr->height - scr->fb_pointer_y - 1 : scr->fb_pointer_y;
            page = py >> 3;
            bit = py & 7;
            rmask = 1 << bit;
            pb = scr->frame_buffer + page * scr->width + scr->fb_pointer_x;
        }
        if (!mask) {
            b = *++data;
            mask = 0x80;
        }
        if (mask & b)
            *pb |= rmask;
        else
            *pb &= ~rmask;
        pb++;
        scr->fb_pointer_x++;
        lenbits--;
        mask >>= 1;
    }
}

void dgx_i2c_write_value(dgx_screen_t *scr, uint32_t value) {
    if (scr->fb_pointer_x >= scr->fb_col_hi) {
        scr->fb_pointer_x = scr->fb_col_lo;
        scr->fb_pointer_y++;
    }
    if (scr->fb_pointer_y >= scr->fb_row_hi) {
        scr->fb_pointer_y = scr->fb_row_lo;
    }
    scr->draw_pixel(scr, scr->fb_pointer_x, scr->fb_pointer_y, value);
}

void dgx_i2c_ssd1306_draw_pixel(dgx_screen_t *scr, int16_t x, int16_t y, uint32_t color) {
    uint8_t page = y / 8;
    uint8_t bit = y & 7;
    if (scr->direction_y == BottomTop) {
        page = scr->height / 8 - page - 1;
        bit = 7 - bit;
    }
    uint8_t mask = 1 << bit;
    uint8_t *pb = scr->frame_buffer + page * scr->width + x;
    if (color)
        *pb |= mask;
    else
        *pb &= ~mask;
    if (!scr->in_progress) scr->update_screen(scr, x, y, x, y);
}

void dgx_i2c_wait_data(dgx_screen_t *scr) {
    return;
}

uint32_t dgx_i2c_fill_hline(dgx_screen_t *scr, int16_t x, int16_t y, int16_t w, uint32_t color, uint32_t bg,
        uint32_t mask, uint8_t mask_bits, uint8_t shift_dir) {
    uint32_t test_bit = shift_dir ? 1 : (1 << (mask_bits - 1));
    uint32_t rmask;
    if (w < 0) {
        w = -w;
        uint8_t rb = (mask_bits - (w % mask_bits)) % mask_bits;
        if (shift_dir) {
            // ror
            mask = (mask << (mask_bits - rb)) | (mask >> rb);
            rmask = ror_bits(mask, mask_bits);
        } else {
            // rol
            mask = (mask >> (mask_bits - rb)) | (mask << rb);
            rmask = rol_bits(mask, mask_bits);
        }
    } else {
        uint8_t rb = w % mask_bits;
        if (rb && shift_dir) {
            // ror
            rmask = (mask << (mask_bits - rb)) | (mask >> rb);
        } else if (rb) {
            // rol
            rmask = (mask >> (mask_bits - rb)) | (mask << rb);
        } else {
            rmask = mask;
        }

    }
    scr->in_progress++;
    if (shift_dir == 0) {
        for (uint16_t i = 0; i < w; ++i, mask = rol_bits(mask, mask_bits)) {
            uint32_t c = mask & test_bit ? color : bg;
            scr->draw_pixel(scr, x + i, y, c);
        }
    } else {
        for (uint16_t i = 0; i < w; ++i, mask = ror_bits(mask, mask_bits)) {
            uint32_t c = mask & test_bit ? color : bg;
            scr->draw_pixel(scr, x + i, y, c);
        }
    }
    if (!--scr->in_progress) scr->update_screen(scr, x, y, x + w - 1, y);
    return rmask;
}

uint32_t dgx_i2c_fill_vline(dgx_screen_t *scr, int16_t x, int16_t y, int16_t h, uint32_t color, uint32_t bg,
        uint32_t mask, uint8_t mask_bits, uint8_t shift_dir) {
    uint32_t test_bit = shift_dir ? 1 : (1 << (mask_bits - 1));
    uint32_t rmask;
    if (h < 0) {
        h = -h;
        uint8_t rb = (mask_bits - (h % mask_bits)) % mask_bits;
        if (shift_dir) {
            // ror
            mask = (mask << (mask_bits - rb)) | (mask >> rb);
            rmask = ror_bits(mask, mask_bits);
        } else {
            // rol
            mask = (mask >> (mask_bits - rb)) | (mask << rb);
            rmask = rol_bits(mask, mask_bits);
        }
    } else {
        uint8_t rb = h % mask_bits;
        if (rb && shift_dir) {
            // ror
            rmask = (mask << (mask_bits - rb)) | (mask >> rb);
        } else if (rb) {
            // rol
            rmask = (mask >> (mask_bits - rb)) | (mask << rb);
        } else {
            rmask = mask;
        }

    }
    scr->in_progress++;
    if (shift_dir == 0) {
        for (uint16_t i = 0; i < h; ++i, mask = rol_bits(mask, mask_bits)) {
            uint32_t c = mask & test_bit ? color : bg;
            scr->draw_pixel(scr, x, y + i, c);
        }
    } else {
        for (uint16_t i = 0; i < h; ++i, mask = ror_bits(mask, mask_bits)) {
            uint32_t c = mask & test_bit ? color : bg;
            scr->draw_pixel(scr, x, y + i, c);
        }
    }
    if (!--scr->in_progress) scr->update_screen(scr, x, y, x, y + h - 1);
    return rmask;
}

void dgx_i2c_ssd1306_vline(dgx_screen_t *scr, int16_t x, int16_t y, int16_t h, uint32_t color) {
    int16_t y2 = y + h - 1;
    if (scr->direction_y == BottomTop) {
        y = scr->height - y2 - 1;
        y2 = y + h - 1;
    }
    uint8_t page = (uint16_t) y >> 3;
    uint8_t *pb = scr->frame_buffer + page * scr->width + x;
    for (int16_t yl = y; yl <= y2; yl += 8 - (yl & 7), pb += scr->width) {
        page = (uint16_t) yl >> 3;
        uint8_t mask = 0xff << (yl & 7);
        if ((yl & ~7) == (y2 & ~7)) mask &= 0xff >> (7 - (y2 & 7));
        if (color)
            *pb |= mask;
        else
            *pb &= ~mask;
    }
    if (!scr->in_progress) scr->update_screen(scr, x, y, x, y);
}
void dgx_i2c_fill_rectangle(dgx_screen_t *scr, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    if (y < 0 || w < 0 || x + w < 0 || x >= scr->width || h < 0 || y + h < 0 || y > scr->height) return;
    if (x < 0) {
        w = x + w;
        x = 0;
    }
    if (x + w > scr->width) {
        w = scr->width - x;
    }
    if (y < 0) {
        h = y + h;
        y = 0;
    }
    if (y + h > scr->height) {
        h = scr->height - y;
    }
    scr->in_progress++;
    for (uint16_t i = 0; i < w; ++i) {
        dgx_i2c_ssd1306_vline(scr, x + i, y, h, color);
    }
    if (!--scr->in_progress) scr->update_screen(scr, x, y, x + w - 1, y + h - 1);
}

void dgx_i2c_update_screen(dgx_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    if (x1 > x2) {
        int16_t t = x1;
        x1 = x2;
        x2 = t;
    }
    if (y1 > y2) {
        int16_t t = y1;
        y1 = y2;
        y2 = t;
    }
    if (x2 < 0 || y2 < 0 || x1 >= scr->width || y1 >= scr->height) return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= scr->width) x2 = scr->width - 1;
    if (y2 >= scr->height) y2 = scr->height - 1;
    for (int16_t y = (y1 & ~7); y <= y2; y += 8) {
        uint8_t page = y / 8;
        if (scr->direction_y == BottomTop) {
            page = scr->height / 8 - page - 1;
        }
        uint8_t *pb = scr->frame_buffer + page * scr->width + x1;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (scr->i2c_address << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_CMD_SINGLE, true);
        i2c_master_write_byte(cmd, 0x00 | (x1 & 0xf), true); // reset column
        i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_CMD_SINGLE, true);
        i2c_master_write_byte(cmd, 0x10 | ((x1 & 0xf0) >> 4), true);
        i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_CMD_SINGLE, true);
        i2c_master_write_byte(cmd, 0xB0 | page, true); // increment page
        i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_DATA_STREAM, true);
        i2c_master_write(cmd, pb, x2 - x1 + 1, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(scr->i2c_num, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
    }
}

void dgx_i2c_ssd1306_contrast(dgx_screen_t *scr, uint8_t contrast) {
    i2c_cmd_handle_t cmd;
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (scr->i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SSD1306_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, SSD1306_CMD_SET_CONTRAST, true);
    i2c_master_write_byte(cmd, contrast, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(scr->i2c_num, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

esp_err_t dgx_i2c_ssd1306_init(dgx_screen_t *scr, i2c_port_t i2c_num, uint8_t height, uint8_t is_ext_vcc,
        dgx_orientation_t ydir) {
    scr->dc = 0;
    scr->pending_transactions = 0;
    scr->max_transactions = 20;
    scr->semaphore = 0;
    scr->trans = 0;
    scr->trans_data = 0;
    scr->data_free = true;
    scr->in_progress = 0;
    if (height == 16) {
        scr->width = 96;
    } else {
        scr->width = 128;
    }
    scr->height = height;
    scr->i2c_num = i2c_num;
    scr->i2c_address = SSD1306_I2C_ADDRESS;
    scr->color_bits = 1;
    scr->lcd_ram_bits = 1;
    scr->direction_x = 1;
    scr->direction_y = ydir;
    scr->buffer_len = scr->width * scr->height / 8;
    scr->buffer = heap_caps_calloc(1, scr->buffer_len, MALLOC_CAP_DMA);
    if (!scr->buffer) return ESP_FAIL;
    scr->frame_buffer = heap_caps_calloc(1, scr->width * scr->height / 8, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!scr->frame_buffer) return ESP_FAIL;
    dgx_i2c_set_area(scr, 0, scr->width - 1, 0, height - 1);
    scr->set_area = dgx_i2c_set_area;
    scr->draw_pixel = dgx_i2c_ssd1306_draw_pixel;
    scr->write_area = dgx_i2c_write_data;
    scr->write_value = dgx_i2c_write_value;
    scr->wait_screen_buffer = dgx_i2c_wait_data;
    scr->fill_hline = dgx_i2c_fill_hline;
    scr->fill_vline = dgx_i2c_fill_vline;
    scr->fill_rectangle = dgx_i2c_fill_rectangle;
    scr->update_screen = dgx_i2c_update_screen;
    esp_err_t rc = dgx_i2c_ssd1306_driver_init(scr, is_ext_vcc);
    if (rc == ESP_OK) dgx_i2c_update_screen(scr, 0, 0, scr->width - 1, scr->height - 1);
    return rc;
}

