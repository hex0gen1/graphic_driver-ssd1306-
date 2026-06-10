// ---- Imports ----

#include "text.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

// ---- Constants ----

#define SSD_1306_ADDR 0x78;
uint8_t framebuffer[1024];
#define ABS(x) ((x) < 0 ? -(x) : (x))

// ---- Functions ----

void twi_init(void) {
  TWBR = 152; //  72/152
  TWSR &= ~((1 << TWPS0) | (1 << TWPS1));
  TWCR = (1 << TWEN);
};
uint8_t twi_probe(uint8_t address) {
  TWCR = (1 << TWEN) | (1 << TWSTA) | (1 << TWINT);
  while (!(TWCR & (1 << TWINT)))
    ;

  TWDR = address;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;

  uint8_t status = TWSR & 0xF8;

  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  _delay_ms(10);

  return (status == 0x18);
}
uint8_t twi_start() {
  TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA);
  while (!(TWCR & (1 << TWINT)))
    ;
  return (TWSR & 0xF8);
}

void twi_stop() { TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); }

uint8_t twi_write(uint8_t data) {
  TWDR = data;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
  return (TWSR & 0xF8);
}

void ssd1306_command(uint8_t cmd) {
  twi_start();
  twi_write(0x78);
  twi_write(0x00); // 0x00 - signal that cmd will be sended after
  twi_write(cmd);
  twi_stop();
}

void ssd1306_init(void) {
  _delay_ms(100); // power stabilization delay

  ssd1306_command(0xAE); // Display OFF
  ssd1306_command(0xD5); // Set Display Clock Divide Ratio/Oscillator Frequency
  ssd1306_command(0x80); // Default value
  ssd1306_command(0xA8); // Set Multiplex Ratio
  ssd1306_command(0x3F); // 1/64 duty (for 64px height)
  ssd1306_command(0xD3); // Set Display Offset
  ssd1306_command(0x00); // No offset
  ssd1306_command(0x40); // Set Start Line (0x40 + 0)
  ssd1306_command(0x8D); // Charge Pump Setting
  ssd1306_command(0x14); // Charge pump enabling
  ssd1306_command(0x20); // Set Memory Addressing Mode
  ssd1306_command(0x00); // Horizontal Addressing Mode
  ssd1306_command(0xA1); // Segment Re-map (A0 or A1 in terms of facing)
  ssd1306_command(0x3F); // 1/64 duty (64px height)
  ssd1306_command(0xD3); // Set Display Offset
  ssd1306_command(0x00); // No offset
  ssd1306_command(0x40); // Set Start Line (0x40 + 0)
  ssd1306_command(0x8D); // Charge Pump Setting
  ssd1306_command(0x14); // Enabling charge pump
  ssd1306_command(0x20); // Set Memory Addressing Mode
  ssd1306_command(0x00); // Horizontal Addressing Mode
  ssd1306_command(0xA1); // Segment Re-map (A0 or A1 in terms of facing
  // direction(if text mirrored, try to switch between))
  ssd1306_command(0xC8); // COM Output Scan Direction
  ssd1306_command(0xDA); // Set COM Pins Hardware Configuration
  ssd1306_command(0x12);
  ssd1306_command(0x81); // Set Contrast Control
  ssd1306_command(0xCF); // Brightness
  ssd1306_command(0xA4); // Disable Entire Display On
  ssd1306_command(0xA6); // Normal Display
  ssd1306_command(0xA6); // Normal Display

  ssd1306_command(0xAF); // Display ON
}

void ssd1306_clear(void) {
  for (uint16_t i = 0; i < 1024; ++i) {
    framebuffer[i] = 0x00;
  }
}
uint8_t mpu6050_read_reg(uint8_t reg_addr) {
  uint8_t data = 0;

  twi_start();
  twi_write(0xD0);
  twi_write(0x75);
  twi_start();
  twi_write(0xD1);

  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
  data = TWDR;

  twi_stop();

  return data;
}
void ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t color) {
  if (x >= 128 || y >= 64)
    return;

  uint16_t index = (y / 8) * 128 + x;
  uint8_t bit = y % 8;

  if (color) {
    framebuffer[index] |= (1 << bit);
  } else {
    framebuffer[index] &= ~(1 << bit);
  }
}

