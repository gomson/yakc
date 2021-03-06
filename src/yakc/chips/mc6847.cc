//------------------------------------------------------------------------------
//  mc6847.cc
//------------------------------------------------------------------------------
#include "mc6847.h"

namespace YAKC {

// 8 color palette
//
// the MC6847 outputs three color values:
//
//  Y' - six level analog luminance
//  phiA - three level analog (U)
//  phiB - three level analog (V)
//
// see discussion here: http://forums.bannister.org/ubbthreads.php?ubb=showflat&Number=64986
//
// NEW VALUES from here: http://www.stardot.org.uk/forums/viewtopic.php?f=44&t=12503
//
//  green:      19 146  11
//  yellow:    155 150  10
//  blue:        2  22 175
//  red:       155  22   7
//  buff:      141 150 154
//  cyan:       15 143 155
//  magenta:   139  39 155
//  orange:    140  31  11
//
//  color intensities are slightly boosted
//
#define clamp(x) ((x)>255?255:(x))
#define rgba(r,g,b) (0xFF000000 | clamp((r*4)/3) | (clamp((g*4)/3)<<8) | (clamp((b*4)/3)<<16))
static const uint32_t colors[8] = {
    rgba(19, 146, 11),      // green
    rgba(155, 150, 10),     // yellow
    rgba(2, 22, 175),       // blue
    rgba(155, 22, 7),       // red
    rgba(141, 150, 154),    // buff
    rgba(15, 143, 155),     // cyan
    rgba(139, 39, 155),     // cyan
    rgba(140, 31, 11)       // orange
};

static const uint32_t black = 0xFF111111;
static const uint32_t alnum_green = rgba(19, 146, 11);
static const uint32_t alnum_dark_green = 0xFF002400;
static const uint32_t alnum_orange = rgba(140, 31, 11);
static const uint32_t alnum_dark_orange = 0xFF000E22;

// internal character ROM dump from MAME
// (ntsc_square_fontdata8x12 in devices/video/mc6847.cpp)
static const uint8_t fontdata8x12[64 * 12] =
{
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1A, 0x2A, 0x2A, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x1C, 0x12, 0x12, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x38, 0x20, 0x20, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x38, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1E, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x2A, 0x24, 0x1A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3E, 0x10, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2A, 0x24, 0x1A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1C, 0x20, 0x20, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x04, 0x02, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x0C, 0x14, 0x3E, 0x04, 0x04, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
};

//------------------------------------------------------------------------------
void
mc6847::init(read_func reader_func, uint32_t* fb_write_ptr, int cpu_khz) {
    // the 6847 is clocked at 3.58 MHz, the CPU clock is lower, thus
    // need to compute tick counter limit in CPU ticks
    const int vdg_khz = 3580;

    // one scanline is 228 3.58 MHz ticks, increase fixed point precision
    h_limit = (228 * cpu_khz * prec) / vdg_khz;
    h_sync_start = (10 * cpu_khz * prec) / vdg_khz;
    h_sync_end = (26 * cpu_khz * prec) / vdg_khz;
    h_count = 0;
    l_count = 0;
    bits = 0;
    read_addr_func = reader_func;
    rgba8_buffer = fb_write_ptr;
}

//------------------------------------------------------------------------------
void
mc6847::reset() {
    h_count = 0;
    l_count = 0;
    bits = 0;
}

//------------------------------------------------------------------------------
void
mc6847::step() {
    prev_bits = bits;
    h_count += prec;
    if ((h_count >= h_sync_start) && (h_count < h_sync_end)) {
        // HSYNC on
        bits |= HSYNC;
        if (l_count == l_disp_end) {
            // switch FSYNC on
            bits |= FSYNC;
        }
    }
    else {
        // HSYNC off
        bits &= ~HSYNC;
    }
    if (h_count >= h_limit) {
        h_count -= h_limit;
        l_count++;
        if (l_count >= l_limit) {
            // rewind line counter, FSYNC off
            l_count = 0;
            bits &= ~FSYNC;
        }
        if (l_count < l_vblank) {
            // skip
        }
        else if (l_count < l_disp_start) {
            // top border (y=0..243)
            decode_border(l_count - l_vblank);
        }
        else if (l_count < l_disp_end) {
            // visible area (y=0..192)
            decode_line(l_count - l_disp_start);
        }
        else if (l_count < l_btmborder_end) {
            // bottom border (y=0..243)
            decode_border(l_count - l_vblank);
        }
    }
}

//------------------------------------------------------------------------------
uint32_t
mc6847::border_color() {
    // determine the currently active border color
    if (bits & A_G) {
        // a graphics mode, either green or buff depending on CSS bit
        return (bits & CSS) ? colors[4] : colors[0];
    }
    else {
        // alphanumeric or semigraphics mode, always 'black'
        return black;
    }
}

//------------------------------------------------------------------------------
void
mc6847::decode_border(int y) {
    uint32_t* dst = &(this->rgba8_buffer[y * disp_width_with_border]);
    const uint32_t c = this->border_color();
    for (int x = 0; x < disp_width_with_border; x++) {
        *dst++ = c;
    }
}

//------------------------------------------------------------------------------
void
mc6847::decode_line(int y) {
    uint32_t* dst = &(this->rgba8_buffer[(y+l_topborder) * disp_width_with_border]);
    const uint32_t b_color = this->border_color();
    // left border
    for (int i = 0; i < h_border; i++) {
        *dst++ = b_color;
    }

    // visible pixels
    if (bits & A_G) {
        // one of the 8 graphics modes
        uint8_t sub_mode = (bits & (GM2|GM1)) >> 6;
        if (bits & GM0) {
            // one of the 'resolution modes' (1 bit == 1 pixel block)
            // GM2|GM1:
            //      00:    RG1, 128x64, 16 bytes per row
            //      01:    RG2, 128x96, 16 bytes per row
            //      10:    RG3, 128x192, 16 bytes per row
            //      11:    RG6, 256x192, 32 bytes per row
            const int bytes_per_row = (sub_mode < 3) ? 16 : 32;
            const int dots_per_bit = (sub_mode < 3) ? 2 : 1;
            const int row_height = (bits & GM2) ? 1 : ((bits & GM1) ? 2 : 3);
            uint16_t addr = (y / row_height) * bytes_per_row;
            const uint32_t fg_color = (bits & CSS) ? colors[4] : colors[0];
            for (int x = 0; x < bytes_per_row; x++) {
                const uint8_t m = this->read_addr_func(addr++);
                for (int p = 7; p >= 0; p--) {
                    const uint32_t c = ((m >> p) & 1) ? fg_color : black;
                    for (int d = 0; d < dots_per_bit; d++) {
                        *dst++ = c;
                    }
                }
            }
        }
        else {
            // one of the 'color modes' (2 bits per pixel == 4 colors, CSS select
            // lower or upper half of palette)
            // GM2|GM1:
            //      00: CG1, 64x64, 16 bytes per row
            //      01: CG2, 128x64, 32 bytes per row
            //      10: CG3, 128x96, 32 bytes per row
            //      11: CG6, 128x192, 32 bytes per row
            const uint32_t pal_offset = (bits & CSS) ? 4 : 0;
            const int bytes_per_row = (sub_mode == 0) ? 16 : 32;
            const int dots_per_2bit = (sub_mode == 0) ? 4 : 2;
            const int row_height = (bits & GM2) ? ((bits & GM1) ? 1 : 2) : 3;
            uint16_t addr = (y / row_height) * bytes_per_row;
            for (int x = 0; x < bytes_per_row; x++) {
                const uint8_t m = this->read_addr_func(addr++);
                for (int p = 6; p >= 0; p -= 2) {
                    const uint32_t c = colors[((m >> p) & 3) + pal_offset];
                    for (int d = 0; d < dots_per_2bit; d++) {
                        *dst++ = c;
                    }
                }
            }
        }
    }
    else {
        // we're in alphanumeric/semigraphics mode, one cell
        // is 8x12 pixels, bit 6 is connected to A_S+INT_EXT and
        // may select semigraphics mode per-characters, bit
        // 7 is connected to INV and may invert the character
        // (this is ignored in semigraphics mode)

        // the vidmem src address and offset into the font data
        uint16_t addr = (y/12)*32;
        uint8_t m;  // the pixel bitmask
        const int chr_y = y % 12;
        // bit-shifters to extract a 2x2 or 2x3 semigraphics 2-bit stack
        const int shift_2x2 = (1 - (chr_y / 6))*2;
        const int shift_2x3 = (2 - (chr_y / 4))*2;
        const uint32_t alnum_fg = (bits & CSS) ? alnum_orange : alnum_green;
        const uint32_t alnum_bg = (bits & CSS) ? alnum_dark_orange : alnum_dark_green;
        for (int x = 0; x < 32; x++) {
            const uint8_t chr = this->read_addr_func(addr++);
            if (bits & A_S) {
                // semigraphics mode
                uint32_t fg_color;
                if (bits & INT_EXT) {
                    // 2x3 semigraphics, 2 color sets at 4 colors (selected by CSS pin)
                    // |C1|C0|L5|L4|L3|L2|L1|L0|
                    //
                    // +--+--+
                    // |L5|L4|
                    // +--+--+
                    // |L3|L2|
                    // +--+--+
                    // |L1|L0|
                    // +--+--+

                    // extract the 2 horizontal bits from one of the 3 stacks
                    m = (chr>>shift_2x3) & 3;
                    // 2 bits of color, CSS bit selects upper or lower
                    // half of color palette
                    fg_color = colors[((chr>>6)&3) + ((bits&CSS)?4:0)];
                }
                else {
                    // 2x2 semigraphics, 8 colors + black
                    // |xx|C2|C1|C0|L3|L2|L1|L0|
                    //
                    // +--+--+
                    // |L3|L2|
                    // +--+--+
                    // |L1|L0|
                    // +--+--+

                    // extract the 2 horizontal bits from the upper or lower stack
                    m = (chr>>shift_2x2) & 3;
                    // 3 color bits directly point into the color palette
                    fg_color = colors[(chr>>4) & 7];
                }
                // write the horizontal pixel blocks (2 blocks @ 4 pixel each)
                for (int p = 1; p>=0; p--) {
                    uint32_t c = (m & (1<<p)) ? fg_color : black;
                    *dst++=c; *dst++=c; *dst++=c; *dst++=c;
                }
            }
            else {
                // alphanumeric mode
                // FIXME: INT_EXT (switch between internal and
                // external font
                uint8_t m = fontdata8x12[(chr&0x3F)*12 + chr_y];
                if (bits & INV) {
                    m = ~m;
                }
                for (int p = 7; p >= 0; p--) {
                    *dst++ = m & (1<<p) ? alnum_fg : alnum_bg;
                }
            }
        }
    }

    // right border
    for (int i = 0; i < h_border; i++) {
        *dst++ = b_color;
    }
}

} // namespace YAKC
