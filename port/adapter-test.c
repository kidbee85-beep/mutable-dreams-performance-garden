#include "vibe.h"
#include "dsp/dreams.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static unsigned notes;
void vibe_notes(int flags) { notes = (unsigned)flags; }

int main(void) {
    vibe_on_init(44100.f);
    assert(notes == VIBE_NOTES);
    const vibe_event tempo = {.type=VIBE_EV_TRANSPORT,.id=VIBE_TRANSPORT_TEMPO,.value=83.f};
    vibe_event note = {.type=VIBE_EV_NOTE,.id=60,.down=1,.value=.76f};
    vibe_on_event(&tempo);
    vibe_on_event(&note);
    float left[VIBE_MAX_BLOCK], right[VIBE_MAX_BLOCK];
    double energy = 0.;
    float peak = 0.f;
    unsigned sizes[] = {64,128,73,256};
    for (unsigned j=0;j<220;j++) {
        unsigned n = sizes[j%4];
        vibe_on_audio(NULL,left,right,(int)n);
        for (unsigned i=0;i<n;i++) {
            assert(isfinite(left[i]) && isfinite(right[i]));
            energy += left[i]*left[i]+right[i]*right[i];
            peak = fmaxf(peak,fmaxf(fabsf(left[i]),fabsf(right[i])));
        }
    }
    assert(energy>0.01 && peak<1.f && md_state()->tempo==83);
    assert(md_midi_held_port(0)==0);
    note.down=0;
    vibe_on_event(&note);
    assert(md_midi_held_port(0)==-1);
    vibe_event play = {.type=VIBE_EV_TRANSPORT,.id=VIBE_TRANSPORT_PLAY,.down=1};
    vibe_on_event(&play);
    vibe_on_audio(NULL,left,right,64);
    assert(md_playing());
    play.down=0;
    vibe_on_event(&play);
    vibe_on_audio(NULL,left,right,73);
    assert(!md_playing());
    vibe_on_init(48000.f);
    vibe_on_audio(NULL,left,right,64);
    for(unsigned i=0;i<64;i++) assert(left[i]==0.f && right[i]==0.f);
    printf("VIBE adapter + actual Mutable Dreams engine at 44.1 kHz, 4 block sizes, note on/off, transport, stereo peak %.4f.\n", peak);
}
