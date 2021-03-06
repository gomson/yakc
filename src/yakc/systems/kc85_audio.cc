//------------------------------------------------------------------------------
//  kc85_audio.cc
//------------------------------------------------------------------------------
#include "kc85_audio.h"

namespace YAKC {

//------------------------------------------------------------------------------
void
kc85_audio::init(breadboard* b) {
    this->board = b;
    this->board->speaker.init(board->clck.base_freq_khz, SOUND_SAMPLE_RATE);
    this->reset();
}

//------------------------------------------------------------------------------
void
kc85_audio::reset() {
    this->board->speaker.stop_all();
    this->channels[0] = channel_state();
    this->channels[1] = channel_state();
}

//------------------------------------------------------------------------------
void
kc85_audio::step(int cycles) {
    this->board->speaker.step(cycles);
}

//------------------------------------------------------------------------------
void
kc85_audio::ctc_write(int chn) {
    // has the CTC channel state changed since last time?
    const auto& ctc_chn = this->board->z80ctc.channels[chn];
    if ((ctc_chn.constant != this->channels[chn].ctc_constant) ||
        (ctc_chn.mode ^ this->channels[chn].ctc_mode)) {

        if (!(this->channels[chn].ctc_mode & z80ctc::RESET) && (ctc_chn.mode & z80ctc::RESET)) {
            // CTC channel has become inactive, call the stop-callback
            this->board->speaker.stop(chn);
            this->channels[chn].ctc_mode = ctc_chn.mode;
        }
        else if (!(ctc_chn.mode & z80ctc::RESET)) {
            // CTC channel has become active or constant has changed, call sound-callback
            int div = ctc_chn.constant * ((ctc_chn.mode & z80ctc::PRESCALER_256) ? 256 : 16);
            if (div > 0) {
                int hz = int((float(1750000) / float(div)) / 2.0f);
                this->board->speaker.start(chn, hz);
            }
            this->channels[chn].ctc_constant = ctc_chn.constant;
            this->channels[chn].ctc_mode = ctc_chn.mode;
        }
    }
}

} // namespace YAKC
