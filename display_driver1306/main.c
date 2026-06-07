#include <avr/io.h>
#include <util/delay.h>

// consts
#define SSD_1306_ADDR 0x78;
uint8_t framebuffer[1024];

void twi_init(void) {
  TWBR = 152; //  72/162
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
  ssd1306_command(0x80); // Значение по умолчанию
  ssd1306_command(0xA8); // Set Multiplex Ratio
  ssd1306_command(0x3F); // 1/64 duty (для 64px высоты)
  ssd1306_command(0xD3); // Set Display Offset
  ssd1306_command(0x00); // Нет смещения
  ssd1306_command(0x40); // Set Start Line (0x40 + 0)
  ssd1306_command(0x8D); // Charge Pump Setting (КРИТИЧНО ВАЖНО!)
  ssd1306_command(0x14); // Включить зарядовый насос (иначе экран будет черным)
  ssd1306_command(0x20); // Set Memory Addressing Mode
  ssd1306_command(0x00); // Horizontal Addressing Mode (удобно для буфера)
  ssd1306_command(
      0xA1); // Segment Re-map (A0 или A1 в зависимости от ориентации)
  ssd1306_command(0xC8); // COM Output Scan Direction
  ssd1306_command(0xDA); // Set COM Pins Hardware Configuration
  ssd1306_command(0x12);
  ssd1306_command(0x81); // Set Contrast Control
  ssd1306_command(0xCF); // Яркость
  ssd1306_command(0xA4); // Disable Entire Display On
  ssd1306_command(0xA6); // Normal Display (не инвертированный)
  ssd1306_command(0xAF); // Display ON
}

void ssd1306_clear(void) {
  for (uint16_t i = 0; i < 1024; ++i) {
    framebuffer[i] = 0x00;
  }
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

int main() {
  DDRB |= (1 << DDB5);
  twi_init();
  ssd1306_init();
  ssd1306_clear();

  for (uint16_t i = 0; i < 1024; ++i) {
    framebuffer[i] = 0xFF;
  }
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
