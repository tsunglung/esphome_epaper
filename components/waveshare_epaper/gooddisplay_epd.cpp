#include "gooddisplay_epd.h"
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "gooddisplay_epd";

int GoodDisplayGDEY075Z08::get_width_internal() { return 800; }

int GoodDisplayGDEY075Z08::get_height_internal() { return 480; }

uint32_t GoodDisplayGDEY075Z08::get_buffer_length_() {
  // black and red buffer
  return this->get_width_controller() * this->get_height_internal() / 4u;
}

void GoodDisplayGDEY075Z08::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

void GoodDisplayGDEY075Z08::deep_sleep() {
  this->command(0x50);  //VCOM AND DATA INTERVAL SETTING
  this->data(0xf7); //WBmode:VBDF 17|D7 VBDW 97 VBDB 57    WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7

  this->command(0x02);  //power off
  //waiting for the electronic paper IC to release the idle signal
  this->wait_until_idle_();
  delay(100);    //!!!The delay here is necessary, 200uS at least!!!
  this->command(0x07);  //deep sleep
  this->data(0xa5);
}

void GoodDisplayGDEY075Z08::reset_() {
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(1);
  this->reset_pin_->digital_write(false);  // Note: Should be inverted logic according to the docs
  delay(this->reset_duration_);
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(10);
}

void GoodDisplayGDEY075Z08::turn_on_display_(RefreshMode mode) {
  this->command(0x12);
  delay(1);
  // possible timeout.
  this->wait_until_idle_();
}

void GoodDisplayGDEY075Z08::update_(RefreshMode mode) {
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  if (mode == PARTIAL_REFRESH) {
	  uint32_t x_start = 0, y_start = 0, x_end, y_end;
    x_end = x_start + this->get_width_internal() - 1;
    y_end = y_start + this->get_height_internal() - 1;

    this->command(0x50);
    this->data(0xA9);
    this->data(0x07);

    this->command(0x91);		// This command makes the display enter partial mode
    this->command(0x90);		// resolution setting
    this->data(x_start / 256);
    this->data(x_start % 256);   //x-start

    this->data(x_end / 256);
    this->data(x_end % 256-1);  //x-end

    this->data(y_start / 256);  //
    this->data(y_start % 256);   //y-start

    this->data(y_end / 256);
    this->data(y_end % 256 - 1);  //y-end
    this->data(0x01);

    this->command(0x13);				 //writes New data to SRAM.
    this->start_data_();
    this->write_array(this->buffer_ + buf_half_len, buf_half_len);
    this->end_data_();
  } else {
    this->command(0x50);      // VCOM AND DATA INTERVAL SETTING
    this->data(0x11);  // 0x10  --------------
    this->data(0x07);

    this->command(0x61);  //resolution setting
    this->data(this->get_width_internal() / 256);
    this->data(this->get_width_internal() % 256);
    this->data(this->get_height_internal() / 256);
    this->data(this->get_height_internal() % 256);

    this->command(0x10);         //  Transfer old data (KW mode) or K/W data (KWR mode)
    this->start_data_();
    this->write_array(this->buffer_, buf_half_len);  // Transfer the actual displayed data
    this->end_data_();

    this->command(0x13);         // Transfer new data (KW mode) or RED data (KWR mode)
    this->start_data_();
    this->write_array(this->buffer_ + buf_half_len, buf_half_len);  // Transfer the actual displayed data
    this->end_data_();
  }

  this->turn_on_display_(mode);

  this->deep_sleep();
  this->is_busy_ = false;
}

void GoodDisplayGDEY075Z08::display() {
  if (this->is_busy_)
    return;
  this->is_busy_ = true;

  const bool partial = this->at_update_ != 0;
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  if (partial) {
    ESP_LOGD(TAG, "Performing partial update");
    this->update_(PARTIAL_REFRESH);
  } else {
    ESP_LOGD(TAG, "Performing full update");
    this->update_(FULL_REFRESH);
  }
}

void GoodDisplayGDEY075Z08::clear_() {
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;
  uint8_t *buffer = (uint8_t *) calloc(buf_half_len, sizeof(uint8_t));

  memset(buffer, 0xff, buf_half_len);
  this->command(0x10);
  this->start_data_();
  this->write_array(buffer, buf_half_len);
  this->end_data_();

  memset(buffer, 0x00, buf_half_len);
  this->command(0x13);
  this->start_data_();
  this->write_array(buffer, buf_half_len);
  this->end_data_();

  this->turn_on_display_(FULL_REFRESH);

  free(buffer);
}

