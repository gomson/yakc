//------------------------------------------------------------------------------
//  cpc.cc
//
//  TODO:
//  - improve CRTC emulation so that demos work (implement CRTC closer
//    to the real thing, a cascade of counters, and decode video memory
//    per CRTC cycle, not per line)
//  - subtle differences between different CRTC types
//  - ROM module switching
//  - KC Compact: doesn't have custom gate array
//  - support more emulator file formats
//
//------------------------------------------------------------------------------
#include "cpc.h"
#include "yakc/core/filetypes.h"

namespace YAKC {

//------------------------------------------------------------------------------
void
cpc::init_key_mask(uint8_t ascii, int col, int bit, int shift) {
    YAKC_ASSERT((col >= 0) && (col < 10));
    YAKC_ASSERT((bit >= 0) && (bit < 8));
    YAKC_ASSERT((shift >= 0) && (shift < 2));
    this->key_map[ascii] = key_mask();
    this->key_map[ascii].col[col] = (1<<bit);
    if (shift != 0) {
        this->key_map[ascii].col[2] = (1<<5);
    }
}

//------------------------------------------------------------------------------
void
cpc::init_keymap() {
    // http://cpctech.cpc-live.com/docs/keyboard.html
    clear(this->key_map, sizeof(this->key_map));
    const char* kbd =
        // no shift
        "   ^08641 "
        "  [-97532 "
        "   @oure  "
        "  ]piytwq "
        "   ;lhgs  "
        "   :kjfda "
        "  \\/mnbc  "
        "   ., vxz "

        // shift
        "    _(&$! "
        "  {=)'%#\" "
        "   |OURE  "
        "  }PIYTWQ "
        "   +LHGS  "
        "   *KJFDA "
        "  `?MNBC  "
        "   >< VXZ ";
    YAKC_ASSERT(strlen(kbd) == 160);
    for (int shift = 0; shift < 2; shift++) {
        for (int col = 0; col < 10; col++) {
            for (int bit = 0; bit < 8; bit++) {
                uint8_t ascii = kbd[shift*80 + bit*10 + col];
                this->init_key_mask(ascii, col, bit, shift);
            }
        }
    }

    // special keys
    this->init_key_mask(' ',  5, 7, 0);     // Space
    this->init_key_mask(0x02, 2, 5, 0);     // Shift
    this->init_key_mask(0x08, 1, 0, 0);     // Cursor Left
    this->init_key_mask(0x09, 0, 1, 0);     // Cursor Right
    this->init_key_mask(0x0A, 0, 2, 0);     // Cursor Down
    this->init_key_mask(0x0B, 0, 0, 0);     // Cursor Up
    this->init_key_mask(0x01, 9, 7, 0);     // Delete
    this->init_key_mask(0x0C, 2, 0, 0);     // Clr
    this->init_key_mask(0x0D, 2, 2, 0);     // Return
    this->init_key_mask(0x03, 8, 2, 0);     // Escape

    // joystick (just use some unused upper ascii codes)
    this->init_key_mask(0xF0, 9, 2, 0);     // joystick left
    this->init_key_mask(0xF1, 9, 3, 0);     // joystick right
    this->init_key_mask(0xF2, 9, 0, 0);     // joystick down
    this->init_key_mask(0xF3, 9, 1, 0);     // joystick up
    this->init_key_mask(0xF4, 9, 5, 0);     // joystick fire0
    this->init_key_mask(0xF5, 9, 4, 0);     // joystick fire1
}

//------------------------------------------------------------------------------
void
cpc::init(breadboard* b, rom_images* r, tapedeck* t) {
    YAKC_ASSERT(b && r && t);
    this->board = b;
    this->roms = r;
    this->tape = t;
    this->init_keymap();
}

//------------------------------------------------------------------------------
bool
cpc::check_roms(const rom_images& roms, system model, os_rom os) {
    if (system::cpc464 == model) {
        return roms.has(rom_images::cpc464_os) && roms.has(rom_images::cpc464_basic);
    }
    else if (system::cpc6128 == model) {
        return roms.has(rom_images::cpc6128_os) && roms.has(rom_images::cpc6128_basic);
    }
    else if (system::kccompact == model) {
        return roms.has(rom_images::kcc_os) && roms.has(rom_images::kcc_basic);
    }
    else {
        return false;
    }
}

//------------------------------------------------------------------------------
void
cpc::init_memory_map() {
    auto& cpu = this->board->z80;
    cpu.mem.unmap_all();
    YAKC_ASSERT(check_roms(*this->roms, this->cur_model, os_rom::none));
    this->ga_config = 0x00;     // enable both ROMs
    this->ram_config = 0x00;    // standard RAM bank config (0,1,2,3)
    this->update_memory_mapping();
}

//------------------------------------------------------------------------------
void
cpc::on_context_switched() {
    // FIXME!
}

//------------------------------------------------------------------------------
void
cpc::poweron(system m) {
    YAKC_ASSERT(this->board);
    YAKC_ASSERT(int(system::any_cpc) & int(m));
    YAKC_ASSERT(!this->on);

    this->cur_model = m;
    this->on = true;
    // http://www.cpcwiki.eu/index.php/Format:TAP_tape_image_file_format
    if (m == system::cpc464) {
        this->casread_trap = 0x2836;
        this->casread_ret = 0x2872;
    }
    else {
        this->casread_trap = 0x29A6;
        this->casread_ret = 0x29E2;
    }
    this->ga_config = 0;
    this->ram_config = 0;
    this->scan_kbd_line = 0;
    this->next_key_mask = key_mask();
    this->next_joy_mask = key_mask();
    this->cur_key_mask = key_mask();

    // map memory
    clear(this->board->ram, sizeof(this->board->ram));
    this->init_memory_map();

    // initialize clock to 4 MHz
    this->board->clck.init(4000);

    // initialize support chips
    this->board->i8255.init(0);
    this->video.init(m, this->board);
    this->board->ay8910.init(this->board->clck.base_freq_khz, 1000, SOUND_SAMPLE_RATE);

    // CPU start state
    this->board->z80.init();
    this->board->z80.PC = 0x0000;
}

//------------------------------------------------------------------------------
void
cpc::poweroff() {
    YAKC_ASSERT(this->on);
    this->board->z80.mem.unmap_all();
    this->on = false;
}

//------------------------------------------------------------------------------
void
cpc::reset() {
    this->video.reset();
    this->board->ay8910.reset();
    this->board->i8255.reset();
    this->ga_config = 0;
    this->ram_config = 0;
    this->scan_kbd_line = 0;
    this->next_key_mask = key_mask();
    this->next_joy_mask = key_mask();
    this->cur_key_mask = key_mask();
    this->board->z80.reset();
    this->board->z80.PC = 0x0000;
    this->init_memory_map();
}

//------------------------------------------------------------------------------
uint64_t
cpc::step(uint64_t start_tick, uint64_t end_tick) {
    // step the system for given number of cycles, return actually
    // executed number of cycles
    auto& cpu = this->board->z80;
    auto& dbg = this->board->dbg;
    uint64_t cur_tick = start_tick;
    while (cur_tick < end_tick) {
        // FIXME: the whole "CPU instructions are aligned to 4 cycles" is
        // quite hacky at the moment and doesn't work right!
        uint32_t ticks = cpu.handle_irq(this);
        if (ticks > 0) {
            ticks -= 2;
        }
        ticks += cpu.step(this);
        ticks = (ticks + 3) & ~3;

        this->video.step(this, ticks);
        this->board->ay8910.step(ticks);

        if (dbg.step(cpu.PC, ticks)) {
            return end_tick;
        }

        if (cpu.PC == this->casread_trap) {
            // trapped cassette read function
            this->casread();
        }
        cur_tick += ticks;
    }
    return cur_tick;
}

//------------------------------------------------------------------------------
uint32_t
cpc::step_debug() {
    auto& cpu = this->board->z80;
    auto& dbg = this->board->dbg;
    uint32_t all_ticks = 0;
    uint16_t old_pc;
    do {
        old_pc = cpu.PC;
        uint32_t ticks = cpu.handle_irq(this);
        if (ticks > 0) {
            ticks -= 2;
        }
        ticks += cpu.step(this);
        ticks = (ticks + 3) & ~3;
        this->video.step(this, ticks);
        this->board->ay8910.step(ticks);
        dbg.step(cpu.PC, ticks);
        all_ticks += ticks;
    }
    while ((old_pc == cpu.PC) && !cpu.INV);
    return all_ticks;
}

//------------------------------------------------------------------------------
void
cpc::cpu_out(uint16_t port, uint8_t val) {
    // http://cpcwiki.eu/index.php/Default_I/O_Port_Summary
    if (0 == (port & (1<<15))) {
        // Gate Array or RAM configuration
        if (0 != (port & (1<<14))) {
            // Gate Array
            switch (val & 0xC0) {
                // select pen
                case 0x00:
                    this->video.select_pen(val);
                    break;
                // assign color to selected pen
                case 0x40:
                    this->video.assign_color(val);
                    break;
                // select screen mode, rom config, interrupt ctrl
                case 0x80:
                    //  - bits 0 and 1 select the screen mode
                    //      00: Mode 0 (160x200 @ 16 colors)
                    //      01: Mode 1 (320x200 @ 4 colors)
                    //      02: Mode 2 (640x200 @ 2 colors)
                    //      11: Mode 3 (160x200 @ 2 colors, undocumented)
                    //
                    //  - bit 2: disable/enable lower ROM
                    //  - bit 3: disable/enable upper ROM
                    //
                    //  - bit 4: interrupt generation control
                    //
                    this->ga_config = val;
                    this->video.set_video_mode(val & 3);
                    if (val & (1<<4)) {
                        this->video.interrupt_control(this);
                    }
                    this->update_memory_mapping();
                    break;
            }
        }
        // CPC6128 RAM configuration
        if (((val & 0xC0) == 0xC0) && (system::cpc6128 == this->cur_model)) {
            this->ram_config = val;
            this->update_memory_mapping();
        }
    }
    if (0 == (port & (1<<14))) {
        // CRTC function
        const uint16_t crtc_func = port & 0x0300;
        if (crtc_func == 0x0000) {
            // 0xBCxx: select CRTC register
            this->board->mc6845.select(val);
        }
        else if (crtc_func == 0x0100) {
            // 0xBDxx: write CRTC register
            this->board->mc6845.write(val);
        }
        else {
            //printf("OUT: unknown CRTC function!\n");
        }
        return;
    }
    if (0 == (port & (1<<13))) {
        // FIXME: ROM select
        //printf("OUT ROM Select: %02x\n", val);
    }
    if (0 == (port & (1<<12))) {
        // FIXME: printer port
        //printf("OUT Printer Port: %02x\n", val);
    }
    if (0 == (port & (1<<11))) {
        // 8255 PPI
        this->board->i8255.write(this, ((port & 0x0300)>>8) & 0x03, val);
    }
    if (0 == (port & (1<<10))) {
        // FIXME: peripheral soft reset
        //printf("OUT Peripheral Soft Reset: %02x\n", val);
    }
}

//------------------------------------------------------------------------------
uint8_t
cpc::cpu_in(uint16_t port) {
    if (0 == (port & (1<<14))) {
        // CRTC function
        // FIXME: untested
        const uint16_t crtc_func = port & 0x0300;
        if (crtc_func == 0x0200) {
            // 0xBExx: read status register on type 1 CRTC
            return this->board->mc6845.read_status();
        }
        else if (crtc_func == 0x0300) {
            // 0xBFxx: read from selected CRTC register
            return this->board->mc6845.read();
        }
        else {
            //printf("IN: CRTC unknown function!\n");
            return 0xFF;
        }
    }
    if (0 == (port & (1<<11))) {
        return this->board->i8255.read(this, ((port & 0x0300)>>8) & 0x03);
    }
    if (0 == (port & (1<<10))) {
        // FIXME: Expansion Peripherals
    }
    // fallthrough
    return 0x00;
}

//------------------------------------------------------------------------------
void
cpc::pio_out(int /*pio_id*/, int port_id, uint8_t val) {
    if (i8255::PORT_C == port_id) {
        // PSG function
        const uint8_t func = val & 0xC0;
        switch (func) {
            case 0xC0:
                // select PSG register from PIO Port A
                this->board->ay8910.select(this->board->i8255.output[i8255::PORT_A]);
                break;
            case 0x80:
                // write to selected PSG register
                this->board->ay8910.write(this->board->i8255.output[i8255::PORT_A]);
                break;
        }
        // FIXME: cassette write data
        // bits 0..5: select keyboard matrix line
        this->scan_kbd_line = val & 0x1F;

        // cassette deck motor control
        if (val & (1<<4)) {
            if (!this->tape->is_playing()) {
                this->tape->play();
            }
        }
        else {
            if (this->tape->is_playing()) {
                this->tape->stop();
            }
        }
    }
}

//------------------------------------------------------------------------------
uint8_t
cpc::pio_in(int /*pio_id*/, int port_id) {
    if (i8255::PORT_A == port_id) {
        // catch keyboard data which is normally in PSG PORT A
        if (this->board->ay8910.selected() == ay8910::IO_PORT_A) {
            if (this->scan_kbd_line < 10) {
                return ~(this->cur_key_mask.col[this->scan_kbd_line]);
            }
            else {
                return 0xFF;
            }
        }
        else {
            // read PSG register
            return this->board->ay8910.read();
        }
    }
    else if (i8255::PORT_B == port_id) {
        //  Bit 7: cassette data input
        //  Bit 6: printer port ready (1=not ready, 0=ready)
        //  Bit 5: expansion port /EXP pin
        //  Bit 4: screen refresh rate (1=50Hz, 0=60Hz)
        //  Bit 3..1: distributor id (shown in start screen)
        //      0: Isp
        //      1: Triumph
        //      2: Saisho
        //      3: Solavox
        //      4: Awa
        //      5: Schneider
        //      6: Orion
        //      7: Amstrad
        //  Bit 0: vsync
        //
        uint8_t val = (1<<4) | (7<<1);    // 50Hz refresh rate, Amstrad
        if (this->video.vsync_bit()) {
            val |= (1<<0);
        }
        return val;
    }
    else {
        // shouldn't happen
        return 0xFF;
    }
}

//------------------------------------------------------------------------------
// CPC6128 RAM block indices (see cpu_out())
static int ram_config_table[8][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 2, 7 },
    { 4, 5, 6, 7 },
    { 0, 3, 2, 7 },
    { 0, 4, 2, 3 },
    { 0, 5, 2, 3 },
    { 0, 6, 2, 3 },
    { 0, 7, 2, 3 },
};

