#include "dhwstate.h"

#include <cstring>
#include <sstream>

#include <SimpleKalmanFilter.h>
#include <Arduino.h>

#include "constants.h"
#include "mqtttask.h"
#include "util.h"

namespace aquamqtt
{

#define STATE_UNKNOWN "unknown"
#define STATE_BINARY_ON "on"
#define STATE_BINARY_OFF "off"


char *device_id = nullptr;
const char* unique_device_id() {
    if (!device_id) {
        uint64_t chipid;
        chipid = ESP.getEfuseMac();
        size_t n = snprintf(nullptr, 0, "%08x", (uint32_t)chipid) + 1;
        device_id = new char[n];
        snprintf(device_id, n, "%08x", (uint32_t)chipid);
    }
    return device_id;
}

/*** ENTITY ***/

Entity::Entity(DhwState* _device, const char* _entity_id, const char* _name, bool _is_diagnostic)
    : device(_device)
    , entity_id(_entity_id)
    , name(_name)
    , is_diagnostic(_is_diagnostic)
{
    snprintf(unique_id, MAX_UNIQUE_ID_SIZE, "%s_%s", unique_device_id(), _entity_id);
    snprintf(state_topic, MAX_STATE_TOPIC_SIZE, "homeassistant/%s/state", unique_id);
    
    _def["uniq_id"] = unique_id; // unique_id
    _def["name"] = name;
    _def["stat_t"] = state_topic; // state_topic

    if (is_diagnostic) {
        _def["ent_cat"] = "diagnostic"; // entity_category
    }
}

const char* Entity::state()
{
    if (_has_value) {
        return _state.c_str();
    } else {
        return STATE_UNKNOWN;
    }
}

void Entity::unset_value()
{
    _has_value = false;
}

void Entity::update_state(const char* new_state)
{
    if (!_has_value && new_state == nullptr) {
        return; // no change
    }

    if (_has_value && new_state != nullptr && !strcmp(_state.c_str(), new_state)) {
        return; // no change
    }

    // state changed -> update values and notify MQTT
    _has_value = (new_state != nullptr);
    if (new_state) {
        _has_value = true;
        _state = new_state;
    } else {
        _has_value = false;
    }

    MqttTaskV5::getInstance().queueUpdateEntity(this);
}

void Entity::writeDefinition(std::stringstream& s)
{
    s << "\"p\":\"" << getPlatform() << "\",";

    for (auto it = _def.begin(); it != _def.end(); it++) {
        s << "\"" << it->first << "\":\"" << it->second << "\",";
    }
}

/*** BINARYSENSOR ***/

BinarySensor::BinarySensor(DhwState* _device, const char* _entity_id, const char* _name, bool _is_diagnostic)
    : Entity(_device, _entity_id, _name, _is_diagnostic)
    , value(false)
{
}

void BinarySensor::set_value(const bool new_value)
{
    value = new_value;
    update_state(value ? STATE_BINARY_ON : STATE_BINARY_OFF);
}

void BinarySensor::set_state(const char* new_state)
{
    if (new_state == nullptr) {
        update_state(nullptr);
    } else if (!strcmp(new_state, STATE_BINARY_ON)) {
        set_value(true);
    } else if (!strcmp(new_state, STATE_BINARY_OFF)) {
        set_value(false);
    } else {
        update_state(nullptr);
    }
}

/*** SWITCH ***/

Switch::Switch(DhwState* _device, const char* _entity_id, const char* _name, bool _is_diagnostic)
    : BinarySensor(_device, _entity_id, _name, _is_diagnostic)
{
    snprintf(command_topic, MAX_STATE_TOPIC_SIZE, "homeassistant/%s/set", getUniqueId());

    _def["command_topic"] = command_topic;

    MqttTaskV5::getInstance().registerCommandTopic(this);
}

/*** SENSOR ***/

Sensor::Sensor(DhwState* _device, const char* _entity_id, const char* _name, bool _is_diagnostic)
    : Entity(_device, _entity_id, _name, _is_diagnostic)
{
    _def["dev_cla"] = "temperature"; // device_class
    _def["unit_of_meas"] = "°C"; // unit_of_measurement
}

void Sensor::set_value(const float new_value)
{
    value = new_value;

    size_t n = snprintf(nullptr, 0, format, value) + 1;
    char buf[n];
    snprintf(buf, n, format, value);

    update_state(buf);
}

void Sensor::setFormat(const char* fmt)
{
    format = fmt;
}

void Sensor::set_state(const char* new_state)
{
    if (new_state == nullptr) {
        update_state(nullptr);
    } else {
        set_value(strtod(new_state, nullptr));
    }
}

/*** FILTEREDSENSOR ***/

class FilteredSensor: public Sensor
{
private:
    SimpleKalmanFilter filter; // See: https://github.com/denyssene/SimpleKalmanFilter
public:

