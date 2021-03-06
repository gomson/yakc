//------------------------------------------------------------------------------
//  i8255.cc
//
//  NOTE: only MODE_0 is implemented, no interrupt or handshake handling!
//------------------------------------------------------------------------------
#include "i8255.h"
#include "yakc/core/system_bus.h"

namespace YAKC {

//------------------------------------------------------------------------------
void
i8255::init(int id_) {
    this->id = id_;
    this->reset();
}

//------------------------------------------------------------------------------
void
i8255::reset() {
    // set all ports to input
    this->set_mode(nullptr, 0x9B);
}

//------------------------------------------------------------------------------
void
i8255::output_port_c(system_bus* bus) const {
    if (bus) {
        uint8_t mask = 0;
        uint8_t val = 0;
        if (this->port_c_upper_mode() == MODE_OUTPUT) {
            mask |= 0xF0;
        }
        else {
            val |= 0xF0;   // if in input mode, return all bits set
        }
        if (this->port_c_lower_mode() == MODE_OUTPUT) {
            mask |= 0x0F;
        }
        else {
            val |= 0x0F;
        }
        val |= this->output[PORT_C] & mask;
        bus->pio_out(this->id, PORT_C, val);
    }
}

//------------------------------------------------------------------------------
void
i8255::set_mode(system_bus* bus, uint8_t val) {
    this->control = val;
    for (int i = 0; i < num_ports; i++) {
        this->output[i] = 0;
        //this->input[i] = 0;
    }
    if (bus) {
        if (this->port_a_mode() == MODE_OUTPUT) {
            bus->pio_out(this->id, PORT_A, this->output[PORT_A]);
        }
        else {
            bus->pio_out(this->id, PORT_A, 0xFF);
        }
        if (this->port_b_mode() == MODE_OUTPUT) {
            bus->pio_out(this->id, PORT_B, this->output[PORT_B]);
        }
        else {
            bus->pio_out(this->id, PORT_B, 0xFF);
        }
        this->output_port_c(bus);
    }
}

//------------------------------------------------------------------------------
void
i8255::write(system_bus* bus, int addr, uint8_t val) {
    switch (addr & 3) {
        case PORT_A:
            if (this->port_a_mode() == MODE_OUTPUT) {
                this->output[PORT_A] = val;
                if (bus) {
                    bus->pio_out(this->id, PORT_A, val);
                }
            }
            break;
        case PORT_B:
            if (this->port_b_mode() == MODE_OUTPUT) {
                this->output[PORT_B] = val;
                if (bus) {
                    bus->pio_out(this->id, PORT_B, val);
                }
            }
            break;
        case PORT_C:
            this->output[PORT_C] = val;
            this->output_port_c(bus);
            break;
        case CONTROL:
            if (val & (1<<7)) {
                this->set_mode(bus, val);
            }
            else {
                // set/clear single bit in port C
                const uint8_t mask = 1<<((val>>1)&7);
                if (val & (1<<0)) {
                    this->output[PORT_C] |= mask;
                }
                else {
                    this->output[PORT_C] &= ~mask;
                }
                // FIXME: interrupts, buffer full flags
                this->output_port_c(bus);
            }
            break;
    }
}

//------------------------------------------------------------------------------
uint8_t
i8255::input_port_c(system_bus* bus) {
    uint8_t mask = 0;
    uint8_t val = 0;
    if (this->port_c_upper_mode() == MODE_OUTPUT) {
        val |= this->output[PORT_C] & 0xF0;     // read data from output latch
    }
    else {
        mask |= 0xF0;       // read data from port
    }
    if (this->port_c_lower_mode() == MODE_OUTPUT) {
        val |= this->output[PORT_C] & 0x0F;
    }
    else {
        mask |= 0x0F;
    }
    if (0 != mask) {
        if (bus) {
            val |= bus->pio_in(this->id, PORT_C) & mask;
        }
        else {
            val |= 0xFF & mask;
        }
    }
    return val;
}

//------------------------------------------------------------------------------
uint8_t
i8255::read(system_bus* bus, int addr) {
    uint8_t val = 0xFF;
    switch (addr & 3) {
        case PORT_A:
            if (this->port_a_mode() == MODE_OUTPUT) {
                val = this->output[PORT_A];    // read data from output latch
            }
            else {
                val = bus ? bus->pio_in(this->id, PORT_A) : 0xFF;
            }
            this->last_read[PORT_A] = val;
            break;

        case PORT_B:
            if (this->port_b_mode() == MODE_OUTPUT) {
                val = this->output[PORT_B];
            }
            else {
                val = bus ? bus->pio_in(this->id, PORT_B) : 0xFF;
            }
            this->last_read[PORT_B] = val;
            break;

        case PORT_C:
            val = this->input_port_c(bus);
            this->last_read[PORT_C] = val;
            break;

        case CONTROL:
            val = this->control;
            break;
    }
    return val;
}

} // namespace YAKC