void
cpc::update_memory_mapping() {
    // index into RAM config array
    int ram_table_index;
    uint8_t* rom0_ptr,*rom1_ptr;
    if (system::kccompact == this->cur_model) {
        ram_table_index = 0;
        rom0_ptr = this->roms->ptr(rom_images::kcc_os);
        rom1_ptr = this->roms->ptr(rom_images::kcc_basic);
    }
    else if (system::cpc6128 == this->cur_model) {
        ram_table_index = this->ram_config & 0x07;
        rom0_ptr = this->roms->ptr(rom_images::cpc6128_os);
        rom1_ptr = this->roms->ptr(rom_images::cpc6128_basic);
    }
    else {
        ram_table_index = 0;
        rom0_ptr = this->roms->ptr(rom_images::cpc464_os);
        rom1_ptr = this->roms->ptr(rom_images::cpc464_basic);
    }
    auto& cpu = this->board->z80;
    const int i0 = ram_config_table[ram_table_index][0];
    const int i1 = ram_config_table[ram_table_index][1];
    const int i2 = ram_config_table[ram_table_index][2];
    const int i3 = ram_config_table[ram_table_index][3];
    // 0x0000..0x3FFF
    if (this->ga_config & (1<<2)) {
        // read/write from and to RAM bank
        cpu.mem.map(0, 0x0000, 0x4000, this->board->ram[i0], true);
    }
    else {
        // read from ROM, write to RAM
        cpu.mem.map_rw(0, 0x0000, 0x4000, rom0_ptr, this->board->ram[i0]);
    }
    // 0x4000..0x7FFF
    cpu.mem.map(0, 0x4000, 0x4000, this->board->ram[i1], true);
    // 0x8000..0xBFFF
    cpu.mem.map(0, 0x8000, 0x4000, this->board->ram[i2], true);
    // 0xC000..0xFFFF
    if (this->ga_config & (1<<3)) {
        // read/write from and to RAM bank
        cpu.mem.map(0, 0xC000, 0x4000, this->board->ram[i3], true);
    }
    else {
        // read from ROM, write to RAM
        cpu.mem.map_rw(0, 0xC000, 0x4000, rom1_ptr, this->board->ram[i3]);
    }
}