void ssd1306_display(void) {
  ssd1306_command(0x21); // Set Column Address
  ssd1306_command(0x00); // Start 0
  ssd1306_command(0x7F); // End 127
  ssd1306_command(0x22); // Set Page Address
  ssd1306_command(0x00); // Start Page 0
  ssd1306_command(0x07); // End Page 7

  twi_start();
  twi_write(0x78);
  twi_write(0x40); // 0x40 -  data flag

  for (uint16_t i = 0; i < 1024; ++i) {
    twi_write(framebuffer[i]);
  }

  twi_stop();
}
void ssd1306_draw_line(int16_t x0, int16_t y0, int16_t x, int16_t y,
                       int16_t color) {
  int16_t dx = ABS(x - x0);
  int16_t dy = ABS(y - y0);

  int16_t sx = (x0 < x) ? 1 : -1;
  int16_t sy = (y0 < y) ? 1 : -1;

  int16_t err = dx - dy;
  int16_t e2;

  while (1) {
    ssd1306_draw_pixel(x0, y0, color);

    if (x0 == x && y0 == y)
      break;

    e2 = 2 * err;

    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}
void ssd1306_draw_rect(int16_t x, int16_t y, uint8_t h, uint8_t w,
                       uint8_t color) {
  ssd1306_draw_line(x, y, x + w - 1, y, color);
  ssd1306_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
  ssd1306_draw_line(x, y, x, y + h - 1, color);
  ssd1306_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void ssd1306_fill_rect(int16_t x, int16_t y, uint8_t w, uint8_t h,
                       uint8_t color) {
  if (x >= 128 || y >= 64 || h == 0 || w || 0) {
    return;
  }
  if (x + w > 128) {
    w = 128 - x;
  }
  if (y + h > 64) {
    h = 64 - y;
  }

  for (uint8_t row = 0; row < h; ++row) {
    for (uint8_t col = 0; col < w; ++col) {
      ssd1306_draw_pixel(x + col, y + row, color);
    }
  }
}
void ssd1306_draw_char(uint8_t x, uint8_t y, char c) {
  uint8_t fontIdx = c - 0x20;
  if (fontIdx > 95)
    return;

  for (uint8_t col = 0; col < 5; ++col) {
    unsigned char colData = pgm_read_byte(&(font_5x7[fontIdx][col]));

    for (uint8_t row = 0; row < 7; ++row) {
      if (colData & (1 << row)) {
        ssd1306_draw_pixel(x + col, y + row, 1);
      }
    }
  }
}
void ssd1306_draw_char_scaled(uint8_t x, uint8_t y, char c, uint8_t scale) {
  uint8_t fontIdx = c - 0x20;
  if (fontIdx > 95)
    return;

  for (uint8_t col = 0; col < 5; ++col) {
    uint8_t colData = pgm_read_byte(&(font_5x7[fontIdx][col]));

    for (uint8_t row = 0; row < 7; ++row) {
      if (colData & (1 << row)) {
        for (uint8_t dy = 0; dy < scale; ++dy) {
          for (uint8_t dx = 0; dx < scale; ++dx) {
            ssd1306_draw_pixel(x + (col * scale) + dx, y + (row * scale) + dy,
                               1);
          }
        }
      }
    }
  }
}
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str) {
  while (*str) {
    ssd1306_draw_char(x, y, *str);
    x += 6;
    ++str;
  }
}
void ssd1306_draw_string_scaled(uint8_t x, uint8_t y, const char *str,
                                uint8_t scale) {
  while (*str) {
    ssd1306_draw_char_scaled(x, y, *str, scale);
    x += 6 * scale;
    ++str;
  }
}
void set_brightness_level(uint8_t percentage) {
  ssd1306_command(0x81);
  ssd1306_command(percentage);
}
void ssd1306_invert_display(bool invert) {
  if (invert) {
    ssd1306_command(0xA7);
  } else {
    ssd1306_command(0xA6);
  }
}
void ssd1306_set_power(bool on) {
  if (on) {
    ssd1306_command(0xAF);
  } else {
    ssd1306_command(0xAE);
  }
}
void ssd1306_draw_string_wrapped(uint8_t x, uint8_t y, const char *str,
                                 uint8_t scale, uint8_t max_width) {
  uint8_t current_x = x;
  uint8_t current_y = y;

  uint8_t char_w = 6 * scale;
  uint8_t line_h = 8 * scale;

  while (*str) {
    if (*str == '\n') {
      current_x = x;
      current_y += line_h;
      str++;
      continue;
    }
    const char *word_end = str;
    while (*word_end != ' ' && *word_end != '\n' && *word_end != '\0') {
      word_end++;
    }
    uint8_t word_len = word_end - str;
    uint8_t word_width = word_len * char_w;
    if (current_x > x && current_x + word_width > max_width) {
      current_x = x;
      current_y += line_h;
    }

    const char *p = str;
    while (p < word_end) {
      if (current_y >= 64)
        return;

      ssd1306_draw_char_scaled(current_x, current_y, *p, scale);
      current_x += char_w;
      p++;
    }

    str = word_end;
    if (*str == ' ') {
      if (current_x + char_w > max_width) {
        current_x = x;
        current_y += line_h;
      } else {
        current_x += char_w;
      }
      str++;
    }
  }
}
void uart_init(uint16_t ubrr) {

  UBRR0H = (uint8_t)(ubrr >> 8);
  UBRR0L = (uint8_t)ubrr;

  UCSR0A = (1 << U2X0);

  UCSR0B = (1 << TXEN0);

  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_send_char(char c) {
  while (!(UCSR0A & (1 << UDRE0))) {
  }
  UDR0 = c;
}

void uart_send_string(const char *str) {
  while (*str) {
    uart_send_char(*str);
    str++;
  }
}

void uart_send_hex(uint8_t num) {
  const char hex_chars[] = "0123456789ABCDEF";

  uart_send_char(hex_chars[(num >> 4) & 0x0F]);

  uart_send_char(hex_chars[num & 0x0F]);
}

void uart_send_dec(uint16_t num) {
  if (num == 0) {
    uart_send_char('0');
    return;
  }

  char buffer[6];
  uint8_t i = 0;

  while (num > 0) {
    buffer[i++] = (num % 10) + '0';
    num /= 10;
  }

  while (i > 0) {
    uart_send_char(buffer[--i]);
  }
}
// ---- Main ----

int main() {
  uart_init(16);
  DDRB |= (1 << DDB5);
  twi_init();
  _delay_ms(100);
  ssd1306_init();
  ssd1306_clear();
  uart_send_string("=== MPU6050 Test Started ===\r\n");
  uint8_t who_am_i = mpu6050_read_reg(0x75);
  uart_send_string("Reading WHO_AM_I register (0x75): 0x");
  uart_send_hex(who_am_i);
  uart_send_string("\r\n");
  if (who_am_i == 0x68) {
    uart_send_string("SUCCESS! MPU6050 is alive and responding correctly!\r\n");
  } else if (who_am_i == 0x00 || who_am_i == 0xFF) {
    uart_send_string(
        "FAILED! No response from I2C. Check wiring/soldering.\r\n");
  } else {
    uart_send_string("WARNING! Unexpected response. Did you have cheap chinese "
                     "microscheme? If not, Check AD0 pin.\r\n");
  }

  uart_send_string("==============================\r\n");
  for (uint16_t i = 0; i < 1024; ++i) {
    framebuffer[i] = 0xFF;
  }
  ssd1306_display();
  ssd1306_clear();
  ssd1306_draw_string_wrapped(
      0, 0, "HELLO, BARE METAL", 2,
      128); // max width - width of your screen(in my case its 128x64)
  ssd1306_display();
  while (1) {
    uint8_t found = 0;
    for (uint8_t i = 0x00; i <= 0xFE; i += 2) {
      if (twi_probe(i)) {
        found = 1;
        PORTB |= (1 << PORTB5);
        _delay_ms(100);
        PORTB &= ~(1 << PORTB5);
        _delay_ms(100);
      }
    }
    if (!found) {
      PORTB |= (1 << PORTB5);
      _delay_ms(500);
      PORTB &= ~(1 << PORTB5);
      _delay_ms(500);
    }
  }
}