    FilteredSensor(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~FilteredSensor() = default;

    virtual void set_value(const float new_value);
};

FilteredSensor::FilteredSensor(DhwState* _device, const char* _entity_id, const char* _name, bool _is_diagnostic)
    : Sensor(_device, _entity_id, _name, _is_diagnostic)
    , filter(config::KALMAN_MEA_E, config::KALMAN_EST_E, config::KALMAN_Q)
{
    setFormat("%.1f");
}

void FilteredSensor::set_value(const float new_value)
{
    float filtered = filter.updateEstimate(new_value);
    Sensor::set_value(filtered);
}

/*** SELECT_ENTITY ***/

SelectEntity::SelectEntity(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic)
    : Entity(device_id, entity_id, name, is_diagnostic)
{
    snprintf(command_topic, MAX_STATE_TOPIC_SIZE, "homeassistant/%s/set", getUniqueId());

    _def["device_class"] = "enum";
    _def["command_topic"] = command_topic;

    MqttTaskV5::getInstance().registerCommandTopic(this);
}

SelectEntity& SelectEntity::addOption(const char* value)
{
    options.push_back(value);
    return *this;
}

void SelectEntity::set_value(const int new_value)
{
    if (new_value < 0 || new_value >= options.size()) {
        value = -1;
        update_state(nullptr);
    } else {
        value = new_value;
        update_state(options[value].c_str());
    }
}

void SelectEntity::set_state(const char* state)
{
    for (int i=0 ; i < options.size() ; i++) {
        if (!strcmp(state, options[i].c_str())) {
            set_value(i);
            return;
        }
    }
}

void SelectEntity::writeDefinition(std::stringstream& s)
{
    Entity::writeDefinition(s);

    s << "\"options\":[";
    for (auto it=options.begin() ; it!=options.end() ; it++) {
        if (it != options.begin()) {
            s << ",";
        }
        s << "\"" << it->c_str() << "\"";
    }
    s << "],";
}

/*** DHWSTATE ***/

DhwState& DhwState::getInstance() {
    static DhwState instance(unique_device_id(), HA_DEVICE_NAME);
    return instance;
}

DhwState::DhwState(const char* _device_id, const char* _device_name)
    : device_id(_device_id)
    , device_name(_device_name)
{
    entities = {
        water_temperature = new FilteredSensor(this, "water_temperature", "Water Temperature", false),
        water_temperature_min = new Sensor(this, "water_temperature_min", "Water Temperature (min)", true),
        water_temperature_max = new Sensor(this, "water_temperature_max", "Water Temperature (max)", true),

        compressor_outlet_temperature = new FilteredSensor(this, "compressor_outlet_temperature", "Compressor Outlet Temperature", true),
        compressor_outlet_temperature_min = new Sensor(this, "compressor_outlet_temperature_min", "Compressor Outlet Temperature (min)", true),
        compressor_outlet_temperature_max = new Sensor(this, "compressor_outlet_temperature_max", "Compressor Outlet Temperature (max)", true),

        air_inlet_temperature = new FilteredSensor(this, "air_inlet_temperature", "Air inlet Temperature", true),
        air_inlet_temperature_min = new Sensor(this, "air_inlet_temperature_min", "Air inlet Temperature (min)", true),
        air_inlet_temperature_max = new Sensor(this, "air_inlet_temperature_max", "Air inlet Temperature (max)", true),

        evaporator1_temperature = new FilteredSensor(this, "evaporator1_temperature", "Evaporator1 Temperature", true),
        evaporator2_temperature = new FilteredSensor(this, "evaporator2_temperature", "Evaporator2 Temperature", true),
        evaporator3_temperature = new FilteredSensor(this, "evaporator3_temperature", "Evaporator3 Temperature", true),

        input_i2 = new BinarySensor(this, "input_i2", "Input I2", true),
        input_i1 = new BinarySensor(this, "input_i1", "Input I1", true),
        heating_active = new BinarySensor(this, "heating_active", "Heating Active", false),

        operation_mode = new SelectEntity(this, "operation_mode", "Operation Mode", false),
    };

    operation_mode->addOption("use input");
    operation_mode->addOption("normal");
    operation_mode->addOption("eager");
}


}  // namespace aquamqtt
