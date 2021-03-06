#pragma once
//------------------------------------------------------------------------------
/**
    @class YAKC::bbcmicro_video
    @brief BBC Micro video subsystem
*/
#include "yakc/systems/breadboard.h"

namespace YAKC {

class bbcmicro_video {
public:
    /// initialize the object
    void init(breadboard* board);
    /// perform a reset
    void reset();
    /// step the video hardware (at 2 MHz)
    void step();
    /// decode the next 16 pixels into the emulator framebuffer
    void decode_pixels(uint32_t* dst);

    /// write the ULA video control register (0xFE20)
    void write_video_control(uint8_t v);
    /// write the ULA palette register
    void write_palette(uint8_t v);

    breadboard* board = nullptr;
    int tick_period = 2;
    int tick_count = 2;
    uint8_t video_control = 0;
    uint8_t palette = 0;

    static const int display_width = 768;
    static const int display_height = 272;
    static_assert(display_width <= global_max_fb_width, "bbcmicro fb size");
    static_assert(display_height <= global_max_fb_height, "bbcmicro fb size");
    uint32_t* rgba8_buffer = nullptr;
};

} // namespace YAKC
