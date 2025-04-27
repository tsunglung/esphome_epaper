#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

enum RefreshMode {
  PARTIAL_REFRESH = 0,
  FULL_REFRESH,
  FAST_REFRESH,
};

class GoodDisplayGDEY075Z08 : public WaveshareEPaper {
 public:
  void dump_config() override;
  void display() override;
  void initialize() override;
  void deep_sleep() override;
  void fill(Color color) override;

  void set_full_update_every(uint32_t full_update_every);
  void set_initial_mode(uint8_t mode) { this->initial_mode_ = (RefreshMode)mode; }

 protected:
  void initialize_internal_(RefreshMode mode);
  void update_(RefreshMode mode);
  void turn_on_display_(RefreshMode mode);

  void reset_();
  void clear_();

  int get_width_internal() override;
  int get_height_internal() override;
  uint32_t get_buffer_length_() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  bool is_busy_{false};
  RefreshMode initial_mode_{FAST_REFRESH};
};

class GoodDisplayGDEM075F52 : public WaveshareEPaper {
 public:
  void dump_config() override;
  void display() override;
  void initialize() override;
  void deep_sleep() override;
  void fill(Color color) override;

  void set_full_update_every(uint32_t full_update_every);
  void set_initial_mode(uint8_t mode) { this->initial_mode_ = (RefreshMode)mode; }

 protected:
  void initialize_internal_(RefreshMode mode, bool update_only = false);
  void update_(RefreshMode mode);
  void turn_on_display_(RefreshMode mode);
  bool wait_until_idle_();
  uint32_t idle_timeout_(void);
  uint8_t color_to_hex_(Color color);

  void reset_();
  void clear_();

  int get_width_internal() override;
  int get_height_internal() override;
  uint32_t get_buffer_length_() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  bool is_busy_{false};
  RefreshMode initial_mode_{FAST_REFRESH};
};

}  // namespace waveshare_epaper
}  // namespace esphome