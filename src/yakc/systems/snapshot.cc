//------------------------------------------------------------------------------
//  snapshot.cc
//------------------------------------------------------------------------------
#include "snapshot.h"

namespace YAKC {

//------------------------------------------------------------------------------
bool
snapshot::is_snapshot(const state_t& state) {
    return (state.magic == 'YAKC') && (state.version == 1);
}

//------------------------------------------------------------------------------
void
snapshot::take_snapshot(const yakc& emu, state_t& state) {
    memset(&state, 0, sizeof(state));
    state.magic = 'YAKC';
    state.version = 1;
    write_emu_state(emu, state);
    write_clock_state(emu, state);
    write_cpu_state(emu, state);
    write_ctc_state(emu, state);
    write_pio_state(emu, state);
    write_kc_state(emu, state);
    write_z1013_state(emu, state);
    write_z9001_state(emu, state);
    write_memory_state(emu, state);
}

//------------------------------------------------------------------------------
void
snapshot::apply_snapshot(const state_t& state, yakc& emu) {
    YAKC_ASSERT(is_snapshot(state));
    apply_emu_state(state, emu);
    apply_clock_state(state, emu);
    apply_cpu_state(state, emu);
    apply_ctc_state(state, emu);
    apply_pio_state(state, emu);
    apply_kc_state(state, emu);
    apply_z1013_state(state, emu);
    apply_z9001_state(state, emu);
    apply_memory_state(state, emu);
    emu.on_context_switched();
}

//------------------------------------------------------------------------------
void
snapshot::write_emu_state(const yakc& emu, state_t& state) {
    state.emu.model = (uword)emu.model;
    state.emu.os = (uword)emu.os;
}

//------------------------------------------------------------------------------
void
snapshot::apply_emu_state(const state_t& state, yakc& emu) {
    emu.model = (system) state.emu.model;
    emu.os = (os_rom) state.emu.os;
}

//------------------------------------------------------------------------------
void
snapshot::write_clock_state(const yakc& emu, state_t& state) {
    const clock& clk = emu.board.clck;
    state.clock.base_freq_khz = clk.base_freq_khz;
    for (int i = 0; i < 4; i++) {
        state.clock.timers[i].period = clk.timers[i].period;
        state.clock.timers[i].value  = clk.timers[i].value;
    }
}

//------------------------------------------------------------------------------
void
snapshot::apply_clock_state(const state_t& state, yakc& emu) {
    clock& clk = emu.board.clck;
    clk.base_freq_khz = state.clock.base_freq_khz;
    for (int i = 0; i < 4; i++) {
        clk.timers[i].period = state.clock.timers[i].period;
        clk.timers[i].value  = state.clock.timers[i].value;
    }
}

//------------------------------------------------------------------------------
void
snapshot::write_kc_state(const yakc& emu, state_t& state) {
    const kc85& kc = emu.kc85;
    state.kc.on = kc.on;
    state.kc.model = (uword) kc.cur_model;
    state.kc.caos  = (ubyte) kc.cur_caos;
    state.kc.io84 = kc.io84;
    state.kc.io86 = kc.io86;
    state.kc.pio_a = kc.pio_a;
    state.kc.pio_b = kc.pio_b;
    state.kc.cur_scanline = kc.video.cur_scanline;
    state.kc.irm_control = kc.video.irm_control;
    state.kc.pio_blink_flag = kc.video.pio_blink_flag;
    state.kc.ctc_blink_flag = kc.video.ctc_blink_flag;
    for (int c = 0; c < 2; c++) {
        state.kc.chn[c].ctc_mode = kc.audio.channels[c].ctc_mode;
        state.kc.chn[c].ctc_constant = kc.audio.channels[c].ctc_constant;
    }
    for (int s = 0; s < 2; s++) {
        auto& dst = state.kc.slots[s];
        const auto& src = kc.exp.slots[s];
        dst.slot_addr = src.slot_addr;
        dst.module_type = src.mod.type;
        dst.control_byte = src.control_byte;
    }
}

//------------------------------------------------------------------------------
void
snapshot::apply_kc_state(const state_t& state, yakc& emu) {
    kc85& kc = emu.kc85;
    kc.on = 0 != state.kc.on;
    kc.cur_model = (system) state.kc.model;
    kc.cur_caos  = (os_rom) state.kc.caos;
    kc.io84      = state.kc.io84;
    kc.io86      = state.kc.io86;
    kc.pio_a     = state.kc.pio_a;
    kc.pio_b     = state.kc.pio_b;
    kc.video.model = (system) state.kc.model;
    kc.video.cur_scanline = state.kc.cur_scanline;
    kc.video.irm_control = state.kc.irm_control;
    kc.video.pio_blink_flag = 0 != state.kc.pio_blink_flag;
    kc.video.ctc_blink_flag = 0 != state.kc.ctc_blink_flag;
    kc.audio.reset();
    for (int c = 0; c < 2; c++) {
        kc.audio.channels[c].ctc_mode = state.kc.chn[c].ctc_mode;
        kc.audio.channels[c].ctc_constant = state.kc.chn[c].ctc_constant;
    }
    for (int s = 0; s < 2; s++) {
        const auto& slot = state.kc.slots[s];
        if (kc.exp.slot_occupied(slot.slot_addr)) {
            kc.exp.remove_module(slot.slot_addr, kc.board->z80.mem);
        }
        kc.exp.insert_module(slot.slot_addr, (kc85_exp::module_type)slot.module_type);
        kc.exp.update_control_byte(slot.slot_addr, slot.control_byte);
    }
}

//------------------------------------------------------------------------------
void
snapshot::write_z1013_state(const yakc& emu, state_t& state) {
    state.z1013.on = emu.z1013.on;
    state.z1013.model = (uword) emu.z1013.cur_model;
    state.z1013.os = (ubyte) emu.z1013.cur_os;
    state.z1013.kbd_column_nr_requested = emu.z1013.kbd_column_nr_requested;
    state.z1013.kbd_8x8_requested = emu.z1013.kbd_8x8_requested;
    state.z1013.next_kbd_column_bits = emu.z1013.next_kbd_column_bits;
    state.z1013.kbd_column_bits = emu.z1013.kbd_column_bits;
}

//------------------------------------------------------------------------------
void
snapshot::apply_z1013_state(const state_t& state, yakc& emu) {
    emu.z1013.on = 0 != state.z1013.on;
    emu.z1013.cur_model = (system) state.z1013.model;
    emu.z1013.cur_os = (os_rom) state.z1013.os;
    emu.z1013.kbd_column_nr_requested = state.z1013.kbd_column_nr_requested;
    emu.z1013.kbd_8x8_requested = 0 != state.z1013.kbd_8x8_requested;
    emu.z1013.next_kbd_column_bits = state.z1013.next_kbd_column_bits;
    emu.z1013.kbd_column_bits = emu.z1013.kbd_column_bits;
}

//------------------------------------------------------------------------------
void
snapshot::write_z9001_state(const yakc& emu, state_t& state) {
    state.z9001.on = 0 != emu.z9001.on;
    state.z9001.model = (uword) emu.z9001.cur_model;
    state.z9001.os = (ubyte) emu.z9001.cur_os;
    state.z9001.ctc0_mode = emu.z9001.ctc0_mode;
    state.z9001.kbd_column_mask = emu.z9001.kbd_column_mask;
    state.z9001.kbd_line_mask = emu.z9001.kbd_line_mask;
    state.z9001.blink_flipflop = emu.z9001.blink_flipflop;
    state.z9001.brd_color = emu.z9001.brd_color;
    state.z9001.key_mask = emu.z9001.key_mask;
    state.z9001.blink_counter = emu.z9001.blink_counter;
    state.z9001.ctc0_constant = emu.z9001.ctc0_constant;
}

//------------------------------------------------------------------------------
void
snapshot::apply_z9001_state(const state_t& state, yakc& emu) {
    emu.z9001.on = 0 != state.z9001.on;
    emu.z9001.cur_model = (system) state.z9001.model;
    emu.z9001.cur_os = (os_rom) state.z9001.os;
    emu.z9001.ctc0_mode = state.z9001.ctc0_mode;
    emu.z9001.kbd_column_mask = state.z9001.kbd_column_mask;
    emu.z9001.kbd_line_mask = state.z9001.kbd_line_mask;
    emu.z9001.blink_flipflop = 0 != state.z9001.blink_flipflop;
    emu.z9001.brd_color = state.z9001.brd_color;
    emu.z9001.key_mask = state.z9001.key_mask;
    emu.z9001.blink_counter = state.z9001.blink_counter;
    emu.z9001.ctc0_constant = state.z9001.ctc0_constant;
}

//------------------------------------------------------------------------------
void
snapshot::write_cpu_state(const yakc& emu, state_t& state) {
    const z80& cpu = emu.board.z80;
    state.cpu.AF  = cpu.AF;
    state.cpu.BC  = cpu.BC;
    state.cpu.DE  = cpu.DE;
    state.cpu.HL  = cpu.HL;
    state.cpu.WZ  = cpu.WZ;
    state.cpu.AF_ = cpu.AF_;
    state.cpu.BC_ = cpu.BC_;
    state.cpu.DE_ = cpu.DE_;
    state.cpu.HL_ = cpu.HL_;
    state.cpu.WZ_ = cpu.WZ_;
    state.cpu.IX  = cpu.IX;
    state.cpu.IY  = cpu.IY;
    state.cpu.SP  = cpu.SP;
    state.cpu.PC  = cpu.PC;
    state.cpu.I   = cpu.I;
    state.cpu.R   = cpu.R;
    state.cpu.IM  = cpu.IM;
    state.cpu.HALT = cpu.HALT;
    state.cpu.IFF1 = cpu.IFF1;
    state.cpu.IFF2 = cpu.IFF2;
    state.cpu.INV  = cpu.INV;
    state.cpu.int_active = cpu.int_active;
    state.cpu.int_enable = cpu.int_enable;
}

//------------------------------------------------------------------------------
void
snapshot::apply_cpu_state(const state_t& state, yakc& emu) {
    z80& cpu = emu.board.z80;
    cpu.AF = state.cpu.AF;
    cpu.BC = state.cpu.BC;
    cpu.DE = state.cpu.DE;
    cpu.HL = state.cpu.HL;
    cpu.WZ = state.cpu.WZ;
    cpu.AF_ = state.cpu.AF_;
    cpu.BC_ = state.cpu.BC_;
    cpu.DE_ = state.cpu.DE_;
    cpu.HL_ = state.cpu.HL_;
    cpu.WZ_ = state.cpu.WZ_;
    cpu.IX  = state.cpu.IX;
    cpu.IY  = state.cpu.IY;
    cpu.SP  = state.cpu.SP;
    cpu.PC  = state.cpu.PC;
    cpu.I   = state.cpu.I;
    cpu.R   = state.cpu.R;
    cpu.IM  = state.cpu.IM;
    cpu.HALT = 0 != state.cpu.HALT;
    cpu.IFF1 = 0 != state.cpu.IFF1;
    cpu.IFF2 = 0 != state.cpu.IFF2;
    cpu.INV  = 0 != state.cpu.INV;
    cpu.int_active = 0 != state.cpu.int_active;
    cpu.int_enable = 0 != state.cpu.int_enable;
}

//------------------------------------------------------------------------------
void
snapshot::write_intctrl_state(const z80int& src, state_t::intctrl_t& dst) {
    dst.enabled = src.int_enabled;
    dst.requested = src.int_requested;
    dst.request_data = src.int_request_data;
    dst.pending = src.int_pending;
}

//------------------------------------------------------------------------------
void
snapshot::apply_intctrl_state(const state_t::intctrl_t& src, z80int& dst) {
    dst.int_enabled = 0 != src.enabled;
    dst.int_requested = 0 != src.requested;
    dst.int_request_data = src.request_data;
    dst.int_pending = 0 != src.pending;
}

//------------------------------------------------------------------------------
void
snapshot::write_ctc_state(const yakc& emu, state_t& state) {
    for (int c = 0; c < 4; c++) {
        auto& dst = state.ctc.chn[c];
        const auto& src = emu.board.z80ctc.channels[c];
        dst.down_counter = src.down_counter;
        dst.mode = src.mode;
        dst.constant = src.constant;
        dst.waiting_for_trigger = src.waiting_for_trigger;
        dst.interrupt_vector = src.interrupt_vector;
        write_intctrl_state(src.int_ctrl, dst.intctrl);
    }
}

//------------------------------------------------------------------------------
void
snapshot::apply_ctc_state(const state_t& state, yakc& emu) {
    for (int c = 0; c < 4; c++) {
        auto& dst = emu.board.z80ctc.channels[c];
        const auto& src = state.ctc.chn[c];
        dst.down_counter = src.down_counter;
        dst.mode = src.mode;
        dst.constant = src.constant;
        dst.waiting_for_trigger = 0 != src.waiting_for_trigger;
        dst.interrupt_vector = src.interrupt_vector;
        apply_intctrl_state(src.intctrl, dst.int_ctrl);
    }
}

//------------------------------------------------------------------------------
void
snapshot::write_pio_state(const yakc& emu, state_t& state) {
    const z80pio& pio1 = emu.board.z80pio;
    for (int i = 0; i < 2; i++) {
        state.pio1.port[i] = pio1.port[i];
    }
    write_intctrl_state(pio1.int_ctrl, state.pio1.intctrl);
    const z80pio& pio2 = emu.board.z80pio2;
    for (int i = 0; i < 2; i++) {
        state.pio2.port[i] = pio2.port[i];
    }
    write_intctrl_state(pio2.int_ctrl, state.pio2.intctrl);
}

//------------------------------------------------------------------------------
void
snapshot::apply_pio_state(const state_t& state, yakc& emu) {
    z80pio& pio1 = emu.board.z80pio;
    for (int i = 0; i < 2; i++) {
        pio1.port[i] = state.pio1.port[i];
    }
    apply_intctrl_state(state.pio1.intctrl, pio1.int_ctrl);
    z80pio& pio2 = emu.board.z80pio2;
    for (int i = 0; i < 2; i++) {
        pio2.port[i] = state.pio2.port[i];
    }
    apply_intctrl_state(state.pio2.intctrl, pio2.int_ctrl);
}

//------------------------------------------------------------------------------
void
snapshot::write_memory_state(const yakc& emu, state_t& state) {
    static_assert(sizeof(emu.board.ram) == sizeof(state.ram), "Breadboard RAM size mismatch");
    memcpy(state.ram, emu.board.ram, sizeof(emu.board.ram));
    if (emu.is_system(system::any_kc85)) {
        // copy content of KC85 RAM modules
        const kc85& kc = emu.kc85;
        const auto& slot08 = kc.exp.slot_by_addr(0x08);
        if (slot08.mod.mem_ptr && slot08.mod.mem_owned) {
            memcpy(state.ram8, slot08.mod.mem_ptr, slot08.mod.mem_size);
        }
        const auto& slot0C = kc.exp.slot_by_addr(0x0C);
        if (slot0C.mod.mem_ptr && slot0C.mod.mem_owned) {
            memcpy(state.ramC, slot0C.mod.mem_ptr, slot0C.mod.mem_size);
        }
    }
}

//------------------------------------------------------------------------------
void
snapshot::apply_memory_state(const state_t& state, yakc& emu) {
    static_assert(sizeof(emu.board.ram) == sizeof(state.ram), "Breadboard RAM size mismatch");
    memcpy(emu.board.ram, state.ram, sizeof(emu.board.ram));
    if (emu.is_system(system::any_kc85)) {
        // copy content of KC85 RAM modules
        kc85& kc = emu.kc85;
        const auto& slot08 = kc.exp.slot_by_addr(0x08);
        if (slot08.mod.mem_ptr && slot08.mod.mem_owned) {
            memcpy(slot08.mod.mem_ptr, state.ram8, slot08.mod.mem_size);
        }
        const auto& slot0C = kc.exp.slot_by_addr(0x0C);
        if (slot0C.mod.mem_ptr && slot0C.mod.mem_owned) {
            memcpy(slot0C.mod.mem_ptr, state.ramC, slot0C.mod.mem_size);
        }
    }
}

} // namespace YAKC
