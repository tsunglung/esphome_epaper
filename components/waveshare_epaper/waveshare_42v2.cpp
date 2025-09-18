#include "waveshare_42v2.h"
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_4.2v2";

static const uint8_t LUT_ALL[233]={              
  0x01,  0x0A,  0x1B,  0x0F,  0x03,  0x01,  0x01,  
  0x05,  0x0A,  0x01,  0x0A,  0x01,  0x01,  0x01,  
  0x05,  0x08,  0x03,  0x02,  0x04,  0x01,  0x01,  
  0x01,  0x04,  0x04,  0x02,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x0A,  0x1B,  0x0F,  0x03,  0x01,  0x01,  
  0x05,  0x4A,  0x01,  0x8A,  0x01,  0x01,  0x01,  
  0x05,  0x48,  0x03,  0x82,  0x84,  0x01,  0x01,  
  0x01,  0x84,  0x84,  0x82,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x0A,  0x1B,  0x8F,  0x03,  0x01,  0x01,  
  0x05,  0x4A,  0x01,  0x8A,  0x01,  0x01,  0x01,  
  0x05,  0x48,  0x83,  0x82,  0x04,  0x01,  0x01,  
  0x01,  0x04,  0x04,  0x02,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x8A,  0x1B,  0x8F,  0x03,  0x01,  0x01,  
  0x05,  0x4A,  0x01,  0x8A,  0x01,  0x01,  0x01,  
  0x05,  0x48,  0x83,  0x02,  0x04,  0x01,  0x01,  
  0x01,  0x04,  0x04,  0x02,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x8A,  0x9B,  0x8F,  0x03,  0x01,  0x01,  
  0x05,  0x4A,  0x01,  0x8A,  0x01,  0x01,  0x01,  
  0x05,  0x48,  0x03,  0x42,  0x04,  0x01,  0x01,  
  0x01,  0x04,  0x04,  0x42,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x01,  0x00,  0x00,  0x00,  0x00,  0x01,  0x01,  
  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  
  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  
  0x02,  0x00,  0x00,  0x07,  0x17,  0x41,  0xA8,  
  0x32,  0x30,
};

int WaveshareEPaper4P2InV2::get_width_internal() { return 400; }

int WaveshareEPaper4P2InV2::get_height_internal() { return 300; }

uint32_t WaveshareEPaper4P2InV2::get_buffer_length_() {
  if (this->initial_mode_ == MODE_GRAYSCALE4) {
    // black and gray buffer
    return this->get_width_controller() * this->get_height_internal() / 4u;
  } else {
    // just a black buffer
    return this->get_width_controller() * this->get_height_internal() / 8u;
  }
}

void WaveshareEPaper4P2InV2::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

void WaveshareEPaper4P2InV2::initialize() {
  if (this->initial_mode_ == MODE_PARTIAL)
    this->initialize_internal_(MODE_FULL);
  else
    this->initialize_internal_(this->initial_mode_);
}

uint32_t WaveshareEPaper4P2InV2::idle_timeout_() {
  if (this->initial_mode_ == MODE_GRAYSCALE4) {
    return 1000;
  } else {
    return 100;
  }
}

void WaveshareEPaper4P2InV2::deep_sleep() {
  this->command(0x10);
  this->data(0x01);
  delay(200);
}

void WaveshareEPaper4P2InV2::reset_() {
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(100);
  this->reset_pin_->digital_write(false);  // Note: Should be inverted logic according to the docs
  delay(this->reset_duration_);
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(100);
}

void WaveshareEPaper4P2InV2::turn_on_display_(TurnOnMode mode) {
  this->command(0x22);
  switch (mode) {
    case MODE_GRAYSCALE4:
      this->data(0xcf);
      break;
    case MODE_PARTIAL:
      this->data(0xff);
      break;
    case MODE_FAST:
      this->data(0xc7);
      break;
    case MODE_FULL:
    default:
      this->data(0xf7);
      break;
  }
  this->command(0x20);

  // possible timeout.
  this->wait_until_idle_();
}

