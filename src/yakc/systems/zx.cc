//------------------------------------------------------------------------------
//  zx.cc
//
//  TODO
//  - wait states when CPU accesses 'contended memory' and IO ports
//  - reads from port 0xFF must return 'current VRAM byte':
//      - reset a T-state counter at start of each PAL-line
//      - on CPU instruction, increment PAL-line T-state counter
//      - use this counter to find the VRAM byte
//  - add the system info text
//
//------------------------------------------------------------------------------
#include "zx.h"
#include "yakc/core/filetypes.h"

namespace YAKC {

uint32_t zx::palette[8] = {
    0xFF000000,     // black
    0xFFFF0000,     // blue
    0xFF0000FF,     // red
    0xFFFF00FF,     // magenta
    0xFF00FF00,     // green
    0xFFFFFF00,     // cyan
    0xFF00FFFF,     // yellow
    0xFFFFFFFF,     // white
};

//------------------------------------------------------------------------------
void
zx::init(breadboard* b, rom_images* r) {
    YAKC_ASSERT(b && r);
    this->board = b;
    this->roms = r;
    this->rgba8_buffer = this->board->rgba8_buffer;
    // setup key translation table
    this->init_keymap();
}

//------------------------------------------------------------------------------
bool
zx::check_roms(const rom_images& roms, system model, os_rom os) {
    if (system::zxspectrum48k == model) {
        return roms.has(rom_images::zx48k);
    }
    else if (system::zxspectrum128k == model) {
        return roms.has(rom_images::zx128k_0) && roms.has(rom_images::zx128k_1);
    }
    return false;
}

//------------------------------------------------------------------------------
static uint64_t
key_bit(int column, int line) {
    // returns keyboard matrix bit mask with single bit set at column and line
    YAKC_ASSERT((column >= 0) && (column < 8));
    YAKC_ASSERT((line >= 0) && (line < 5));
    return (uint64_t(1)<<line)<<(column*5);
}

//------------------------------------------------------------------------------
void
zx::init_key_mask(ubyte ascii, int column, int line, int shift) {
    // initialize an entry in the keyboard mapping table which
    // maps ASCII codes to keyboard matrix bit mask
    YAKC_ASSERT((column >= 0) && (column < 8));
    YAKC_ASSERT((line >= 0) && (line < 5));
    YAKC_ASSERT((shift >= 0) && (shift < 3));
    uint64_t mask = key_bit(column, line);

    // FIXME: hmm, may need simultanous caps+sym shift later
    if (1 == shift) {
        // caps-shift
        mask |= key_bit(0, 0);
    }
    else if (2 == shift) {
        // sym-shift
        mask |= key_bit(7, 1);
    }
    this->key_map[ascii] = mask;
}

//------------------------------------------------------------------------------
void
zx::init_keymap() {
    // initialize the keyboard matrix lookup table, maps
    // ASCII codes to keyboard matrix state
    clear(this->key_map, sizeof(this->key_map));
    const char* kbd =
        // no shift
        " zxcv"         // A8       shift,z,x,c,v
        "asdfg"         // A9       a,s,d,f,g
        "qwert"         // A10      q,w,e,r,t
        "12345"         // A11      1,2,3,4,5
        "09876"         // A12      0,9,8,7,6
        "poiuy"         // A13      p,o,i,u,y
        " lkjh"         // A14      enter,l,k,j,h
        "  mnb"         // A15      space,symshift,m,n,b

        // shift
        " ZXCV"         // A8
        "ASDFG"         // A9
        "QWERT"         // A10
        "     "         // A11
        "     "         // A12
        "POIUY"         // A13
        " LKJH"         // A14
        "  MNB"         // A15

        // symshift
        " : ?/"         // A8
        "     "         // A9
        "   <>"         // A10
        "!@#$%"         // A11
        "_)('&"         // A12
        "\";   "        // A13
        " =+-^"         // A14
        "  .,*";        // A15

    for (int shift = 0; shift < 3; shift++) {
        for (int column = 0; column < 8; column++) {
            for (int line = 0; line < 5; line++) {
                const ubyte c = kbd[shift*40 + column*5 + line];
                if (c != 0x20) {
                    this->init_key_mask(c, column, line, shift);
                }
            }
        }
    }

    // special keys
    this->init_key_mask(' ', 7, 0, 0);  // Space
    this->init_key_mask(0x0F, 7, 1, 0); // SymShift
    this->init_key_mask(0x08, 3, 4, 1); // Cursor Left (Shift+5)
    this->init_key_mask(0x0A, 4, 4, 1); // Cursor Down (Shift+6)
    this->init_key_mask(0x0B, 4, 3, 1); // Cursor Up (Shift+7)
    this->init_key_mask(0x09, 4, 2, 1); // Cursor Right (Shift+8)
    this->init_key_mask(0x07, 3, 0, 1); // Edit (Shift+1)
    this->init_key_mask(0x0C, 4, 0, 1); // Delete (Shift+0)
    this->init_key_mask(0x0D, 6, 0, 0); // Enter
}

//------------------------------------------------------------------------------
void
zx::init_memory_map() {
    z80& cpu = this->board->z80;
    cpu.mem.unmap_all();
    if (system::zxspectrum48k == this->cur_model) {
        // 48k RAM between 0x4000 and 0xFFFF
        cpu.mem.map(0, 0x4000, 0x4000, this->board->ram[0], true);
        cpu.mem.map(0, 0x8000, 0x4000, this->board->ram[1], true);
        cpu.mem.map(0, 0xC000, 0x4000, this->board->ram[2], true);

        // 16k ROM between 0x0000 and 0x3FFF
        YAKC_ASSERT(this->roms->has(rom_images::zx48k) && (this->roms->size(rom_images::zx48k) == 0x4000));
        cpu.mem.map(0, 0x0000, 0x4000, this->roms->ptr(rom_images::zx48k), false);
    }
    else {
        // Spectrum 128k initial memory mapping
        cpu.mem.map(0, 0x4000, 0x4000, this->board->ram[5], true);
        cpu.mem.map(0, 0x8000, 0x4000, this->board->ram[2], true);
        cpu.mem.map(0, 0xC000, 0x4000, this->board->ram[0], true);
        YAKC_ASSERT(this->roms->has(rom_images::zx128k_0) && (this->roms->size(rom_images::zx128k_0) == 0x4000));
        YAKC_ASSERT(this->roms->has(rom_images::zx128k_1) && (this->roms->size(rom_images::zx128k_1) == 0x4000));
        cpu.mem.map(0, 0x0000, 0x4000, this->roms->ptr(rom_images::zx128k_0), false);
    }
}

//------------------------------------------------------------------------------
void
zx::on_context_switched() {
    // FIXME!
}

//------------------------------------------------------------------------------
void
zx::poweron(system m) {
    YAKC_ASSERT(this->board);
    YAKC_ASSERT(int(system::any_zx) & int(m));
    YAKC_ASSERT(!this->on);

    this->cur_model = m;
    this->border_color = 0xFF000000;
    this->last_fe_out = 0;
    this->scanline_counter = 0;
    this->blink_counter = 0;
    this->memory_paging_disabled = false;
    this->joy_mask = 0;
    this->next_kbd_mask = 0;
    this->cur_kbd_mask = 0;
    if (system::zxspectrum48k == this->cur_model) {
        this->display_ram_bank = 0;
    }
    else {
        this->display_ram_bank = 5;
    }
    this->on = true;

    // map memory
    clear(this->board->ram, sizeof(this->board->ram));
    this->init_memory_map();

    // initialize the system clock and PAL-line timer
    int cpu_khz;
    if (system::zxspectrum48k == m) {
        // Spectrum48K is exactly 3.5 MHz
        cpu_khz = 3500;
        this->board->clck.init(cpu_khz);
        this->board->clck.config_timer_cycles(0, 224);
    }
    else {
        // 128K is slightly faster
        cpu_khz = 3547;
        this->board->clck.init(cpu_khz);
        this->board->clck.config_timer_cycles(0, 228);
    }

    // initialize sound generator
    this->board->beeper.init(cpu_khz, SOUND_SAMPLE_RATE);
    if (system::zxspectrum128k == this->cur_model) {
        this->board->ay8910.init(cpu_khz, cpu_khz/2, SOUND_SAMPLE_RATE);
    }

    // cpu start state
    this->board->z80.init();
    this->board->z80.PC = 0x0000;
}

//------------------------------------------------------------------------------
void
zx::poweroff() {
    YAKC_ASSERT(this->on);
    this->board->z80.mem.unmap_all();
    this->on = false;
}

//------------------------------------------------------------------------------
void
zx::reset() {
    this->board->beeper.reset();
    if (system::zxspectrum128k == this->cur_model) {
        this->board->ay8910.reset();
    }
    this->memory_paging_disabled = false;
    this->joy_mask = 0;
    this->next_kbd_mask = 0;
    this->cur_kbd_mask = 0;
    this->last_fe_out = 0;
    this->scanline_counter = 0;
    this->blink_counter = 0;
    this->display_ram_bank = 5;
    this->board->z80.reset();
    this->board->z80.PC = 0x0000;
    this->init_memory_map();
}

//------------------------------------------------------------------------------
uint64_t
zx::step(uint64_t start_tick, uint64_t end_tick) {
    auto& cpu = this->board->z80;
    auto& dbg = this->board->dbg;
    uint64_t cur_tick = start_tick;
    if (system::zxspectrum48k == this->cur_model) {
        while (cur_tick < end_tick) {
            uint32_t ticks = cpu.step(this);
            ticks += cpu.handle_irq(this);
            this->board->clck.step(this, ticks);
            this->board->beeper.step(ticks);
            if (dbg.step(cpu.PC, ticks)) {
                return end_tick;
            }
            cur_tick += ticks;
        }
    }
    else {
        while (cur_tick < end_tick) {
            uint32_t ticks = cpu.step(this);
            ticks += cpu.handle_irq(this);
            this->board->clck.step(this, ticks);
            this->board->beeper.step(ticks);
            this->board->ay8910.step(ticks);
            if (dbg.step(cpu.PC, ticks)) {
                return end_tick;
            }
            cur_tick += ticks;
        }
    }
    return cur_tick;
}

//------------------------------------------------------------------------------
uint32_t
zx::step_debug() {
    auto& cpu = this->board->z80;
    auto& dbg = this->board->dbg;
    uint64_t all_ticks = 0;
    uint16_t old_pc;
    do {
        old_pc = cpu.PC;
        uint32_t ticks = cpu.step(this);
        ticks += cpu.handle_irq(this);
        this->board->clck.step(this, ticks);
        this->board->beeper.step(ticks);
        if (system::zxspectrum128k == this->cur_model) {
            this->board->ay8910.step(ticks);
        }
        dbg.step(cpu.PC, ticks);
        all_ticks += ticks;
    }
    while ((old_pc == cpu.PC) && !cpu.INV);    
    return uint32_t(all_ticks);
}

//------------------------------------------------------------------------------
void
zx::cpu_out(uword port, ubyte val) {
    // handle Z80 OUT instruction
    if ((port & 1) == 0) {
        // "every even IO port addresses the ULA but to avoid
        // problems with other I/O devices, only FE should be used"
        this->border_color = palette[val & 7] & 0xFFD7D7D7;
        // FIXME:
        //      bit 3: MIC output (CAS SAVE, 0=On, 1=Off)
        //      bit 4: Beep output (ULA sound, 0=Off, 1=On)
        this->last_fe_out = val;
        this->board->beeper.write(0 != (val & (1<<4)));
        return;
    }

    // Spectrum128K specific out ports
    if (system::zxspectrum128k == this->cur_model) {
        // control memory bank switching on 128K
        // http://8bit.yarek.pl/computer/zx.128/
        if (0x7FFD == port) {
            if (!this->memory_paging_disabled) {
                // bit 3 defines the video scanout memory bank (5 or 7)
                this->display_ram_bank = (val & (1<<3)) ? 7 : 5;

                auto& mem = this->board->z80.mem;

                // only last memory bank is mappable
                mem.map(0, 0xC000, 0x4000, this->board->ram[val & 0x7], true);

                // ROM0 or ROM1
                if (val & (1<<4)) {
                    // bit 4 set: ROM1
                    mem.map(0, 0x0000, 0x4000, this->roms->ptr(rom_images::zx128k_1), false);
                }
                else {
                    // bit 4 clear: ROM0
                    mem.map(0, 0x0000, 0x4000, this->roms->ptr(rom_images::zx128k_0), false);
                }
            }
            if (val & (1<<5)) {
                // bit 5 prevents further changes to memory pages
                // until computer is reset, this is used when switching
                // to the 48k ROM
                this->memory_paging_disabled = true;
            }
            return;
        }
        else if (0xFFFD == port) {
            this->board->ay8910.select(val);
        }
        else if (0xBFFD == port) {
            this->board->ay8910.write(val);
        }
    }

}

//------------------------------------------------------------------------------
void
zx::put_input(ubyte ascii, ubyte joy_mask) {
    // register a new key press with the emulator,
    // ascii=0 means no key pressed
    this->next_kbd_mask = this->key_map[ascii];
    this->joy_mask = joy_mask;
}

//------------------------------------------------------------------------------
static ubyte
extract_kbd_line_bits(uint64_t mask, int column) {
    // extract keyboard matrix line bits by column
    return (ubyte) ((mask>>(column*5)) & 0x1F);
}

//------------------------------------------------------------------------------
ubyte
zx::cpu_in(uword port) {
    // handle Z80 IN instruction
    //
    // FIXME: reading from port xxFF should return 'current VRAM data'

    // keyboard
    if ((port & 0xFF) == 0xFE) {
        ubyte val = 0;
        // MIC/EAR flags -> bit 6
        if (this->last_fe_out & (1<<3|1<<4)) {
            val |= (1<<6);
        }

        // keyboard matrix bits
        ubyte kbd_bits = 0;
        for (int i = 0; i < 8; i++) {
            if ((port & (0x0100 << i)) == 0) {
                kbd_bits |= extract_kbd_line_bits(this->cur_kbd_mask, i);
            }
        }
        val |= (~kbd_bits) & 0x1F;
        return val;
    }
    else if ((port & 0xFF) == 0x1F) {
        // Kempston Joystick
        ubyte val = 0;
        if (this->joy_mask & joystick::left) {
            val |= 1<<1;
        }
        if (this->joy_mask & joystick::right) {
            val |= 1<<0;
        }
        if (this->joy_mask & joystick::up) {
            val |= 1<<3;
        }
        if (this->joy_mask & joystick::down) {
            val |= 1<<2;
        }
        if (this->joy_mask & joystick::btn0) {
            val |= 1<<4;
        }
        return val;
    }
    else if (port == 0xFFFD) {
        return this->board->ay8910.read();
    }
    return 0xFF;
}

//------------------------------------------------------------------------------
void
zx::irq(bool b) {
    // forward interrupt request to CPU
    this->board->z80.irq(b);
}

//------------------------------------------------------------------------------
void
zx::timer(int timer_id) {
    // clock timer callback
    if (0 == timer_id) {
        // timer 0: new PAL line
        this->scanline();
    }
}

//------------------------------------------------------------------------------
void
zx::scanline() {
    // this is called by the timer callback for every PAL line, controlling
    // the vidmem decoding and vblank interrupt
    //
    // detailed information about frame timings is here:
    //  for 48K:    http://rk.nvg.ntnu.no/sinclair/faq/tech_48.html#48K
    //  for 128K:   http://rk.nvg.ntnu.no/sinclair/faq/tech_128.html
    //
    // one PAL line takes 224 T-states on 48K, and 228 T-states on 128K
    // one PAL frame is 312 lines on 48K, and 311 lines on 128K
    //
    const uint32_t frame_scanlines = this->cur_model == system::zxspectrum128k ? 311 : 312;
    const uint32_t top_border = this->cur_model == system::zxspectrum128k ? 63 : 64;

    // decode the next videomem line into the emulator framebuffer,
    // the border area of a real Spectrum is bigger than the emulator
    // (the emu has 32 pixels border on each side, the hardware has:
    //
    //  63 or 64 lines top border
    //  56 border lines bottom border
    //  48 pixels on each side horizontal border
    //
    const uint32_t top_decode_line = top_border - 32;
    const uint32_t btm_decode_line = top_border + 192 + 32;
    if ((this->scanline_counter >= top_decode_line) && (this->scanline_counter < btm_decode_line)) {
        this->decode_video_line(this->scanline_counter - top_decode_line);
    }

    if (this->scanline_counter++ >= frame_scanlines) {
        // start new frame, fetch next key, request vblank interrupt
        this->cur_kbd_mask = this->next_kbd_mask;
        this->scanline_counter = 0;
        this->blink_counter++;
        this->irq(true);
    }
}

//------------------------------------------------------------------------------
const char*
zx::system_info() const {
    return
        "FIXME!\n\n"
        "ZX ROM images acknowledgement:\n\n"
        "Amstrad have kindly given their permission for the redistribution of their copyrighted material but retain that copyright.\n\n"
        "See: http://www.worldofspectrum.org/permits/amstrad-roms.txt";
}

//------------------------------------------------------------------------------
void
zx::decode_video_line(uint16_t y) {
    YAKC_ASSERT(y < display_height);

    // decode a single line from the ZX framebuffer into the
    // emulator's framebuffer
    //
    // this is called from the scanline() callback so that video decoding
    // is synchronized with the CPU and vblank interrupt,
    // y is ranging from 0 to 256 (32 pixels vertical border on each side)
    //
    // the blink flag flips every 16 frames
    //
    uint32_t* dst = &(this->rgba8_buffer[y*display_width]);
    const uint8_t* vidmem_bank = this->board->ram[this->display_ram_bank];
    const bool blink = 0 != (this->blink_counter & 0x10);
    uint32_t fg, bg;
    if ((y < 32) || (y >= 224)) {
        // upper/lower border
        for (int x = 0; x < display_width; x++) {
            *dst++ = this->border_color;
        }
    }
    else {
        // compute video memory Y offset (inside 256x192 area)
        // this is how the 16-bit video memory address is computed
        // from X and Y coordinates:
        //
        // | 0| 1| 0|Y7|Y6|Y2|Y1|Y0|Y5|Y4|Y3|X4|X3|X2|X1|X0|
        uint16_t yy = y-32;
        uint16_t y_offset = ((yy & 0xC0)<<5) | ((yy & 0x07)<<8) | ((yy & 0x38)<<2);

        // left border
        for (int x = 0; x < (4*8); x++) {
            *dst++ = this->border_color;
        }

        // valid 256x192 vidmem area
        for (uint16_t x = 0; x < 32; x++) {
            uint16_t pix_offset = y_offset | x;
            uint16_t clr_offset = 0x1800 + (((yy & ~0x7)<<2) | x);

            // pixel mask and color attribute bytes
            uint8_t pix = vidmem_bank[pix_offset];
            uint8_t clr = vidmem_bank[clr_offset];

            // foreground and background color
            if ((clr & (1<<7)) && blink) {
                fg = palette[(clr>>3) & 7];
                bg = palette[clr & 7];
            }
            else {
                fg = palette[clr & 7];
                bg = palette[(clr>>3) & 7];
            }
            if (0 == (clr & (1<<6))) {
                // standard brightness
                fg &= 0xFFD7D7D7;
                bg &= 0xFFD7D7D7;
            }
            for (int px = 7; px >=0; px--) {
                *dst++ = pix & (1<<px) ? fg : bg;
            }
        }

        // right border
        for (int x = 0; x < (4*8); x++) {
            *dst++ = this->border_color;
        }
    }
}

//------------------------------------------------------------------------------
void
zx::decode_audio(float* buffer, int num_samples) {
    this->board->beeper.fill_samples(buffer, num_samples);
    // if 128k, mix-in AY-8910 data
    if (system::zxspectrum128k == this->cur_model) {
        this->board->ay8910.fill_samples(buffer, num_samples, true);
    }
}

//------------------------------------------------------------------------------
const void*
zx::framebuffer(int& out_width, int& out_height) {
    out_width = display_width;
    out_height = display_height;
    return this->rgba8_buffer;
}

//------------------------------------------------------------------------------
bool
zx::quickload(filesystem* fs, const char* name, filetype type, bool start) {
    auto fp = fs->open(name, filesystem::mode::read);
    if (!fp) {
        return false;
    }
    zxz80_header hdr = { };
    zxz80ext_header ext_hdr = { };
    bool hdr_valid = false;
    bool ext_hdr_valid = false;
    if (filetype::zx_z80 == type) {
        if (fs->read(fp, &hdr, sizeof(hdr)) != sizeof(hdr)) {
            goto error;
        }
        hdr_valid = true;
        uint16_t pc = (hdr.PC_h<<8 | hdr.PC_l) & 0xFFFF;
        const bool is_version1 = 0 != pc;
        if (!is_version1) {
            // read length of ext_hdr
            if (fs->read(fp, &ext_hdr, 2) != 2) {
                goto error;
            }
            // read remaining ext-hdr bytes
            int ext_hdr_len = (ext_hdr.len_h<<8)|ext_hdr.len_l;
            YAKC_ASSERT(ext_hdr_len <= int(sizeof(ext_hdr)));
            if (fs->read(fp, &(ext_hdr.PC_l), ext_hdr_len) != ext_hdr_len) {
                goto error;
            }
            ext_hdr_valid = true;
            if (ext_hdr.hw_mode < 3) {
                if (this->cur_model != system::zxspectrum48k) {
                    goto error;
                }
            }
            else {
                if (this->cur_model != system::zxspectrum128k) {
                    goto error;
                }
            }
        }
        else {
            if (this->cur_model != system::zxspectrum48k) {
                goto error;
            }
        }
        const bool v1_compr = 0 != (hdr.flags0 & (1<<5));

        while (!fs->eof(fp)) {
            int page_index = 0;
            int src_len = 0, dst_len = 0;
            if (is_version1) {
                src_len = fs->size(fp) - sizeof(zxz80_header);
                dst_len = 48 * 1024;
            }
            else {
                zxz80page_header phdr;
                if (fs->read(fp, &phdr, sizeof(phdr)) == sizeof(phdr)) {
                    src_len = (phdr.len_h<<8 | phdr.len_l) & 0xFFFF;
                    dst_len = 0x4000;
                    page_index = phdr.page_nr - 3;
                    if ((system::zxspectrum48k == this->cur_model) && (page_index == 5)) {
                        page_index = 0;
                    }
                    if ((page_index < 0) || (page_index > 7)) {
                        page_index = -1;
                    }
                }
                else {
                    goto error;
                }
            }
            uint8_t* dst_ptr;
            if (-1 == page_index) {
                dst_ptr = this->board->junk;
            }
            else {
                dst_ptr = this->board->ram[page_index];
            }
            const uint8_t* dst_end_ptr = dst_ptr + dst_len;
            if (0xFFFF == src_len) {
                // FIXME: uncompressed not supported yet
                goto error;
            }
            else {
                // compressed
                int src_pos = 0;
                bool v1_done = false;
                uint8_t val[4];
                while ((src_pos < src_len) && !v1_done) {
                    val[0] = fs->peek_u8(fp, src_pos);
                    val[1] = fs->peek_u8(fp, src_pos+1);
                    val[2] = fs->peek_u8(fp, src_pos+2);
                    val[3] = fs->peek_u8(fp, src_pos+3);
                    // check for version 1 end marker
                    if (v1_compr && (0==val[0]) && (0xED==val[1]) && (0xED==val[2]) && (0==val[3])) {
                        v1_done = true;
                        src_pos += 4;
                    }
                    else if (0xED == val[0]) {
                        if (0xED == val[1]) {
                            uint8_t count = val[2];
                            YAKC_ASSERT(0 != count);
                            uint8_t data = val[3];
                            src_pos += 4;
                            for (int i = 0; i < count; i++) {
                                YAKC_ASSERT(dst_ptr < dst_end_ptr);
                                *dst_ptr++ = data;
                            }
                        }
                        else {
                            // single ED
                            YAKC_ASSERT(dst_ptr < dst_end_ptr);
                            *dst_ptr++ = val[0];
                            src_pos++;
                        }
                    }
                    else {
                        // any value
                        YAKC_ASSERT(dst_ptr < dst_end_ptr);
                        *dst_ptr++ = val[0];
                        src_pos++;
                    }
                }
                YAKC_ASSERT(dst_ptr == dst_end_ptr);
                YAKC_ASSERT(src_pos == src_len);
            }
            if (0xFFFF == src_len) {
                fs->skip(fp, 0x4000);
            }
            else {
                fs->skip(fp, src_len);
            }
        }
    }
    fs->close(fp);
    fs->rm(name);

    // start loaded image
    if (start && hdr_valid) {
        z80& cpu = this->board->z80;
        cpu.A = hdr.A; cpu.F = hdr.F;
        cpu.B = hdr.B; cpu.C = hdr.C;
        cpu.D = hdr.D; cpu.E = hdr.E;
        cpu.H = hdr.H; cpu.L = hdr.L;
        cpu.IXH = hdr.IX_h; cpu.IXL = hdr.IX_l;
        cpu.IYH = hdr.IY_h; cpu.IYL = hdr.IY_l;
        cpu.AF_ = (hdr.A_<<8 | hdr.F_) & 0xFFFF;
        cpu.BC_ = (hdr.B_<<8 | hdr.C_) & 0xFFFF;
        cpu.DE_ = (hdr.D_<<8 | hdr.E_) & 0xFFFF;
        cpu.HL_ = (hdr.H_<<8 | hdr.L_) & 0xFFFF;
        cpu.SP = (hdr.SP_h<<8 | hdr.SP_l) & 0xFFFF;
        cpu.I = hdr.I;
        cpu.R = (hdr.R & 0x7F) | ((hdr.flags0 & 1)<<7);
        cpu.IFF2 = hdr.IFF2 != 0;
        cpu.int_enable = hdr.EI != 0;
        if (hdr.flags1 != 0xFF) {
            cpu.IM = (hdr.flags1 & 3);
        }
        else {
            cpu.IM = 1;
        }
        if (ext_hdr_valid) {
            cpu.PC = (ext_hdr.PC_h<<8 | ext_hdr.PC_l) & 0xFFFF;
            for (int i = 0; i < 16; i++) {
                this->board->ay8910.regs[i] = ext_hdr.audio[i];
            }
            cpu.out(this, 0xFFFD, ext_hdr.out_fffd);
            cpu.out(this, 0x7FFD, ext_hdr.out_7ffd);
        }
        else {
            cpu.PC = (hdr.PC_h<<8 | hdr.PC_l) & 0xFFFF;
        }
        this->border_color = zx::palette[(hdr.flags0>>1) & 7] & 0xFFD7D7D7;
    }
    return true;

error:
    fs->close(fp);
    fs->rm(name);
    return false;
}

} // namespace YAKC