void GoodDisplayGDEY075Z08::initialize_internal_(RefreshMode mode) {
  // EPD hardware init start
  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->command(0x01);  // POWER SETTING
  this->data(0x07);
  this->data(0x07);  // VGH=20V,VGL=-20V
  this->data(0x3f);  // VDH=15V
  this->data(0x3f);  // VDL=-15V

  // Enhanced display drive(Add 0x06 command)
  if (this->initial_mode_ == FAST_REFRESH) {
    this->command(0x06);  // Booster Soft Start
    this->data(0x27);
    this->data(0x27);
    this->data(0x18);
    this->data(0x17);
  } else {
    this->command(0x06);  // Booster Soft Start
    this->data(0x17);
    this->data(0x17);
    this->data(0x28);
    this->data(0x17);
  }

  this->command(0x04); // POWER ON
  // waiting for the electronic paper IC to release the idle signal
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->command(0x00);  // PANNEL SETTING
  this->data(0x0F);  // KW-3f   KWR-2F  BWROTP 0f  BWOTP 1f

  this->command(0x61);  //resolution setting
  this->data(this->get_width_internal() / 256);
  this->data(this->get_width_internal() % 256);
  this->data(this->get_height_internal() / 256);
  this->data(this->get_height_internal() % 256);

  if (this->initial_mode_ == FAST_REFRESH) {
    this->command(0xE0);
    this->data(0x02);
    this->command(0xE5);
    this->data(0x5A);
  } else {
    this->command(0x15);
    this->data(0x00);
  }

  this->command(0x50);      // VCOM AND DATA INTERVAL SETTING
  this->data(0x11);  // 0x10  --------------
  this->data(0x07);

  this->command(0x60);      // TCON SETTING
  this->data(0x22);
}

void GoodDisplayGDEY075Z08::initialize() {
  this->set_mode(esphome::spi::MODE0);
  this->set_data_rate(10000000);
  this->initialize_internal_(this->initial_mode_);
//  this->clear_();
  this->set_auto_clear(false);
}

void HOT GoodDisplayGDEY075Z08::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (this->buffer_ == nullptr)
    return;
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = x & 0x07;

  // flip logic
  if ((color.red > 0) || (color.green > 0) || (color.blue > 0) || (color.white > 0))
    this->buffer_[pos] &= ~(0x80 >> subpos);
  else
    this->buffer_[pos] |= 0x80 >> subpos;

  // draw red pixels only, if the color contains red only
  if (((color.red > 0) && (color.green == 0) && (color.blue == 0))) {
    this->buffer_[pos + buf_half_len] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
  }
}

void GoodDisplayGDEY075Z08::fill(Color color) {
  this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color);
}

void GoodDisplayGDEY075Z08::dump_config() {
  LOG_DISPLAY("", "GoodDisplay e-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: GDEY075Z08");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
  ESP_LOGCONFIG(TAG, "  SPI Mode: %d", this->mode_);
}


int GoodDisplayGDEM075F52::get_width_internal() { return 800; }

int GoodDisplayGDEM075F52::get_height_internal() { return 480; }

uint32_t GoodDisplayGDEM075F52::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 4u;
}

uint32_t GoodDisplayGDEM075F52::idle_timeout_() { return 13000; }

bool GoodDisplayGDEM075F52::wait_until_idle_() {
  if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(1);
  }
  return true;
};


void GoodDisplayGDEM075F52::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

void GoodDisplayGDEM075F52::deep_sleep() {
  this->command(0x02);  	//power off
  this->data(0x00);
  this->wait_until_idle_();          //waiting for the electronic paper IC to release the idle signal

  this->command(0x07);  	//deep sleep
  this->data(0xA5);
}

void GoodDisplayGDEM075F52::reset_() {
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(10);
  this->reset_pin_->digital_write(false);  // Note: Should be inverted logic according to the docs
  delay(this->reset_duration_);
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(10);
}

void GoodDisplayGDEM075F52::turn_on_display_(RefreshMode mode) {
  this->command(0x12);
  this->data(0x00);

  // possible timeout.
  this->wait_until_idle_();
}

void GoodDisplayGDEM075F52::update_(RefreshMode mode) {
  const uint32_t buf_len = this->get_buffer_length_();

  this->command(0x10);         //  Transfer old data (KW mode) or K/W data (KWR mode)
  this->start_data_();
  this->write_array(this->buffer_, buf_len);  // Transfer the actual displayed data
  this->end_data_();

  this->turn_on_display_(mode);

  this->deep_sleep();
  this->is_busy_ = false;
}

void GoodDisplayGDEM075F52::display() {
  if (this->is_busy_)
    return;
  this->is_busy_ = true;

  const bool fast = this->at_update_ != 0;
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  if (fast) {
    ESP_LOGD(TAG, "Performing fast update");
    this->initialize_internal_(this->initial_mode_, true);
    this->update_(FAST_REFRESH);
  } else {
    ESP_LOGD(TAG, "Performing full update");
    this->initialize_internal_(this->initial_mode_, true);
    this->update_(FULL_REFRESH);
  }
}

void GoodDisplayGDEM075F52::clear_() {
  const uint32_t buf_len = this->get_buffer_length_();
  uint8_t *buffer = (uint8_t *) calloc(buf_len, sizeof(uint8_t));

  // Black: 0x00, White: 0x55, Yellow: 0xAA, Red: 0xFF
  memset(buffer, 0x55, buf_len);
  this->command(0x10);
  this->start_data_();
  this->write_array(buffer, buf_len);
  this->end_data_();

  this->turn_on_display_(FULL_REFRESH);

  free(buffer);
}

