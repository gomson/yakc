#pragma once
//------------------------------------------------------------------------------
/**
    @class YAKC::system_bus
    @brief chip-interconnect callback interface
*/
#include "yakc/core/core.h"

namespace YAKC {

class system_bus {
public:
    /// called by cycle-stepped CPU per 'subcycle'
    virtual void cpu_tick();

    /// CPU IN callback
    virtual uint8_t cpu_in(uint16_t port);
    /// CPU OUT callback
    virtual void cpu_out(uint16_t port, uint8_t val);

    /// PIO input callback
    virtual uint8_t pio_in(int pio_id, int port_id);
    /// PIO output callback
    virtual void pio_out(int pio_id, int port_id, uint8_t val);
    /// PIO ready callback
    virtual void pio_rdy(int pio_id, int port_id, bool active);

    /// CTC write callback
    virtual void ctc_write(int ctc_id, int chn_id);
    /// CTC ZCTO callback
    virtual void ctc_zcto(int ctc_id, int chn_id);

    /// trigger CPU INT line on/off
    virtual void irq(bool b);
    /// interrupt acknowled CPU
    virtual void iack();
    /// clock timer triggered
    virtual void timer(int timer_id);

    /// optional, called when vblank happens
    virtual void vblank();
};

} // namespace YAKC
