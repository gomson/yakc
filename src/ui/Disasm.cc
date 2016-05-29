//------------------------------------------------------------------------------
//  Disasm.cc
//------------------------------------------------------------------------------
#include "Disasm.h"
#include "z80dasm.h"
#include "Core/Memory/Memory.h"

using namespace Oryol;
using namespace z80dasm;

namespace yakc {

//------------------------------------------------------------------------------
Disasm::Disasm() :
context(0) {
    Memory::Clear(this->buffer, sizeof(this->buffer));
}

//------------------------------------------------------------------------------
ubyte
Disasm::fetch(uword base, int offset, void* userdata) {
    Disasm* self = (Disasm*) userdata;
    return self->context->board.cpu.mem.r8(base + offset);
}

//------------------------------------------------------------------------------
uword
Disasm::Disassemble(const emu& emu, uword addr) {
    this->context = &emu;
    int res = z80disasm(fetch, addr, this->buffer, this);
    return (res & 0xFFFF);
}

//------------------------------------------------------------------------------
const char*
Disasm::Result() const {
    return this->buffer;
}

} // namespace yakc
