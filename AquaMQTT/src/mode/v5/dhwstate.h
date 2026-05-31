#ifndef AQUAMQTT_DHWSTATE_H
#define AQUAMQTT_DHWSTATE_H

#include <string>
#include <map>
#include <vector>


#define MAX_UNIQUE_ID_SIZE 60
#define MAX_STATE_TOPIC_SIZE (MAX_UNIQUE_ID_SIZE+30)

#define OPERATION_MODE_USE_INPUT 0
#define OPERATION_MODE_NORMAL 1
#define OPERATION_MODE_EAGER 2
#define OPERATION_MODE_OFF 3
#define OPERATION_MODE_BOOST 4

#define OPERATION_MODE_USE_INPUT_STR "use input"
#define OPERATION_MODE_NORMAL_STR "normal"
#define OPERATION_MODE_EAGER_STR "eager"
#define OPERATION_MODE_OFF_STR "off"
#define OPERATION_MODE_BOOST_STR "boost"

namespace aquamqtt
{

class DhwState;

class Entity
{
private:
    DhwState* device;
    const char* entity_id;
    const char* name;
    bool is_diagnostic;

    char unique_id[MAX_UNIQUE_ID_SIZE];
    char state_topic[MAX_STATE_TOPIC_SIZE];

    bool enabled_by_default = true;

    bool _has_value = false;
    std::string _state;

public:
    Entity(DhwState* device, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~Entity() = default;

    virtual const char* getPlatform() const = 0;
    const char* state();
    virtual void set_state(const char* state) = 0;
    void unset_value();

    inline const bool getEnabledByDefault() const { return enabled_by_default; };
    inline void setEnabledByDefault(bool new_value) { enabled_by_default = new_value; };

    inline const DhwState& getDevice() { return *device; };
    inline const char* getUniqueId() const { return unique_id; };
    inline const char* getStateTopic() const { return state_topic; };
    inline virtual const char* getCommandTopic() const { return nullptr; };

    virtual void writeDefinition(std::stringstream& ss);

protected:
    std::map<std::string, std::string> _def;
    std::map<std::string, Entity*> commands;
    void update_state(const char* new_state);
};


class BinarySensor: public Entity
{
private:
    bool value;

public:
    BinarySensor(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~BinarySensor() = default;

    const char* getPlatform() const override { return "binary_sensor"; };

    void set_value(const bool new_value);
    void set_state(const char* state) override;
};


class Switch: public BinarySensor
{
private:
    char command_topic[MAX_STATE_TOPIC_SIZE];

public:
    Switch(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~Switch() = default;

    const char* getPlatform() const override { return "switch"; };

    inline virtual const char* getCommandTopic() const override { return command_topic; };
};


class Sensor: public Entity
{
private:
    float value = 0;
    const char* format = "%f";
public:

    Sensor(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~Sensor() = default;

    const char* getPlatform() const override { return "sensor"; };

    virtual void set_value(const float new_value);
    void set_state(const char* state) override;
    void setFormat(const char* fmt);
};


class SelectEntity: public Entity
{
private:
    int value = -1;
    std::vector<std::string> options;
    char command_topic[MAX_STATE_TOPIC_SIZE];

public:
    SelectEntity(DhwState* device_id, const char* entity_id, const char* name, bool is_diagnostic);
    virtual ~SelectEntity() = default;

    const char* getPlatform() const override { return "select"; };

    int getIndex() const { return value; };
    SelectEntity& addOption(const char* value);

    void set_value(const int new_value);
    void set_state(const char* state) override;
    inline virtual const char* getCommandTopic() const override { return command_topic; };
    void writeDefinition(std::stringstream& s) override;
};


class DhwState final
{
private:
    std::string device_id;
    std::string device_name;

public:
    static DhwState& getInstance();

    DhwState(DhwState const&) = delete;
    void operator=(DhwState const&) = delete;

    inline const char* getDeviceId() const { return device_id.c_str(); };
    inline const char* getDeviceName() const { return device_name.c_str(); };

    Sensor *water_temperature, *water_temperature_min, *water_temperature_max;
    Sensor *compressor_outlet_temperature, *compressor_outlet_temperature_min, *compressor_outlet_temperature_max;
    Sensor *air_inlet_temperature, *air_inlet_temperature_min, *air_inlet_temperature_max;
    Sensor *evaporator1_temperature;
    Sensor *evaporator2_temperature;
    Sensor *evaporator3_temperature;

    BinarySensor* input_i2;
    BinarySensor* input_i1;
    BinarySensor* heating_active;

    SelectEntity* operation_mode;

    std::vector<Entity*> entities;

private:
    DhwState(const char* device_id, const char* device_name);
    ~DhwState() = default;
};

}  // namespace aquamqtt

#endif  // AQUAMQTT_DHWSTATE_H
