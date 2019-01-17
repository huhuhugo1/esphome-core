#ifndef ESPHOMELIB_ESP32_CAMERA_H
#define ESPHOMELIB_ESP32_CAMERA_H

#include "esphomelib/defines.h"

#ifdef USE_ESP32_CAMERA

#include "esphomelib/component.h"
#include "esp_camera.h"

ESPHOMELIB_NAMESPACE_BEGIN

class ESP32Camera;

class CameraImage {
 public:
  CameraImage();
  ~CameraImage();
  void acquire(ESP32Camera *parent);
  void return_image();
  bool has_image();
  void consume_at_most(uint32_t len, uint8_t **buf, uint32_t *consumed, bool *done);
  ESP32Camera *get_parent() const;

 protected:
  friend ESP32Camera;

  uint32_t len_;
  uint32_t offset_;
  uint8_t *buffer_;
  ESP32Camera *parent_;
  bool returned_{false};
};

class ESP32Camera : public PollingComponent, public Nameable {
 public:
  ESP32Camera(const std::string &name);
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void add_on_image_callback(std::function<void()> f);
  void acquire(CameraImage *image);
 protected:
  friend CameraImage;

  uint32_t hash_base_() override;

  static void framebuffer_task_(void *pv);
  camera_fb_t *framebuffer_{nullptr};
  int framebuffer_uses_{0};
  QueueHandle_t framebuffer_get_queue_;
  QueueHandle_t framebuffer_return_queue_;
  CallbackManager<void()> callback_;
};

extern ESP32Camera *global_esp32_camera;

ESPHOMELIB_NAMESPACE_END

#endif //USE_ESP32_CAMERA

#endif //ESPHOMELIB_ESP32_CAMERA_H
