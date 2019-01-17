#include "esphomelib/defines.h"

#ifdef USE_ESP32_CAMERA

#include "esphomelib/esp32_camera.h"
#include "esphomelib/log.h"
#include "esp32_camera.h"

ESPHOMELIB_NAMESPACE_BEGIN

static const char *TAG = "esp32_camera";

ESP32Camera *global_esp32_camera;

void ESP32Camera::setup() {
  global_esp32_camera = this;

  camera_config_t config{};
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sscb_sda = 25;
  config.pin_sscb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 17; //32;
  config.pin_vsync = 22;
  config.pin_href = 26;
  config.pin_pclk = 21;
  config.xclk_freq_hz = 20000000;
  ESP_LOGI(TAG, "psramFound: %d, ", psramFound());
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;
  // no psram
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  // with psram
  // config.frame_size = FRAMESIZE_UXGA;
  // config.jpeg_quality = 10;
  // config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, true);
  s->set_hmirror(s, true);
  auto st = s->status;
  ESP_LOGI(TAG, "status: framesize=%d quality=%u brightness=%d contrast=%d saturation=%d special_effect=%d",
           st.framesize, st.quality, st.brightness, st.contrast, st.saturation, st.special_effect);
  ESP_LOGI(TAG, "        wb_mode=%u awb=%u awb_gain=%u aec=%u aec2=%u ae_level=%d aec_value=%u agc=%u agc_gain=%u",
           st.wb_mode, st.awb, st.awb_gain, st.aec, st.aec2, st.ae_level, st.aec_value, st.agc, st.agc_gain);
  ESP_LOGI(TAG, "        gainceiling=%u bpc=%u wpc=%u raw_gma=%u lenc=%u hmirror=%u vflip=%u dcw=%u colorbar=%u",
           st.gainceiling, st.bpc, st.wpc, st.raw_gma, st.lenc, st.hmirror, st.vflip, st.dcw, st.colorbar);
  ESP_LOGI(TAG, "        id={MIDH=%u,MIDL=%u,PID=%u,VER=%u} slv_addr=%u pixformat=%d",
           s->id.MIDH, s->id.MIDL, s->id.PID, s->id.VER, s->slv_addr, s->pixformat);

  this->framebuffer_get_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
  this->framebuffer_return_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
  xTaskCreatePinnedToCore(
      &ESP32Camera::framebuffer_task_,
      "framebuffer_task", // name
      4096,  // stack size
      nullptr, // task pv params
      0, // priority
      nullptr, // handle
      1 // core
  );
}
void ESP32Camera::update() {
  if (this->framebuffer_ != nullptr && this->framebuffer_uses_ != 0) {
    // Image not finished transmitting
    return;
  }

  if (this->framebuffer_ != nullptr) {
    // return image
    uint8_t hash = 0;
    for (uint32_t i = 0; i < this->framebuffer_->len; i++) {
      hash ^= framebuffer_->buf[i];
    }
    ESP_LOGW(TAG, "Returning framebuffer 0x%02X!", hash);
    xQueueSend(this->framebuffer_return_queue_, &this->framebuffer_, portMAX_DELAY);
    this->framebuffer_ = nullptr;
  }

  if (xQueueReceive(this->framebuffer_get_queue_, &this->framebuffer_, 0L) != pdTRUE) {
    // no frame ready
    ESP_LOGV(TAG, "No frame ready");
    return;
  }

  if (this->framebuffer_ == nullptr) {
    ESP_LOGW(TAG, "Got invalid frame from camera!");
    xQueueSend(this->framebuffer_return_queue_, &this->framebuffer_, portMAX_DELAY);
    return;
  }

  uint8_t hash = 0;
  for (uint32_t i = 0; i < this->framebuffer_->len; i++) {
    hash ^= framebuffer_->buf[i];
  }
  ESP_LOGD(TAG, "Got Image: %p len=%u width=%u height=%u format=%u hash=0x%02x",
           framebuffer_->buf, framebuffer_->len, framebuffer_->width, framebuffer_->height, framebuffer_->format,
           hash);
  this->callback_.call();
}
void ESP32Camera::dump_config() {

}
float ESP32Camera::get_setup_priority() const {
  return setup_priority::POST_HARDWARE;
}

void ESP32Camera::framebuffer_task_(void *pv) {
  while (true) {
    camera_fb_t *framebuffer = esp_camera_fb_get();
    xQueueSend(global_esp32_camera->framebuffer_get_queue_, &framebuffer, portMAX_DELAY);
    xQueueReceive(global_esp32_camera->framebuffer_return_queue_, &framebuffer, portMAX_DELAY);
    esp_camera_fb_return(framebuffer);
  }
}
void ESP32Camera::add_on_image_callback(std::function<void()> f) {
  this->callback_.add(std::move(f));
}
uint32_t ESP32Camera::hash_base_() {
  return 3021486383U;
}
ESP32Camera::ESP32Camera(const std::string &name) : Nameable(name), PollingComponent(500) {}

void ESP32Camera::acquire(CameraImage *image) {
  image->offset_ = 0;
  if (this->framebuffer_ == nullptr) {
    image->len_ = 0;
    image->buffer_ = 0;
    return;
  }
  image->len_ = this->framebuffer_->len;
  image->buffer_ = this->framebuffer_->buf;
  this->framebuffer_uses_++;
}

CameraImage::CameraImage() : len_(0), buffer_(nullptr), parent_(nullptr), returned_(true) {

}
void CameraImage::return_image() {
  if (!this->returned_) {
    this->parent_->framebuffer_uses_--;
    this->returned_ = true;
  }
}
bool CameraImage::has_image() {
  return !this->returned_;
}
void CameraImage::consume_at_most(uint32_t len, uint8_t **buf, uint32_t *consumed, bool *done) {
  if (!this->has_image()) {
    *buf = nullptr;
    *consumed = 0;
    *done = false;
    return;
  }

  uint32_t available = this->len_ - this->offset_;
  *consumed = std::min(len, available);
  *buf = this->buffer_ + this->offset_;
  this->offset_ += *consumed;
  if (*consumed == available) {
    *done = true;
    this->return_image();
  } else {
    *done = false;
  }
}
ESP32Camera *CameraImage::get_parent() const {
  return this->parent_;
}
void CameraImage::acquire(ESP32Camera *parent) {
  this->return_image();
  this->parent_ = parent;
  this->parent_->acquire(this);
  this->returned_ = false;
}
CameraImage::~CameraImage() {
  this->return_image();
}

ESPHOMELIB_NAMESPACE_END

#endif //USE_ESP32_CAMERA