void WaveshareEPaper4P2InV2::update_(TurnOnMode mode) {
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  if (mode == MODE_PARTIAL) {
    this->command(0x21); 
    this->data(0x00);
    this->data(0x00);

    this->command(0x3C); 
    this->data(0x80); 

//    this->set_window_(0, 0, this->get_width_internal() - 1, this->get_height_internal() - 1);
//    this->set_cursor_(0, 0);
  } else if (mode == MODE_FULL) {
    this->command(0x21);
    this->data(0x40);
    this->data(0x00);

    this->command(0x3C);
    this->data(0x05);
  }

  this->command(0x24);
  this->start_data_();
  if (mode == MODE_GRAYSCALE4)
    this->write_array(this->buffer_, buf_half_len);
  else
    this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // new data
  if ((mode == MODE_FULL) || (mode == MODE_FAST)) {
    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  } else if (mode == MODE_GRAYSCALE4) {
    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_ + buf_half_len, buf_half_len);
    this->end_data_();
  }

  this->turn_on_display_(mode);

  this->deep_sleep();
  this->is_busy_ = false;
}

void WaveshareEPaper4P2InV2::set_window_(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2) {
  this->command(0x44);  // SET_RAM_X_ADDRESS_START_END_POSITION
  this->data((x >> 3) & 0xFF);
  this->data((x2 >> 3) & 0xFF);

  this->command(0x45);  // SET_RAM_Y_ADDRESS_START_END_POSITION
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
  this->data(62 & 0xFF);
  this->data((y2 >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::clear_() {
  uint32_t bufflen;
  if (this->initial_mode_ == MODE_GRAYSCALE4) {
    bufflen = this->get_buffer_length_() / 2u;
  } else {
    bufflen = this->get_buffer_length_();
  }
  uint8_t *buffer = (uint8_t *) calloc(bufflen, sizeof(uint8_t));
  memset(buffer, 0xff, bufflen);

  this->command(0x24);
  this->start_data_();
  this->write_array(buffer, bufflen);
  this->end_data_();

  this->command(0x26);
  this->start_data_();
  this->write_array(buffer, bufflen);
  this->end_data_();

  free(buffer);
}

void WaveshareEPaper4P2InV2::set_cursor_(uint16_t x, uint16_t y) {
  this->command(0x4E);  // SET_RAM_X_ADDRESS_COUNTER
  this->data(x & 0xFF);

  this->command(0x4F);  // SET_RAM_Y_ADDRESS_COUNTER
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::write_lut_() {
  this->command(0x32);
  this->start_data_();
  this->write_array(LUT_ALL, 227);
  this->end_data_();

  this->command(0x3F);
  this->data(LUT_ALL[227]);

  this->command(0x03);
  this->data(LUT_ALL[228]);

  this->command(0x04);
  this->data(LUT_ALL[229]);
  this->data(LUT_ALL[230]);
  this->data(LUT_ALL[231]);

  this->command(0x2c);
  this->data(LUT_ALL[232]);
}

void WaveshareEPaper4P2InV2::initialize_internal_(TurnOnMode mode) {
#define MODE_1_SECOND 1
#define MODE_1_5_SECOND 0

  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }
  this->command(0x12);  // soft  reset
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->command(0x21);
  if (mode == MODE_GRAYSCALE4)
    this->data(0x00);
  else
    this->data(0x40);
  this->data(0x00);


  this->command(0x3C);
  if (mode == MODE_GRAYSCALE4)
    this->data(0x03);
  else
    this->data(0x05);

  if (mode == MODE_FAST) {
#if MODE_1_5_SECOND
    // 1.5s
    this->command(0x1A);  // Write to temperature register
    this->data(0x6E);
#endif
#if MODE_1_SECOND
    // 1s
    this->command(0x1A);  // Write to temperature register
    this->data(0x5A);
#endif

    this->command(0x22);  // Load temperature value
    this->data(0x91);
    this->command(0x20);
    if (!this->wait_until_idle_()) {
      ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
    }
  } else if (mode == MODE_GRAYSCALE4) {
    this->command(0x0C);
    this->data(0x8B);
    this->data(0x9C);
    this->data(0xA4);
    this->data(0x0F);

    this->write_lut_();
  }

  this->command(0x11);	// data entry mode
  this->data(0x03);		// X-mode

  this->set_window_(0, 0, this->get_width_internal() - 1, this->get_height_internal() - 1);
  this->set_cursor_(0, 0);

  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->clear_();
  this->turn_on_display_(mode);
}

void WaveshareEPaper4P2InV2::display() {
  if (this->is_busy_ || (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()))
    return;
  this->is_busy_ = true;
  if (this->initial_mode_ == MODE_GRAYSCALE4) {
    ESP_LOGD(TAG, "Performing grayscale4 update");
    this->update_(MODE_GRAYSCALE4);
  } else if (this->initial_mode_ == MODE_FAST) {
      ESP_LOGD(TAG, "Performing fast update");
      this->update_(MODE_FAST);
  } else {
    const bool partial = this->at_update_ != 0;
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
    if (partial) {
      ESP_LOGD(TAG, "Performing partial update");
      this->update_(MODE_PARTIAL);
    } else {
      ESP_LOGD(TAG, "Performing full update");
      //this->init_();
      this->update_(MODE_FULL);
    }
  }
}

void HOT WaveshareEPaper4P2InV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  const uint32_t pos = (x + y * this->get_width_controller()) / 8u;
  const uint8_t subpos = x & 0x07;
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  if (!color.is_on()) {
    if (this->initial_mode_ == MODE_GRAYSCALE4) {
      // flip logic
      this->buffer_[pos] |= (0x80 >> subpos);
      this->buffer_[pos + buf_half_len] |= (0x80 >> subpos);
    } else {
      this->buffer_[pos] |= (0x80 >> subpos);
    }
  } else {
    if (this->initial_mode_ == MODE_GRAYSCALE4) {

/****Color display description****
      white  gray1  gray2  black
0x10|  01     01     00     00
0x13|  01     00     01     00
*********************************/

      if (((color.red > 0) || (color.green > 0) || (color.blue > 0))) {
        // draw gray pixels
        if ((color.red >= 0xc0) || (color.green >= 0xc0) || (color.blue >= 0xc0)) {
          // white
          this->buffer_[pos] &= ~(0x80 >> subpos);
          this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
        } else if ((color.red >= 0x80) || (color.green >= 0x80) || (color.blue >= 0x80)) {
          // gray 1
          this->buffer_[pos] |= 0x80 >> subpos;
          this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
        } else {
          // gray 2
          this->buffer_[pos] &= ~(0x80 >> subpos);
          this->buffer_[pos + buf_half_len] |= 0x80 >> subpos;
        }
      } else {
          // black
          this->buffer_[pos] |= 0x80 >> subpos;
          this->buffer_[pos + buf_half_len] |= 0x80 >> subpos;
      }
    } else {
      // flip logic
      if ((color.r > 0) || (color.g > 0) || (color.b > 0))
        this->buffer_[pos] |= (0x80 >> subpos);
      else
        this->buffer_[pos] &= ~(0x80 >> subpos);
    }
  }
}

void WaveshareEPaper4P2InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this)
  ESP_LOGCONFIG(TAG, "  Model: 4.20inV2");
  if (this->initial_mode_ == MODE_GRAYSCALE4)
    ESP_LOGCONFIG(TAG, "  Initial Mode: 4 Grayscale");
  else if (this->initial_mode_ == MODE_FAST)
    ESP_LOGCONFIG(TAG, "  Initial Mode: Fast");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome
