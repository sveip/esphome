#include "airthings_wave_plus.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace airthings_wave_plus {

void AirthingsWavePlus::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle = 0;
      auto chr = this->parent()->get_characteristic(service_uuid, sensors_data_characteristic_uuid);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", service_uuid.to_string().c_str(),
                 sensors_data_characteristic_uuid.to_string().c_str());
        break;
      }
      this->handle = chr->handle;
      this->node_state = espbt::ClientState::Established;

      request_read_values_();
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent()->conn_id)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->handle) {
        read_sensors_(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void AirthingsWavePlus::read_sensors_(uint8_t *raw_value, uint16_t value_len) {
  auto value = (WavePlusReadings *) raw_value;

  if (sizeof(WavePlusReadings) <= value_len) {
    ESP_LOGD(TAG, "version = %d", value->version);

    if (value->version == 1) {
      ESP_LOGD(TAG, "ambient light = %d", value->ambientLight);

      this->humidity_sensor_->publish_state(value->humidity / 2.0f);
      if (is_valid_radon_value_(value->radon)) {
        this->radon_sensor_->publish_state(value->radon);
      }
      if (is_valid_radon_value_(value->radon_lt)) {
        this->radon_long_term_sensor_->publish_state(value->radon_lt);
      }
      this->temperature_sensor_->publish_state(value->temperature / 100.0f);
      this->pressure_sensor_->publish_state(value->pressure / 50.0f);
      if (is_valid_co2_value_(value->co2)) {
        this->co2_sensor_->publish_state(value->co2);
      }
      if (is_valid_voc_value_(value->voc)) {
        this->tvoc_sensor_->publish_state(value->voc);
      }

      // This instance must not stay connected
      // so other clients can connect to it (e.g. the
      // mobile app).
      parent()->set_enabled(false);
    } else {
      ESP_LOGE(TAG, "Invalid payload version (%d != 1, newer version or not a Wave Plus?)", value->version);
    }
  }
}

bool AirthingsWavePlus::is_valid_radon_value_(short radon) { return 0 <= radon && radon <= 16383; }

bool AirthingsWavePlus::is_valid_voc_value_(short voc) { return 0 <= voc && voc <= 16383; }

bool AirthingsWavePlus::is_valid_co2_value_(short co2) { return 0 <= co2 && co2 <= 16383; }

void AirthingsWavePlus::loop() {}

void AirthingsWavePlus::update() {
  if (this->node_state != espbt::ClientState::Established) {
    if (!parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      parent()->set_enabled(true);
      parent()->connect();
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

void AirthingsWavePlus::request_read_values_() {
  auto status =
      esp_ble_gattc_read_char(this->parent()->gattc_if, this->parent()->conn_id, this->handle, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
  }
}

void AirthingsWavePlus::dump_config() {
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
}

AirthingsWavePlus::AirthingsWavePlus() : PollingComponent(10000) {
  auto service_bt = *BLEUUID::fromString(std::string("b42e1c08-ade7-11e4-89d3-123b93f75cba")).getNative();
  auto characteristic_bt = *BLEUUID::fromString(std::string("b42e2a68-ade7-11e4-89d3-123b93f75cba")).getNative();

  service_uuid = espbt::ESPBTUUID::from_uuid(service_bt);
  sensors_data_characteristic_uuid = espbt::ESPBTUUID::from_uuid(characteristic_bt);
}

void AirthingsWavePlus::setup() {}

}  // namespace airthings_wave_plus
}  // namespace esphome

#endif  // ARDUINO_ARCH_ESP32
