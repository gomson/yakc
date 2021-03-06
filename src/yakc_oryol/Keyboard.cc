//------------------------------------------------------------------------------
//  Keyboard.cc
//------------------------------------------------------------------------------
#include "Keyboard.h"
#include <cctype>

namespace YAKC {

using namespace Oryol;

//------------------------------------------------------------------------------
static bool
is_modifier(Key::Code key) {
    switch (key) {
        case Key::LeftShift:
        case Key::RightShift:
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
static uint8_t
translate_special_key(const yakc* emu, Key::Code key, bool shift, bool ctrl) {
    if (emu->is_system(system::any_zx)) {
        switch (key) {
            case Key::Left:         return 0x08;
            case Key::Right:        return 0x09;
            case Key::Down:         return 0x0A;
            case Key::Up:           return 0x0B;
            case Key::Enter:        return 0x0D;
            case Key::BackSpace:    return 0x0C;
            case Key::Escape:       return 0x07;
            case Key::LeftControl:  return 0x0F;    // unused, for SymShift
            default:                return 0;
        }
    }
    else if (emu->is_system(system::acorn_atom)) {
        // http://www.vintagecomputer.net/fjkraan/comp/atom/atap/atap03.html#131
        switch (key) {
            case Key::Left:         return 0x08;
            case Key::Right:        return 0x09;
            case Key::Down:         return 0x0A;
            case Key::Up:           return 0x0B;
            case Key::Enter:        return 0x0D;
            case Key::BackSpace:    return 0x01;
            case Key::Escape:       return ctrl ? 0x1E : 0x1B;  // Home, Esc
            // Atom ctrl+key codes:
            case Key::G:            return ctrl ? 0x07:0;   // Ctrl+G: bleep
            case Key::H:            return ctrl ? 0x08:0;   // Ctrl+H: left
            case Key::I:            return ctrl ? 0x09:0;   // Ctrl+I: right
            case Key::J:            return ctrl ? 0x0A:0;   // Ctrl+J: down
            case Key::K:            return ctrl ? 0x0B:0;   // Ctrl+K: up
            case Key::L:            return ctrl ? 0x0C:0;   // Ctrl+L: formfeed
            case Key::M:            return ctrl ? 0x0D:0;   // Ctrl+M: return
            case Key::N:            return ctrl ? 0x0E:0;   // Ctrl+N: pagemode on
            case Key::O:            return ctrl ? 0x0F:0;   // Ctrl+O: pagemode off
            case Key::U:            return ctrl ? 0x15:0;   // Ctrl+U: end screen
            case Key::X:            return ctrl ? 0x18:0;   // Ctrl+X: cancel
            case Key::LeftBracket:  return ctrl ? 0x1B:0;   // Ctrl+[: escape
            default:                return 0;
        }
    }
    else if (emu->is_system(system::any_cpc)) {
        switch (key) {
            case Key::Left:         return 0x08;
            case Key::Right:        return 0x09;
            case Key::Down:         return 0x0A;
            case Key::Up:           return 0x0B;
            case Key::Enter:        return 0x0D;
            case Key::LeftShift:    return 0x02;
            case Key::BackSpace:    return shift ? 0x0C : 0x01; // 0x0C: clear screen
            case Key::Escape:       return shift ? 0x13 : 0x03; // 0x13: break
            case Key::F1:           return 0xF1;
            case Key::F2:           return 0xF2;
            case Key::F3:           return 0xF3;
            case Key::F4:           return 0xF4;
            case Key::F5:           return 0xF5;
            case Key::F6:           return 0xF6;
            case Key::F7:           return 0xF7;
            case Key::F8:           return 0xF8;
            case Key::F9:           return 0xF9;
            case Key::F10:          return 0xFA;
            case Key::F11:          return 0xFB;
            case Key::F12:          return 0xFC;
            default:                return 0;
        }
    }
    else {
        switch (key) {
            case Key::Left:         return 0x08;
            case Key::Right:        return 0x09;
            case Key::Down:         return 0x0A;
            case Key::Up:           return 0x0B;
            case Key::Enter:        return 0x0D;
            case Key::BackSpace:    return shift ? 0x0C : 0x01; // 0x0C: clear screen
            case Key::Escape:       return shift ? 0x13 : 0x03; // 0x13: break
            case Key::F1:           return 0xF1;
            case Key::F2:           return 0xF2;
            case Key::F3:           return 0xF3;
            case Key::F4:           return 0xF4;
            case Key::F5:           return 0xF5;
            case Key::F6:           return 0xF6;
            case Key::F7:           return 0xF7;
            case Key::F8:           return 0xF8;
            case Key::F9:           return 0xF9;
            case Key::F10:          return 0xFA;
            case Key::F11:          return 0xFB;
            case Key::F12:          return 0xFC;
            default:                return 0;
        }
    }
}

//------------------------------------------------------------------------------
void
Keyboard::Setup(yakc& emu_) {
    o_assert_dbg(!this->emu);
    this->emu = &emu_;
    Input::SubscribeEvents([this](const InputEvent& e) {
        if (!this->hasInputFocus) {
            return;
        }
        if (e.Type == InputEvent::WChar) {
            if ((e.WCharCode >= 32) && (e.WCharCode < 127)) {
                uint8_t ascii = (uint8_t) e.WCharCode;
                // invert case (not on ZX or CPC machines)
                if (!(this->emu->is_system(system::any_zx)||this->emu->is_system(system::any_cpc))) {
                    if (std::islower(ascii)) {
                        ascii = std::toupper(ascii);
                    }
                    else if (std::isupper(ascii)) {
                        ascii = std::tolower(ascii);
                    }
                }
                this->cur_char = ascii;
            }
        }
        else if (e.Type == InputEvent::KeyDown) {
            // keep track of pressed keys
            if (!is_modifier(e.KeyCode) && !this->pressedKeys.Contains(e.KeyCode)) {
                this->pressedKeys.Add(e.KeyCode);
            }
            // translate any non-alphanumeric keys (Enter, Esc, cursor keys etc...)
            bool shift = Input::KeyPressed(Key::LeftShift);
            bool ctrl = Input::KeyPressed(Key::LeftControl);
            uint8_t special_ascii = translate_special_key(this->emu, e.KeyCode, shift, ctrl);
            if (0 != special_ascii) {
                this->cur_char = special_ascii;
            }
            // joystick
            if (e.KeyCode == Key::Left) {
                this->cur_kbd_joy |= joystick::left;
            }
            else if (e.KeyCode == Key::Right) {
                this->cur_kbd_joy |= joystick::right;
            }
            else if (e.KeyCode == Key::Up) {
                this->cur_kbd_joy |= joystick::up;
            }
            else if (e.KeyCode == Key::Down) {
                this->cur_kbd_joy |= joystick::down;
            }
            else if (e.KeyCode == Key::Space) {
                this->cur_kbd_joy |= joystick::btn0;
            }
            else if (e.KeyCode == Key::LeftControl) {
                this->cur_kbd_joy |= joystick::btn1;
            }
        }
        else if (e.Type == InputEvent::KeyUp) {
            // keep track of pressed keys
            if (!is_modifier(e.KeyCode) && this->pressedKeys.Contains(e.KeyCode)) {
                this->pressedKeys.Erase(e.KeyCode);
                if (this->pressedKeys.Empty()) {
                    this->cur_char = 0;
                }
            }
            // keyboard joystick emulation
            if (e.KeyCode == Key::Left) {
                this->cur_kbd_joy &= ~joystick::left;
            }
            else if (e.KeyCode == Key::Right) {
                this->cur_kbd_joy &= ~joystick::right;
            }
            else if (e.KeyCode == Key::Up) {
                this->cur_kbd_joy &= ~joystick::up;
            }
            else if (e.KeyCode == Key::Down) {
                this->cur_kbd_joy &= ~joystick::down;
            }
            else if (e.KeyCode == Key::Space) {
                this->cur_kbd_joy &= ~joystick::btn0;
            }
            else if (e.KeyCode == Key::LeftControl) {
                this->cur_kbd_joy &= ~joystick::btn1;
            }
        }
    });
}

//------------------------------------------------------------------------------
void
Keyboard::Discard() {
    o_assert_dbg(this->emu);
    Input::UnsubscribeEvents(this->callbackId);
}

//------------------------------------------------------------------------------
void
Keyboard::HandleInput() {
    o_assert_dbg(this->emu);
    this->cur_pad_joy = 0;
    if (this->hasInputFocus) {
        // gamepad input (FIXME: add support for 2nd joystick!)
        if (Input::GamepadAttached(0)) {
            if (Input::GamepadButtonPressed(0, GamepadButton::A)) {
                this->cur_pad_joy |= joystick::btn0;
            }
            if (Input::GamepadButtonPressed(0, GamepadButton::B)) {
                this->cur_pad_joy |= joystick::btn1;
            }
            if (Input::GamepadButtonPressed(0, GamepadButton::DPadLeft)) {
                this->cur_pad_joy |= joystick::left;
            }
            if (Input::GamepadButtonPressed(0, GamepadButton::DPadRight)) {
                this->cur_pad_joy |= joystick::right;
            }
            if (Input::GamepadButtonPressed(0, GamepadButton::DPadUp)) {
                this->cur_pad_joy |= joystick::up;
            }
            if (Input::GamepadButtonPressed(0, GamepadButton::DPadDown)) {
                this->cur_pad_joy |= joystick::down;
            }
            const float deadZone = 0.3f;
            float x = Input::GamepadAxisValue(0, GamepadAxis::LeftStickHori) +
                      Input::GamepadAxisValue(0, GamepadAxis::RightStickHori);
            float y = Input::GamepadAxisValue(0, GamepadAxis::LeftStickVert) +
                      Input::GamepadAxisValue(0, GamepadAxis::RightStickVert);
            if (x < -deadZone) {
                this->cur_pad_joy |= joystick::left;
            }
            else if (x > deadZone) {
                this->cur_pad_joy |= joystick::right;
            }
            if (y < -deadZone) {
                this->cur_pad_joy |= joystick::up;
            }
            else if (y > deadZone) {
                this->cur_pad_joy |= joystick::down;
            }
        }
        if (!this->playbackBuffer.Empty()) {
            this->handleTextPlayback();
        }
        else {
            this->emu->put_input(this->cur_char, this->cur_kbd_joy, this->cur_pad_joy);
        }
    }
    else {
        this->emu->put_input(0, 0);
    }
}

//------------------------------------------------------------------------------
void
Keyboard::StartPlayback(Buffer&& buf) {
    this->playbackBuffer = std::move(buf);
    this->playbackPos = 0;
}

//------------------------------------------------------------------------------
void
Keyboard::handleTextPlayback() {
    if (this->playbackPos < this->playbackBuffer.Size()) {
        if (this->playbackCounter-- > 0) {
            this->emu->put_input(this->playbackChar, 0);
        }
        else {
            // alternate between press and release
            this->playbackFlipFlop = !this->playbackFlipFlop;
            if (this->playbackFlipFlop) {
                this->playbackCounter = 4;
                // feed the next character from buffer
                // filter out unwanted characters and convert
                do {
                    this->playbackChar = this->playbackBuffer.Data()[this->playbackPos++];
                }
                while ((this->playbackChar == '\t') || (this->playbackChar == '\r'));
                if (this->playbackChar == '\n') {
                    this->playbackChar = 0x0D;
                }
            }
            else {
                this->playbackChar = 0;
                this->playbackCounter = 4;
            }
        }
    }
    else {
        this->playbackBuffer.Clear();
        this->playbackPos = 0;
    }
}

} // namespace YAKC