//------------------------------------------------------------------------------
void
cpc::put_input(uint8_t ascii, uint8_t joy0_mask) {
    // ascii=0 means no key pressed, joystick input mutes keyboard input
    this->next_joy_mask = key_mask();
    if (0 == joy0_mask) {
        this->next_key_mask = this->key_map[ascii];
    }
    else {
        this->next_key_mask = key_mask();
        if (joy0_mask & joystick::left) {
            this->next_joy_mask.combine(this->key_map[0xF0]);
        }
        if (joy0_mask & joystick::right) {
            this->next_joy_mask.combine(this->key_map[0xF1]);
        }
        if (joy0_mask & joystick::up) {
            this->next_joy_mask.combine(this->key_map[0xF2]);
        }
        if (joy0_mask & joystick::down) {
            this->next_joy_mask.combine(this->key_map[0xF3]);
        }
        if (joy0_mask & joystick::btn0) {
            this->next_joy_mask.combine(this->key_map[0xF5]);
        }
        if (joy0_mask & joystick::btn1) {
            this->next_joy_mask.combine(this->key_map[0xF4]);
        }
    }
}

//------------------------------------------------------------------------------
void
cpc::irq(bool b) {
    this->board->z80.irq(b);
}

//------------------------------------------------------------------------------
void
cpc::iack() {
    this->video.interrupt_acknowledge();
}

