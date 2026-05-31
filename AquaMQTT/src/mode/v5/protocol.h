#ifndef AQUAMQTT_V5_PROTOCOL_H
#define AQUAMQTT_V5_PROTOCOL_H

#include "Frame.h"


namespace aquamqtt{

bool check_crc(const uint8_t* frame, const uint8_t len);
uint16_t calculate_crc(const uint8_t* frame, const uint8_t len);

bool process_frame(Frame &frame);
bool process_frame_buffer(message::FrameBufferChannel channel, uint8_t* buffer, uint8_t len, char* err_message, uint8_t err_message_limit);

}
#endif // AQUAMQTT_V5_PROTOCOL_H
