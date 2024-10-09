#include "gooddisplay_gdey075z08.h"
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "gooddisplay_gdey075z08";

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
  if (this->is_busy_ || (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()))
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
  LOG_DISPLAY("", "GoodDisplay e-Paper", this)
  ESP_LOGCONFIG(TAG, "  Model: GDEY075Z08");
  LOG_PIN("  CS Pin: ", this->cs_)
  LOG_PIN("  Reset Pin: ", this->reset_pin_)
  LOG_PIN("  DC Pin: ", this->dc_pin_)
  LOG_PIN("  Busy Pin: ", this->busy_pin_)
  LOG_UPDATE_INTERVAL(this)
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
  ESP_LOGCONFIG(TAG, "  SPI Mode: %d", this->mode_);
}

}  // namespace waveshare_epaper
}  // namespace esphome