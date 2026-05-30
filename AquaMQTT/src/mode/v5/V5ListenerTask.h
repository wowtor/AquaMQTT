#ifndef AQUAMQTT_V5_LISTENER_TASK_H
#define AQUAMQTT_V5_LISTENER_TASK_H

#include <queue>

#include "Frame.h"
#include "Task.h"
#include "constants.h"

namespace aquamqtt
{

class FrameBuffer final
{
private:
    FrameChannel channel;
    uint64_t n_frames_received = 0;
    uint64_t n_err_checksum = 0;
    uint64_t n_err_frame_too_long = 0;
    uint64_t n_err_buffer_full = 0;
    uint64_t n_err_no_magic = 0;

    uint8_t buffer[MAX_FRAME_SIZE];
    uint8_t buffer_size = 0;

    uint8_t dropped[MAX_FRAME_SIZE];
    uint8_t dropped_size = 0;

    bool checkCRC();
    Frame *handleFrame();

public:
    FrameBuffer(FrameChannel channel);
    ~FrameBuffer() = default;

    inline uint64_t countFramesReceived() const { return n_frames_received; };
    inline uint64_t countChecksumErrors() const { return n_err_checksum; };
    inline uint64_t countLongFrameErrors() const { return n_err_frame_too_long; };
    inline uint64_t countBufferFullErrors() const { return n_err_buffer_full; };
    inline uint64_t countNoMagicErrors() const { return n_err_no_magic; };

    void pushByte(uint8_t val);
};


class V5ListenerTask final : public Task
{
private:
    HardwareSerial *_port;
    uint8_t _gpio_rx;
    uint8_t _gpio_tx;
    uint8_t _gpio_enable_tx;

    FrameBuffer buffer;

    std::queue<Frame>  write_queue;
    SemaphoreHandle_t  write_queue_mutex;

    unsigned long        last_statistics_update_timestamp;

    V5ListenerTask(HardwareSerial *port, const uint8_t gpio_rx, const uint8_t gpio_tx, const uint8_t gpio_enable_tx);
    ~V5ListenerTask() = default;

    void writeQueuedMessages();

public:
    static V5ListenerTask& getInstance();

    V5ListenerTask(V5ListenerTask const&) = delete;
    void operator=(V5ListenerTask const&) = delete;

    void queueSendFrame(const Frame &message);
    void sendByte(const uint8_t value);

    void setup() override;
    void loop() override;

protected:
    virtual void periodicUpdate() override;
};
}  // namespace aquamqtt

#endif  // AQUAMQTT_V5_LISTENER_TASK_H