//------------------------------------------------------------------------------
void
cpc::vblank() {
    // fetch next key mask
    this->cur_key_mask = this->next_key_mask;
    this->cur_key_mask.combine(this->next_joy_mask);
}

//------------------------------------------------------------------------------
void
cpc::decode_audio(float* buffer, int num_samples) {
    this->board->ay8910.fill_samples(buffer, num_samples);
}

//------------------------------------------------------------------------------
const void*
cpc::framebuffer(int& out_width, int& out_height) {
    if (this->video.debug_video) {
        out_width = cpc_video::dbg_max_display_width;
        out_height = cpc_video::dbg_max_display_height;
    }
    else {
        out_width = cpc_video::max_display_width;
        out_height = cpc_video::max_display_height;
    }
    return this->video.rgba8_buffer;
}

//------------------------------------------------------------------------------
bool
cpc::load_sna(filesystem* fs, const char* name, filetype type, bool start) {
    auto fp = fs->open(name, filesystem::mode::read);
    if (!fp) {
        return false;
    }
    sna_header hdr;
    bool hdr_valid = false;
    if (fs->read(fp, &hdr, sizeof(hdr)) == sizeof(hdr)) {
        hdr_valid = true;
        const uint16_t dump_size = (hdr.dump_size_h<<8 | hdr.dump_size_l) & 0xFFFF;
        if (dump_size == 64) {
            YAKC_ASSERT(sizeof(this->board->ram) >= 0x10000);
            fs->read(fp, this->board->ram, 0x10000);
        }
        else {
            YAKC_ASSERT(sizeof(this->board->ram) >= 0x20000);
            fs->read(fp, this->board->ram, 0x20000);
        }
    }
    // CPU state
    auto& cpu = this->board->z80;
    cpu.F = hdr.F; cpu.A = hdr.A;
    cpu.C = hdr.C; cpu.B = hdr.B;
    cpu.E = hdr.E; cpu.D = hdr.D;
    cpu.L = hdr.L; cpu.H = hdr.H;
    cpu.R = hdr.R; cpu.I = hdr.I;
    cpu.IFF1 = (hdr.IFF1 & 1) != 0;
    cpu.IFF2 = (hdr.IFF2 & 1) != 0;
    cpu.IX = (hdr.IX_h<<8 | hdr.IX_l) & 0xFFFF;
    cpu.IY = (hdr.IY_h<<8 | hdr.IY_l) & 0xFFFF;
    cpu.SP = (hdr.SP_h<<8 | hdr.SP_l) & 0xFFFF;
    cpu.PC = (hdr.PC_h<<8 | hdr.PC_l) & 0xFFFF;
    cpu.IM = hdr.IM;
    cpu.AF_ = (hdr.A_<<8 | hdr.F_) & 0xFFFF;
    cpu.BC_ = (hdr.B_<<8 | hdr.C_) & 0xFFFF;
    cpu.DE_ = (hdr.D_<<8 | hdr.E_) & 0xFFFF;
    cpu.HL_ = (hdr.H_<<8 | hdr.L_) & 0xFFFF;
    // gate array state
    for (int i = 0; i < 17; i++) {
        this->cpu_out(0x7FFF, i);
        this->cpu_out(0x7FFF, (hdr.pens[i] & 0x1F) | 0x40);
    }
    this->cpu_out(0x7FFF, hdr.selected_pen & 0x1F);
    this->cpu_out(0x7FFF, (hdr.gate_array_config & 0x3F) | 0x80);
    this->cpu_out(0x7FFF, (hdr.ram_config & 0x3F) | 0xC0);
    auto& vdg = this->board->mc6845;
    for (int i = 0; i < 18; i++) {
        vdg.select(i);
        vdg.write(hdr.crtc_regs[i]);
    }
    vdg.select(hdr.crtc_selected);
    // FIXME: rom_config
    auto& ppi = this->board->i8255;
    ppi.output[i8255::PORT_A] = hdr.ppi_a;
    ppi.output[i8255::PORT_B] = hdr.ppi_b;
    ppi.output[i8255::PORT_C] = hdr.ppi_c;
    this->board->ay8910.select(hdr.psg_selected);
    for (int i = 0; i < 16; i++) {
        this->board->ay8910.regs[i] = hdr.psg_regs[i];
    }
    fs->close(fp);
    fs->rm(name);
    if (!hdr_valid) {
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
bool
cpc::quickload(filesystem* fs, const char* name, filetype type, bool start) {
    if (filetype::cpc_sna == type) {
        return this->load_sna(fs, name, type, start);
    }
    else {
        return false;
    }
}

//------------------------------------------------------------------------------
void
cpc::casread() {
    auto& cpu = this->board->z80;
    bool success = false;
    // read the next block
    uint16_t len = 0;
    this->tape->read(&len, sizeof(len));
    uint8_t sync = 0;
    this->tape->read(&sync, sizeof(sync));
    if (sync == cpu.A) {
        success = true;
        this->tape->inc_counter(1);
        for (uint16_t i = 0; i < (len-1); i++) {
            uint8_t val;
            this->tape->read(&val, sizeof(val));
            cpu.mem.w8(cpu.HL++, val);
        }
    }
    cpu.F = success ? 0x45 : 0x00;
    cpu.PC = this->casread_ret;
}

//------------------------------------------------------------------------------
const char*
cpc::system_info() const {
    return "FIXME!";
}

} // namespace YAKC
