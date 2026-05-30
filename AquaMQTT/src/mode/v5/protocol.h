#ifndef AQUAMQTT_V5_PROTOCOL_H
#define AQUAMQTT_V5_PROTOCOL_H

#include "Frame.h"


namespace aquamqtt{

bool check_crc(const uint8_t* frame, const uint8_t len);

void process_frame(const Frame &frame);
bool process_frame_buffer(message::FrameBufferChannel channel, const uint8_t* buffer, uint8_t len, char* err_message, uint8_t err_message_limit);

}
#endif // AQUAMQTT_V5_PROTOCOL_H
