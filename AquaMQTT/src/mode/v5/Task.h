#ifndef AQUAMQTT_TASK_H
#define AQUAMQTT_TASK_H


namespace aquamqtt
{

class Task
{
public:
    Task(const char* name);

    void spawn();
    virtual void setup() = 0;
    virtual void loop() = 0;

protected:
    const char* taskName;

    void log_line(const char* msg) const;

    virtual void periodicUpdate();
private:
    [[noreturn]] static void innerTask(void* pvParameters);

    unsigned long last_statistics_update_timestamp = 0;
};

}  // namespace aquamqtt

#endif  // AQUAMQTT_TASK_H