void GoodDisplayGDEM075F52::initialize_internal_(RefreshMode mode, bool update_only) {
  // EPD hardware init start
  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  if (!update_only) {
    this->command(0x00);  // PANNEL SETTING
    this->data(0x0F);  // KW-3f   KWR-2F  BWROTP 0f  BWOTP 1f
    this->data(0x29);

    // Enhanced display drive(Add 0x06 command)
    this->command(0x06);  // Booster Soft Start
    this->data(0x0F);
    this->data(0x8B);
    this->data(0x93);
    this->data(0xA1);

    this->command(0x41);
    this->data(0x00);

    this->command(0x50);      // VCOM AND DATA INTERVAL SETTING
    this->data(0x37);

    this->command(0x60);      // TCON SETTING
    this->data(0x02);
    this->data(0x02);

    this->command(0x61);  //resolution setting
    this->data(this->get_width_internal() / 256);
    this->data(this->get_width_internal() % 256);
    this->data(this->get_height_internal() / 256);
    this->data(this->get_height_internal() % 256);

    this->command(0x62);
    this->data(0x98);
    this->data(0x98);
    this->data(0x98);
    this->data(0x75);
    this->data(0xCA);
    this->data(0xB2);
    this->data(0x98);
    this->data(0x7E);

    this->command(0x65);	//0x65
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);

    this->command(0xE7);	//0xE7
    this->data(0x1C);

    this->command(0xE3);	//0xE3
    this->data(0x00);

    this->command(0xE9);
    this->data(0x01);

    this->command(0x30);// frame go with waveform
    this->data(0x08);
  }

  this->command(0x04); //Power on
  // waiting for the electronic paper IC to release the idle signal
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  if (this->initial_mode_ == FAST_REFRESH) {
    this->command(0xE0);
    this->data(0x02);

    this->command(0xE6);
    this->data(0x5A);

    this->command(0xA5);
    this->data(0x00);
    this->wait_until_idle_();
  }

}

void GoodDisplayGDEM075F52::initialize() {
  this->set_mode(esphome::spi::MODE0);
  this->set_data_rate(10000000);
  this->initialize_internal_(this->initial_mode_, false);
  // this->clear_();
  if (this->buffer_ == nullptr)
    return;
  memset(this->buffer_, 0x55, this->get_buffer_length_());

  this->set_auto_clear(false);
}

uint8_t GoodDisplayGDEM075F52::color_to_hex_(Color color) {
  uint8_t hex_code;
  if (color.red > 127) {
    if (color.green > 170) {
      if (color.blue > 127) {
        hex_code = 0x1;  // White
      } else {
        hex_code = 0x2;  // Yellow
      }
    } else {
      hex_code = 0x3;  // Red (or Magenta)
    }
  } else {
    if (color.green > 127) {
      if (color.blue > 127) {
        hex_code = 0x2;  // Yellow
      } else {
        hex_code = 0x2;  // Yellow
      }
    } else {
      if (color.blue > 127) {
        hex_code = 0x2;  // Yellow
      } else {
        hex_code = 0x0;  // Black
      }
    }
  }

  return hex_code;
}

void HOT GoodDisplayGDEM075F52::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_controller()) / 4u;
  const uint8_t subpos = 6 - (x & 0x03) * 2;
  uint8_t pixel_bits = this->color_to_hex_(color);

    /****Color display description****
          white  red   yellow  black
          0x55   0xFF   0xAA    0x00
#define black   0x00  /// 00
#define white   0x01  /// 01
#define yellow  0x02  /// 10
#define red     0x03  /// 11
    *********************************/

  // flip logic
  if (!color.is_on()) {
      this->buffer_[pos] &= ~(0x3 << subpos);
  } else {
    switch(pixel_bits) {
      case 0x03:
        this->buffer_[pos] |= 0x3 << subpos;
        break;
      case 0x02:
        this->buffer_[pos] &= ~(0x1 << subpos);
        this->buffer_[pos] |= 0x2 << subpos;
        break;
      case 0x01:
        this->buffer_[pos] &= ~(0x2 << subpos);
        this->buffer_[pos] |= 0x1 << subpos;
        break;
      default:
      case 0x00:
        this->buffer_[pos] &= ~(0x3 << subpos);
        break;
    }
  }

  return;
}

void GoodDisplayGDEM075F52::fill(Color color) {
  this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color);
}

void GoodDisplayGDEM075F52::dump_config() {
  LOG_DISPLAY("", "GoodDisplay e-Paper", this)
  ESP_LOGCONFIG(TAG, "  Model: GDEM075F52");
  LOG_PIN("  CS Pin: ", this->cs_)
  LOG_PIN("  Reset Pin: ", this->reset_pin_)
  LOG_PIN("  DC Pin: ", this->dc_pin_)
  LOG_PIN("  Busy Pin: ", this->busy_pin_)
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
  ESP_LOGCONFIG(TAG, "  SPI Mode: %d", this->mode_);
}

}  // namespace waveshare_epaper
}  // namespace esphome
