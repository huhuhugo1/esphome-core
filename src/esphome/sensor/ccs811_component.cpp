#include "esphome/defines.h"

#ifdef USE_CCS811_SENSOR

#include "esphome/sensor/ccs811_component.h"

#include "esphome/log.h"

ESPHOME_NAMESPACE_BEGIN

namespace sensor {

static const char *TAG = "sensor.ccs811";
constexpr uint8_t MEAS_MODE = 0x01;
constexpr uint8_t ALG_RESULT_DAT = 0x02;
constexpr uint8_t DRIVE_MODE = 0x01 << 4;

CCS811Component::CCS811Component(I2CComponent *parent, const std::string &eco2_name,
                                 const std::string &tvoc_name, uint32_t update_interval)
  : PollingComponent(update_interval), I2CDevice(parent, SENSOR_ADDR),
    eco2_(new CCS811eCO2Sensor(eco2_name, this)), tvoc_(new CCS811TVOCSensor(tvoc_name, this)) {}

void CCS811Component::setup() {
  auto returnCode = this->sensor.begin();
  ESP_LOGCONFIG(TAG, "Setting up CCS811... Return code: %d", returnCode);
}

void CCS811Component::update() {
  if (this->sensor.dataAvailable()) {
    this->sensor.readAlgorithmResults();
    const auto co2 = this->sensor.getCO2();
    const auto tvoc = this->sensor.getTVOC();
    this->eco2_->publish_state(co2);
    this->tvoc_->publish_state(tvoc);
  }
}

void CCS811Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CCS811:");
}

float CCS811Component::get_setup_priority() const {
  return setup_priority::HARDWARE_LATE;
}

CCS811eCO2Sensor *CCS811Component::get_eco2_sensor() const {
  return this->eco2_;
}
CCS811TVOCSensor *CCS811Component::get_tvoc_sensor() const {
  return this->tvoc_;
}

} // namespace sensor

ESPHOME_NAMESPACE_END

#endif //USE_CCS811_SENSOR
