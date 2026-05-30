#include "V5ListenerTask.h"

#include <esp_task_wdt.h>

#include "config/Configuration.h"
#include "util.h"
#include "mqtttask.h"
#include "protocol.h"


#define WRITE_QUEUE_SIZE 8

namespace aquamqtt
{

/*** NON-MEMBER FUNCTIONS ***/

void frameReceived(const Frame &frame, bool dropped) {
    /*
    LOG.print("[framebuffer] processing message: header=");
    LOG.print(message.getHeaderValue());
    LOG.print("; payload_size=");
    LOG.print(message.payload_size());
    LOG.println();
    */

    // send frame to MQTT for debugging
    #ifdef MQTT_PUBLISH_FRAMES
    if (dropped) {
        MqttTaskV5::getInstance().queueDroppedBytes(frame);
    } else {
        MqttTaskV5::getInstance().queueFrame(frame);
        process_frame(frame);
    }
    #endif
}

/*** FRAMEBUFFER PUBLIC ***/

FrameBuffer::FrameBuffer(FrameChannel _channel)
    : channel(_channel)
{
}

void FrameBuffer::pushByte(const uint8_t val)
{
    // drop old bytes if the buffer is full
    if (buffer_size >= MAX_FRAME_SIZE) {
        frameReceived(Frame(channel, buffer, buffer_size), true);
        n_err_buffer_full++;
        buffer_size = 0;
    }

    // append the new element to the buffer
    buffer[buffer_size++] = val;

    // see if the buffer contains a full frame
    Frame* message = handleFrame();
    if (message) {
        frameReceived(*message, false);
        delete message;
    }
}

/*** FRAMEBUFFER PRIVATE ***/

Frame * FrameBuffer::handleFrame()
{
    if (dropped_size > MAX_FRAME_SIZE - 2) {
        frameReceived(Frame(channel, dropped, dropped_size), true);
        dropped_size = 0;
    }

    // first byte must be 0x01
    if (buffer_size == 1 && buffer[0] != 0x01) {
        dropped[dropped_size++] = buffer[0];
        //LOG.print("dropping first byte: ");
        //LOG.println(buffer[0]);
        n_err_no_magic++;
        buffer_size = 0;
        return nullptr;
    }

    // second byte must be 0x64 or 0x65
    if (buffer_size == 2 && buffer[1] != 0x64 && buffer[1] != 0x65) {
        dropped[dropped_size++] = buffer[0];
        dropped[dropped_size++] = buffer[1];
        //LOG.print("dropping first 2 bytes: ");
        //LOG.print(buffer[0]);
        //LOG.print("-");
        //LOG.println(buffer[1]);
        n_err_no_magic++;
        buffer_size = 0;
        return nullptr;
    }

    if (dropped_size > 0) {
        frameReceived(Frame(channel, dropped, dropped_size), true);
        dropped_size = 0;
    }

    if (buffer_size < HEADER_LENGTH + 2) {
        return nullptr;
    }

    // check if we have a message without a payload
    if (buffer_size == HEADER_LENGTH + 2) {
        if (check_crc(buffer, buffer_size)) {
            // CRC match -> we have a message without a payload
            n_frames_received++;
            Frame* msg = new Frame(channel, buffer, buffer_size);
            buffer_size = 0;
            return msg;
        }
    }

    int payloadLength = buffer[HEADER_LENGTH];
    int messageLength = HEADER_LENGTH + 1 + payloadLength;

    if (messageLength + 2 > MAX_FRAME_SIZE) {
        LOG.println("[framebuffer] message has invalid length");
        n_err_frame_too_long++;
        frameReceived(Frame(channel, buffer, buffer_size), true);
        buffer_size = 0;
        return nullptr;
    }

    // wait until buffer holds a complete frame
    if (buffer_size < messageLength + 2) {
        return nullptr;
    }

    if (!check_crc(buffer, buffer_size)) {
        LOG.println("[framebuffer] invalid CRC");
        n_err_checksum++;
        frameReceived(Frame(channel, buffer, buffer_size), true);
        buffer_size = 0;
        return nullptr;
    }

    // completed and valid frame
    n_frames_received++;
    Frame* msg = new Frame(channel, buffer, buffer_size);
    buffer_size = 0;
    return msg;
}


/*** SERIALTASK PUBLIC ***/

V5ListenerTask& V5ListenerTask::getInstance() {
    static V5ListenerTask instance(&Serial2, config::GPIO_MAIN_RX, 0, 0);
    return instance;
}

void V5ListenerTask::queueSendFrame(const Frame &frame) {
    if (xSemaphoreTake(write_queue_mutex, portMAX_DELAY)) {
        if (write_queue.size() >= WRITE_QUEUE_SIZE) {
            write_queue.pop();
        }
        write_queue.push(frame);

        xSemaphoreGive(write_queue_mutex);
    }
}

void V5ListenerTask::sendByte(const uint8_t value) {
    digitalWrite(_gpio_enable_tx, HIGH);
    _port->write(value);
    _port->flush();
    digitalWrite(_gpio_enable_tx, LOW);
}

void V5ListenerTask::setup()
{
    Task::setup();

    log_line(taskName);
    _port->begin(38400, SERIAL_8N1, _gpio_rx, _gpio_tx);
    if (_gpio_enable_tx) {
        pinMode(_gpio_enable_tx, OUTPUT);
    }
}

void V5ListenerTask::loop()
{
    Task::loop();

    while (_port->available())
    {
        uint8_t value = _port->read();
        buffer.pushByte(value);
    }

    writeQueuedMessages();
}

/*** SERIALTASK PROTECTED ***/

void V5ListenerTask::periodicUpdate()
{
    Task::periodicUpdate();

    LOG.print("[");
    LOG.print(taskName);
    LOG.print("] messages received: ");
    LOG.print(buffer.countFramesReceived());
    LOG.print("; checksums errors: ");
    LOG.print(buffer.countChecksumErrors());
    LOG.print("; long messages: ");
    LOG.print(buffer.countLongFrameErrors());
    LOG.print("; buffer full: ");
    LOG.print(buffer.countBufferFullErrors());
    LOG.print("; no magic: ");
    LOG.print(buffer.countNoMagicErrors());
    LOG.println();

    LOG.print("[serial] stack size: ");
    LOG.print(uxTaskGetStackHighWaterMark(nullptr));
    LOG.print("; minimum ever heep size: ");
    LOG.println(xPortGetMinimumEverFreeHeapSize());
}

/*** SERIALTASK PRIVATE ***/

V5ListenerTask::V5ListenerTask(HardwareSerial *port, const uint8_t gpio_rx, const uint8_t gpio_tx, const uint8_t gpio_enable_tx)
    : Task("listener")
    , _port((HardwareSerial*)port)
    , _gpio_rx(gpio_rx)
    , _gpio_tx(gpio_tx)
    , _gpio_enable_tx(gpio_enable_tx)
    , buffer(FrameChannel::CH_LISTENER)
    , last_statistics_update_timestamp(0)
{
    write_queue_mutex = xSemaphoreCreateMutex();
}

void V5ListenerTask::writeQueuedMessages() {
    if (!_gpio_enable_tx) {
        return;
    }

    if (true) {
        return;
    }

    if (xSemaphoreTake(write_queue_mutex, portMAX_DELAY)) {
        while (!write_queue.empty()) {
            Frame &frame = write_queue.front();
            LOG.print("forwarding message: enable_tx=");
            LOG.print(_gpio_enable_tx);
            LOG.print("; tx=");
            LOG.print(_gpio_tx);
            LOG.print("; ");
            LOG.println(frame.getBufferAsString().c_str());

            digitalWrite(_gpio_enable_tx, HIGH);
            _port->write(frame.get_buffer(), frame.get_buffer_size());
            _port->flush();
            digitalWrite(_gpio_enable_tx, LOW);
            
            write_queue.pop();
        }

        xSemaphoreGive(write_queue_mutex);
    }
}

}  // namespace aquamqtt
