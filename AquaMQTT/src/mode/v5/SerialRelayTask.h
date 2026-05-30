#ifndef AQUAMQTT_SERIAL_RELAY_TASK_H
#define AQUAMQTT_SERIAL_RELAY_TASK_H

#include <Arduino.h>

#include <vector>
#include <functional>

#include "message/MessageConstants.h"

#include "mode/v5/Task.h"


#define ERROR_MESSAGE_SIZE 200

namespace aquamqtt
{

typedef std::function<bool (const message::FrameBufferChannel, const uint8_t*, const uint8_t, char*, uint8_t)> callback_function_type;

/**
 * Generic relay task that forwards messages between HMI and Main Controller on\
 * the serial interface (MITM).
 *
 * With the passthrough jumper removed, this task:
 *   1. Receives frames from HMI (Serial1) → forwards to Main (Serial2)
 *   2. Receives frames from Main (Serial2) → forwards to HMI (Serial1)
 *   3. Parses all traffic for state extraction (same as listener)
 *
 * Both serial lines are half-duplex 8N1.
 * Frame boundaries are detected by inter-character silence (>2ms).
 */
class SerialRelayTask : public Task
{
public:
    SerialRelayTask(int baud_rate, int max_frame_size);
    virtual ~SerialRelayTask();

    virtual void setup() override;
    virtual void loop() override;

    void addListener(callback_function_type fn) { callback_functions.push_back(fn); };

    static SerialRelayTask& getInstance();

protected:
    virtual void periodicUpdate() override;

private:
    // Process a complete frame received from one side and forward to the other
    void processAndForward(uint8_t* buffer, uint8_t length, HardwareSerial& destination, bool fromHmi);

    int baudRate;
    int maxFrameSize;
    std::vector<callback_function_type> callback_functions;

    // HMI side (Serial1) frame assembly
    uint8_t*      mHmiFrameBuffer;
    uint8_t       mHmiFrameLength;
    unsigned long mHmiLastByteTime;
    bool          mHmiFrameInProgress;

    // Main controller side (Serial2) frame assembly
    uint8_t*      mMainFrameBuffer;
    uint8_t       mMainFrameLength;
    unsigned long mMainLastByteTime;
    bool          mMainFrameInProgress;

    // Statistics
    uint32_t mFramesReceived;
    uint32_t mCrcErrors;
    uint32_t mFramesRelayed;
    uint32_t mWritesInjected;
    uint32_t mHmiFramesIn;
    uint32_t mMainFramesIn;
    uint32_t mHmiBytesIn;
    uint32_t mMainBytesIn;
    uint32_t mLastStatsUpdate;
    uint32_t mEchoBytes;
    uint32_t mTxBytesWritten;

    // Periodic read injection
    unsigned long mLastPeriodicRead;
    uint8_t       mPeriodicReadIndex;

    // Last forwarded frame for debugging
    uint8_t  mLastFwdFrame[32];
    uint8_t  mLastFwdFrameLen;

    char err_message[ERROR_MESSAGE_SIZE];

public:
    uint32_t getFramesReceived() const { return mFramesReceived; }
    uint32_t getCrcErrors() const { return mCrcErrors; }
    uint32_t getFramesRelayed() const { return mFramesRelayed; }
    uint32_t getHmiFramesIn() const { return mHmiFramesIn; }
    uint32_t getMainFramesIn() const { return mMainFramesIn; }
    uint32_t getHmiBytesIn() const { return mHmiBytesIn; }
    uint32_t getMainBytesIn() const { return mMainBytesIn; }
    uint32_t getEchoBytes() const { return mEchoBytes; }
    uint32_t getTxBytesWritten() const { return mTxBytesWritten; }
    uint32_t getWritesInjected() const { return mWritesInjected; }
    const uint8_t* getLastFwdFrame() const { return mLastFwdFrame; }
    uint8_t getLastFwdFrameLen() const { return mLastFwdFrameLen; }
};

}  // namespace aquamqtt

#endif  // AQUAMQTT_SERIAL_RELAY_TASK_H
