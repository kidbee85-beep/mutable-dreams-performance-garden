#include "pad-performance.h"
#include "garden-textures.h"
#include "garden-view-ids.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"
#include "dsp/dreams.h"

static const uint32_t page_ink[]={0x83d2a6,0xe2aa78,0x90cbbf,0xb4b8eb,0x85c7de,0xf0cb86};
static const char *notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static dream_pads pads;
static float stereo[(VIBE_MAX_BLOCK + 5) * 2];
/* SDK 26: the six engine tracks as dry stems on OUT 3-8, when the host has them (a DAW's multi-out plugin) */
static int16_t stem_pcm[(VIBE_MAX_BLOCK + 8) * 8];
static float stem_hist[MD_TRACKS][VIBE_MAX_BLOCK + 8];
static int stems_live;
static double rate_ratio, position;
static float host_rate = 48000.f;   /* (0.24) for the ear */
static unsigned buffered;
static int show_controls;
static int page;                /* the open view, enum View in garden-views.inc */
static unsigned track;
static uint8_t key_routes[128];
/* Pad rows, top to bottom in event order: TONE 0-3, TEXTURE 4-7, SPACE 8-11, FORM 12-15. */
enum { ROW_TONE, ROW_TEXTURE, ROW_SPACE, ROW_FORM };
enum { PAD_THIN = 12, PAD_PULSE, PAD_NEXT, PAD_GESTURE };
static const uint32_t colors[] = {0xe7c27d, 0x6ec7ac, 0x86a8eb, 0xc0a0ed};
static const char *lanes[] = {"TONE", "TEXTURE", "SPACE", "FORM"};
static const char *pad_names[PAD_COUNT] = {
    "ROOT","FIFTH","THIRD","NINTH", "FOREST","AIR","WATER","STARDUST",
    "BLOOM","ECHO","GRAIN","DUSK", "THIN","PULSE","NEXT","GESTURE"};
static const int tone_offsets[4] = {0, 7, 4, 14};
static garden_textures textures;
static dream_gesture gesture;
static uint8_t tone_notes[4][3], tone_channels[4], tone_root[4], arp_step[4];
static float tone_velocity[4];
static unsigned arp_held;
static int tone_mode(void);
static unsigned thin_mask;
static uint16_t favorites[6] = {85,101,94,215,70,244};   /* F1-F6 on LIVE: any of the 271 Biomes */
static unsigned touch_percent;
static int touch_set;           /* TOUCH was set by hand (or restored): only then it is saved */
static void say(const char *text);
static void mutate_dream(void);
static void notes_all_off(void);
static void editor_data(const vibe_event *event);
static const char *track_labels[] = {"T1","T2","T3","T4","T5","T6"};
static unsigned selected;
static int moment_active;  /* defined with the moments in garden-state.inc */
static int gesture_armed;  /* defined with the settings in garden-state.inc */
static unsigned latched;   /* pads held by SHIFT + pad (or a right-click on the stage) until tapped again */
static int shift_held, stage_shift;
static unsigned last_step;
static uint8_t cc_sent[9];

static void thin_end(void) {
    for (unsigned i=0;i<MD_TRACKS;i++)
        if ((thin_mask>>i)&1u && md_command_space()) md_command(MD_MUTE,(int)i,0);
    thin_mask=0;
}

/* Hold THIN: mute one to three of the busiest other tracks, harder with velocity.
 * Only tracks that were audible are touched, and exactly those come back. */
static void thin_begin(float velocity) {
    thin_end();
    const md_patch *p=md_state();
    unsigned left=1u+(unsigned)(velocity*2.99f);
    for (int i=md_active_tracks()-1;i>=0&&left;i--) {
        if ((unsigned)i==track||p->track[i].mute||!md_command_space()) continue;
        if (md_command(MD_MUTE,i,1)) {thin_mask|=1u<<i;left--;}
    }
}

static unsigned tone_track(void);

enum { TONE_NOTE, TONE_CHORD, TONE_ARP, TONE_MODES };
static const int arp_degrees[4]={0,2,4,7};

/* `degrees` scale steps above `note` in the Biome's key; stays below 128. */
static int scale_up(int note,int degrees) {
    const md_patch *p=md_state();
    int n=note;
    while (degrees>0&&n<127) {
        n++;
        if (md_quantize(n,p->root,p->scale)==n) degrees--;
    }
    return n;
}

static void tone_release(unsigned i) {
    for (unsigned k=0;k<3;k++) if (tone_notes[i][k]) {
        md_midi(0x80|tone_channels[i],tone_notes[i][k],0,0);
        tone_notes[i][k]=0;
    }
    arp_held&=~(1u<<i);
}

static void tones_off(void) {
    for (unsigned i=0;i<4;i++) tone_release(i);
}

static unsigned midi_velocity(float v) { return v>=1.f?127u:(unsigned)(v*126.f)+1u; }

static void tone_on(unsigned i,unsigned k,int note,float velocity) {
    if (note<1) note=1;
    if (note>127) note=127;
    tone_notes[i][k]=(uint8_t)note;
    md_midi(0x90|tone_channels[i],(unsigned)note,midi_velocity(velocity),0);
}

/* NOTE: one tone. CHORD: a triad stacked in the Biome's scale. ARP: the pad walks root, third,
 * fifth and octave of its chord on the engine's eighths while it is held (or latched). */
static void tone_pad(unsigned index,int down,float velocity) {
    tone_release(index);
    if (!down) return;
    const unsigned target=tone_track();
    const unsigned route=(unsigned)md_midi_route(target);
    if (!route) return;
    const md_patch *p=md_state();
    const int note=md_quantize(p->track[target].note+tone_offsets[index],p->root,p->scale);
    tone_channels[index]=(uint8_t)(route-1);
    tone_root[index]=(uint8_t)(note<1?1:note>127?127:note);
    tone_velocity[index]=velocity;
    tone_on(index,0,note,velocity);
    if (tone_mode()==TONE_CHORD) {
        tone_on(index,1,scale_up(note,2),velocity*.85f);
        tone_on(index,2,scale_up(note,4),velocity*.8f);
    } else if (tone_mode()==TONE_ARP) {
        arp_held|=1u<<index;
        arp_step[index]=0;
    }
}

/* Audio side, on every new even sixteenth while the dream plays. */
static void arp_tick(void) {
    for (unsigned i=0;i<4;i++) if ((arp_held>>i)&1u) {
        if (tone_notes[i][0]) md_midi(0x80|tone_channels[i],tone_notes[i][0],0,0);
        arp_step[i]=(uint8_t)((arp_step[i]+1)&3u);
        const float accent=arp_step[i]?.78f:1.f;
        tone_on(i,0,scale_up(tone_root[i],arp_degrees[arp_step[i]]),tone_velocity[i]*accent);
    }
}

static void radio_set(int on);
/* STOP, everywhere (button, stage, Dream Console, 8 x 8 grid, ITINERA F2, the DAW): transport, hands and the radio */
static void stop_all(void);
static void release_performance(void) {
    dream_pads_clear(&pads);
    tones_off();
    thin_end();
    gesture.mode=GESTURE_IDLE;
    gesture_armed=0;
    latched=0;
    moment_active=-1;
    notes_all_off();
}

static void select_biome(unsigned index) {
    thin_end();
    latched&=~(1u<<PAD_THIN);
    if (md_command_space()<2) return;
    md_command(MD_SCENE,0,favorites[index]);
    md_command(MD_SCENE_CONFIRM,0,1);
    selected=index;
}

#include "garden-listen.inc"
#include "garden-state.inc"
#include "garden-moments.inc"
#include "garden-worlds.inc"
#include "garden-views.inc"
#include "garden-tour.inc"
#include "garden-rec.inc"
#include "garden-notes.inc"
#include "garden-grid.inc"
#include "garden-radio.inc"
#include "garden-night.inc"
#include "garden-intro.inc"
#include "garden-tour-card.inc"
#include "garden-ui.inc"

static void stop_all(void) {
    md_command(MD_PLAY,0,0);
    release_performance();
    radio_set(0);
}

static void pad_action(unsigned id,int down,float velocity) {
    if (id/4==ROW_TONE) tone_pad(id,down,velocity);
    else if (id==PAD_THIN) {if (down) thin_begin(velocity); else thin_end();}
    else if (id==PAD_NEXT&&down&&(shift_held||stage_shift)) mutate_dream();
    else if (id==PAD_NEXT&&down) select_biome((selected+1)%6);
    else if (id==PAD_GESTURE&&down) {
        if ((shift_held||stage_shift)&&(gesture.mode==GESTURE_LOOPING||gesture.mode==GESTURE_OVERDUB))
            dream_gesture_overdub(&gesture);
        else gesture_press();
    }
}

/* SHIFT + NEXT: the same Biome, a new variation (new seed, new rotations). SHIFT + RETURN undoes. */
static void mutate_dream(void) {
    if (md_command_space()) { md_command(MD_MUTATE,0,1); say("A NEW VARIATION - SHIFT+RETURN UNDOES"); }
}

static int latchable(unsigned id) { return id<12||id==PAD_THIN||id==PAD_PULSE; }

/* SHIFT + pad latches it: it keeps sounding (tone held, texture or space open, THIN or PULSE on)
 * after the hand leaves, and the next tap on it lets go. Two hands free for the rest. */
static void pad_event(const vibe_event *event) {
    const unsigned id=(unsigned)event->id, bit=1u<<id;
    const float velocity=event->value>0.f?(event->value<1.f?event->value:1.f):0.f;
    if (latched&bit) {
        if (!event->down) return;
        latched&=~bit;
        const vibe_event up={.type=VIBE_EV_PAD,.id=(uint8_t)id,.down=0};
        dream_pads_event(&pads,&up);
        pad_action(id,0,0.f);
        return;
    }
    if (event->down&&(shift_held||stage_shift)&&latchable(id)) latched|=bit;
    dream_pads_event(&pads,event);
    pad_action(id,event->down,velocity);
}

static void stage_init(void);
static void stage_record(unsigned frames);
static void stage_event(const vibe_event *event);
static uint16_t *device_fb;
static int sticky_shift;            /* SHIFT on the stage / in the Dream Console: for the next control (0.18, garden-pages.inc) */          /* the display as drawn in vibe_on_draw this frame (mirrored by garden-pages.inc) */
static void display_click(int x, int y);
static void help_page_turn(int d);   /* (0.27, garden-draw.inc) */
static void open_group(int g);
static void stage_button(int id);
static void stage_encoder(int id, float steps);

void vibe_on_init(float sample_rate) {
    rate_ratio = sample_rate >= MD_RATE && sample_rate <= 192000.f ? MD_RATE / (double)sample_rate : 0.;
    position = 1.;
    host_rate = sample_rate > 1000.f ? sample_rate : 48000.f;
    listen_init_all(host_rate);
    buffered = 0;
    pads = (dream_pads){.pressure_channel=9};
    garden_textures_clear(&textures);
    dream_gesture_clear(&gesture);
    for (unsigned i=0;i<4;i++) tone_notes[i][0]=tone_notes[i][1]=tone_notes[i][2]=0;
    arp_held=0;
    thin_mask=0;
    settings_defaults();
    moments_clear();
    params_init();
    editor_init();
    latched=0;
    shift_held=stage_shift=0;
    last_step=0;
    for (unsigned i=0;i<9;i++) cc_sent[i]=0;
    page=V_LIVE;
    show_menu=0;
    stage_init();
    grid_init();
    show_controls = 0;
    vibe_notes(VIBE_NOTES);
    md_init();
    md_patch initial;
    md_preview_biome_kit(&initial,favorites[0]);
    md_load(&initial);
    selected = 0;
    track=0;
    state_restore();
    notes_init();
    listen_apply();
    if (!tour_done_saved) tour_start();   /* (0.26) the very first start: FIRST STEPS */
    for(unsigned i=0;i<128;i++) key_routes[i]=0;
    set_view(page);
    for (unsigned i=0;i<6;i++) vibe_label(VB_K1+(int)i,group_names[i]);
    rec_state=REC_IDLE;
    if (radio_autostart) radio_set(1);           /* a VIBE as a radio: on air at power-on */
    char sources[256];
    vibe_media_sources(sources,(int)sizeof sources);   /* asks the host for media events: a take is written through them */
    params_report();  /* Restored values must be visible even while the DAW suspends audio. */
}

void vibe_on_event(const vibe_event *event) {
    if (!rate_ratio) return;
    if (event->type == VIBE_EV_BUTTON) {
        if ((!event->down && event->id <= VB_F6 && (show_controls || show_menu)) ||
            (event->down && ((event->id >= VB_K1 && event->id <= VB_K6) ||
             event->id == VB_MENU || event->id == VB_HOME || event->id == VB_RETURN ||
             event->id == VB_LEFT || event->id == VB_RIGHT))) soft_hold_id = -1;
    }
    /* the first touch ends the intro */
    if (((event->type==VIBE_EV_PAD||event->type==VIBE_EV_BUTTON||event->type==VIBE_EV_NOTE)&&event->down)||
        event->type==VIBE_EV_ENCODER||(event->type==VIBE_EV_STAGE&&event->id==VIBE_STAGE_DOWN)) intro_dismiss();
    if (event->type == VIBE_EV_STAGE) {stage_event(event); return;}
    if (event->type == VIBE_EV_SAVE) {state_save(); return;}
    if (event->type == VIBE_EV_PARAM) {apply_param(event->id,event->value); return;}
    if (event->type == VIBE_EV_EDITOR) {editor_event(event); return;}
    if (event->type == VIBE_EV_EDITOR_DATA) {editor_data(event); return;}
    if (event->type == VIBE_EV_MEDIA) {rec_pump(); return;}
    if (event->type == VIBE_EV_BUTTON && event->id == VB_REC && event->down) {rec_button(); return;}
    if (event->type == VIBE_EV_PEDAL) {
        if (event->id==VIBE_PEDAL_SWITCH&&event->down) gesture_press();
        if (event->id==VIBE_PEDAL_EXPR) {
            settings.space_depth=clampf(event->value,0.f,1.f);
            show_depth(1);
        }
        return;
    }
    int focus_id;
    if (show_menu) {
        if (event->type==VIBE_EV_ENCODER&&(event->id==VE_1||event->id==VE_DATA)) {
            static float acc;
            acc+=event->value;
            const int steps=(int)acc;
            acc-=(float)steps;
            menu_cursor=clampi(menu_cursor+steps,0,V_COUNT-1);
            return;
        }
        if (event->type==VIBE_EV_BUTTON&&event->down&&(event->id==VB_ENTER||event->id==VB_F1||event->id==VB_DATA_PUSH)) { set_view(menu_cursor); return; }
        if (event->type==VIBE_EV_BUTTON&&event->down&&(event->id==VB_MENU||event->id==VB_RETURN)) { show_menu=0; return; }
        /* 0.18: nothing else acts blind on the page under the directory */
        if (event->type==VIBE_EV_BUTTON&&(event->id<=VB_F6||event->id==VB_HOME||event->id==VB_ENTER||event->id==VB_DATA_PUSH)) return;
        if (event->type==VIBE_EV_ENCODER&&event->id<=VE_4) return;
    }
    if (show_controls) {
        /* HOME help open: F-keys, ENTER and encoders 1-4 / DATA close it instead of acting unseen (0.18) */
        if (event->type==VIBE_EV_BUTTON&&event->down&&(event->id<=VB_F6||event->id==VB_ENTER||event->id==VB_DATA_PUSH)) { show_controls=0; return; }
        if (event->type==VIBE_EV_BUTTON&&event->down&&(event->id==VB_LEFT||event->id==VB_RIGHT)) { help_page_turn(event->id==VB_RIGHT ? 1 : -1); return; }   /* (0.27) */
        if (event->type==VIBE_EV_BUTTON&&!event->down&&(event->id<=VB_F6||event->id==VB_ENTER||event->id==VB_DATA_PUSH)) return;
        if (event->type==VIBE_EV_ENCODER&&(event->id<=VE_4||event->id==VE_DATA)) return;
    }
    if (!show_controls && !show_menu && vf_event(event,&focus_id)!=VF_NO) return;
    if (event->type == VIBE_EV_PAD) {
        /* a big grid: the top-left 4 x 4 are the performance pads, the rest is the grid's own (garden-grid.inc) */
        const int logical = pad_from_stage ? (int)event->id : grid_logical(event->id);
        if (logical < 0) { grid_function(event->id, event->down); return; }
        if (logical >= PAD_COUNT) return;
        vibe_event as_pad = *event;
        as_pad.id = (uint8_t)logical;
        pad_event(&as_pad);
        return;
    }
    if (event->type == VIBE_EV_BUTTON && event->id == VB_SHIFT) shift_held = event->down != 0;
    dream_pads_event(&pads,event);
    if (event->type == VIBE_EV_NOTE && event->midi[1] != VIBE_NOTE_PADS) {
        if(event->id>=128) return;
        unsigned channel=event->midi[0]&15;
        if(event->midi[1]==VIBE_NOTE_KEYS) {
            const unsigned old=key_routes[event->id];
            if(event->down) {
                const unsigned route=(unsigned)md_midi_route(track);
                if(!route) return;
                if(old) md_midi(0x80|(old-1),event->id,0,0);
                key_routes[event->id]=(uint8_t)route;
                channel=route-1;
            } else {
                if(!old) return;
                channel=old-1;
                key_routes[event->id]=0;
            }
        }
        const float value = event->value > 0.f ? event->value : 0.f;
        const unsigned velocity = value >= 1.f ? 127u : (unsigned)(value * 127.f + .5f);
        md_midi((event->down ? 0x90 : 0x80) | channel,(unsigned)event->id,velocity,0);
    } else if (event->type == VIBE_EV_MIDI) {
        /* DAW: Program Change on channel 16 picks a Biome by its place in the library (bank select CC 0 for 128+) */
        static uint8_t pc_bank;
        if (event->midi[0]==0xbf&&event->midi[1]==0) { pc_bank=event->midi[2]&3; return; }
        if (event->midi[0]==0xcf) {
            const unsigned at=pc_bank*128u+(event->midi[1]&127u);
            const int id=at<MD_BIOMES?md_biome_at(at):-1;
            if (id>=0) { load_biome_id(id); say("PROGRAM CHANGE - NEW BIOME"); }
            return;
        }
        /* all sound / notes off from a DAW: notes only – latches, the gesture loop and moments stay (0.18) */
        if ((event->midi[0]&0xf0)==0xb0&&(event->midi[1]==120||event->midi[1]==123)) { tones_off(); notes_all_off(); }
        md_midi(event->midi[0],event->midi[1],event->midi[2],0);
    } else if (event->type == VIBE_EV_TRANSPORT && event->id == VIBE_TRANSPORT_PLAY) {
        if (event->down) { md_command(MD_PLAY,0,1); song_play_starts(); } else stop_all();
    } else if (event->type == VIBE_EV_TRANSPORT && event->id == VIBE_TRANSPORT_TEMPO &&
               event->value >= 30.f && event->value <= 220.f) {
        md_command(MD_TEMPO,0,(int)(event->value + .5f));
    } else if (event->type == VIBE_EV_BUTTON && event->down) {
        if (event->id<=VB_F6) { if (soft_hold_page()) soft_hold_down(event->id-VB_F1); else soft_key(event->id-VB_F1); }   /* (0.25) */
        if (event->id>=VB_K1&&event->id<=VB_K6) group_key(event->id-VB_K1);
        if (event->id==VB_LEFT) set_view((page+V_COUNT-1)%V_COUNT);
        if (event->id==VB_RIGHT) set_view((page+1)%V_COUNT);
        if (event->id==VB_MENU) { show_menu=!show_menu; menu_cursor=page; show_controls=0; }
        if (event->id == VB_PLAY) md_command(MD_PLAY,0,1);
        if (event->id == VB_STOP) stop_all();
        if (event->id == VB_HOME) { tour_end(); show_controls = !show_controls; help_key_text[0] = 0; help_key_id = -1; }   /* (0.26) HOME also ends the tour */
        if (event->id == VB_RETURN && shift_held && !show_controls) { md_command(MD_RETURN,0,1); say("UNDONE"); return; }
        if (event->id == VB_RETURN) {
            if(show_controls) show_controls=0;
            else set_view(V_LIVE);
        }
    } else if (event->type == VIBE_EV_BUTTON && !event->down && event->id<=VB_F6) {
        soft_hold_up(event->id-VB_F1);                 /* (0.25) the soft key of LISTEN / SING / CV acts on release */
    } else if (event->type==VIBE_EV_ENCODER && (event->id==VE_A||event->id==VE_B) &&
               event->value>=-100.f && event->value<=100.f) {
        float *depth=event->id==VE_A?&settings.texture_depth:&settings.space_depth;
        *depth=clampf(*depth+event->value*.02f,0.f,1.f);
        show_depth(event->id==VE_B);
    } else if (event->type==VIBE_EV_ENCODER && event->id<=VE_4 &&
               event->value>=-100.f && event->value<=100.f) {
        view_encoder(event->id-VE_1,event->value);
    }
}

static float interpolate(float a, float b, float c, float d, float t) {
    return b + .5f*t*(c-a+t*(2.f*a-5.f*b+4.f*c-d+t*(3.f*(b-c)+d-a)));
}

void vibe_on_audio(const float *in, float *left, float *right, int frames) {
    if (frames <= 0) return;
    if (!rate_ratio || frames > VIBE_MAX_BLOCK) {
        for (int i=0;i<frames;i++) left[i]=right[i]=0.f;
        return;
    }
    const float source_frames=(float)(frames*rate_ratio);
    dream_pads_update(&pads,source_frames/(source_frames+350.f),
        source_frames/(source_frames+settings.tail*(MD_RATE/3.f)));
    gesture_grid_tick();
    params_report();
    /* PULSE breathes on the engine's eighths: realign the dip at every even sixteenth. */
    const unsigned step=md_status()->step;
    if (md_playing()&&step!=last_step&&!(step&1u)) {
        textures.pulse_phase=0.f;
        arp_tick();
    }
    gesture.speed=gesture_speed_value();
    last_step=step;
    const unsigned wanted=rate_ratio==1.?(unsigned)frames:(unsigned)(position+(frames-1)*rate_ratio)+3;
    const unsigned fresh=rate_ratio==1.?(unsigned)frames:wanted>buffered?wanted-buffered:0u;
    float live[GESTURE_LANES];
    for (unsigned i=0;i<GESTURE_LANES;i++) live[i]=pads.pads[4+i].level;
    dream_gesture_advance(&gesture,live,fresh);
    stage_record(fresh);
    int hands=gesture.mode!=GESTURE_IDLE||gesture_armed;
    for (unsigned i=0;i<PAD_COUNT&&!hands;i++) hands=pads.pads[i].held!=0;
    layers_update(fresh,hands);
    float lane[GESTURE_LANES];
    for (unsigned i=0;i<GESTURE_LANES;i++) {
        float v=live[i]>gesture.output[i]?live[i]:gesture.output[i];
        if (perf[i]>v) v=perf[i];
        if (moment_layer[i]>v) v=moment_layer[i];
        if (wander_layer[i]>v) v=wander_layer[i];
        sounding[i]=v;
        lane[i]=v*(i<4?settings.texture_depth:settings.space_depth);
    }
    float pulse=pads.pads[PAD_PULSE].level>perf[8]?pads.pads[PAD_PULSE].level:perf[8];
    if (moment_layer[8]>pulse) pulse=moment_layer[8];
    sounding[8]=pulse;
    md_performance_set(lane[4],lane[5],lane[6],lane[7]);
    if (settings.cc_out) {
        /* The performance as MIDI CC 20-28 on channel 1: external gear or a DAW track follows the hands. */
        for (unsigned i=0;i<9;i++) {
            const float v=i<8?lane[i]:pulse;
            const uint8_t b=(uint8_t)(v<=0.f?0:v>=1.f?127:v*127.f+.5f);
            if (b!=cc_sent[i]&&vibe_midi_send(0xb0,(uint8_t)(20+i),b)==0) cc_sent[i]=b;
        }
    }
    song_follow();                                      /* (0.25) the sequencer stands where the DAW's song stands */
    soft_hold_tick();                                   /* (0.26) a held soft key opens the help, drawn or not */
    tour_tick((float)frames / host_rate);               /* (0.26) FIRST STEPS: did the player do the step? */
    {   /* (0.26) CV IN, MOVES and KEY at a control rate (~100 Hz), drawn or not: only commands into the engine's queue */
        static float control_t; control_t += (float)frames / host_rate;
        if (control_t >= .01f) { control_t = 0.f; cv_in_follow(); moves_follow(); key_follow(); }
    }
    listen_audio_before(in,frames,fresh,host_rate);   /* (0.24) IN 1 / IN 2 into the effects, SING, CV */
    const md_patch *patch=md_state();
    stems_live = vibe_output_count() >= 2 + MD_TRACKS;
    uint32_t notes_before[MD_TRACKS];
    for (unsigned i=0;i<MD_TRACKS;i++) notes_before[i]=md_status()->events[i];
    if (rate_ratio == 1.) {
        if (stems_live) {
            md_render_stems(stereo,(unsigned)frames,(md_usb_stems){stem_pcm,0,(unsigned)frames});
            for (unsigned t=0;t<MD_TRACKS;t++) {
                float *o=vibe_output(2+(int)t);
                if (o) for (int i=0;i<frames;i++) o[i]=stem_pcm[i*8+(int)t]*(1.f/32767.f);
            }
        } else md_render(stereo,(unsigned)frames);
        garden_textures_render(&textures,stereo,(unsigned)frames,lane,pulse,
            (float)patch->tempo,patch->root,patch->scale);
        listen_ceiling_stereo(stereo,(unsigned)frames);   /* (0.30) REC records the mix that is heard */
        rec_capture(stereo,(unsigned)frames);
        notes_out_tick((unsigned)frames,notes_before);
        radio_tick((unsigned)frames);
        worlds_tick((unsigned)frames);
        for (int i=0;i<frames;i++) {
            left[i]=stereo[2*i];
            right[i]=stereo[2*i+1];
        }
        listen_audio_after(left,right,frames);
        return;
    }
    // Preserve the FM-1's fixed-rate envelopes and effect times. Retain the
    // interpolation neighbourhood across host buffers, including tiny DAW blocks.
    if (fresh) {
        if (stems_live) {
            md_render_stems(stereo+2*buffered,fresh,(md_usb_stems){stem_pcm,0,fresh});
            for (unsigned t=0;t<MD_TRACKS;t++)
                for (unsigned i=0;i<fresh;i++) stem_hist[t][buffered+i]=stem_pcm[i*8+t]*(1.f/32767.f);
        } else md_render(stereo+2*buffered,fresh);
        garden_textures_render(&textures,stereo+2*buffered,fresh,lane,pulse,
            (float)patch->tempo,patch->root,patch->scale);
        listen_ceiling_stereo(stereo+2*buffered,fresh);
        rec_capture(stereo+2*buffered,fresh);
        notes_out_tick(fresh,notes_before);
        radio_tick(fresh);
        worlds_tick(fresh);
    }
    if (wanted>buffered) buffered=wanted;
    float *stem_out[MD_TRACKS]={0};
    for (unsigned t=0;t<MD_TRACKS&&stems_live;t++) stem_out[t]=vibe_output(2+(int)t);
    if (stems_live) {
        double at=position;
        for (int i=0;i<frames;i++,at+=rate_ratio) {
            const unsigned base=(unsigned)at;
            const float t=(float)(at-base);
            for (unsigned k=0;k<MD_TRACKS;k++) if (stem_out[k]) {
                const float *h=stem_hist[k]+base;
                stem_out[k][i]=interpolate(h[-1],h[0],h[1],h[2],t);
            }
        }
    }
    for (int i=0;i<frames;i++,position+=rate_ratio) {
        const unsigned base=(unsigned)position;
        const float t=(float)(position-base);
        const float *sample=stereo+2*base;
        left[i]=interpolate(sample[-2],sample[0],sample[2],sample[4],t);
        right[i]=interpolate(sample[-1],sample[1],sample[3],sample[5],t);
    }
    const unsigned consumed=(unsigned)position-1;
    for (unsigned i=consumed;i<buffered;i++) {
        stereo[(i-consumed)*2]=stereo[2*i];
        stereo[(i-consumed)*2+1]=stereo[2*i+1];
    }
    if (stems_live) for (unsigned k=0;k<MD_TRACKS;k++) for (unsigned i=consumed;i<buffered;i++) stem_hist[k][i-consumed]=stem_hist[k][i];
    buffered-=consumed;
    position-=consumed;
    listen_audio_after(left,right,frames);
}

/* The LIVE page's picture (0.19): the night garden, as on the Brick and the MPC (garden-night.inc). Each track a
 * plant of its sound's species, the macros as weather, the texture and space lanes as their own weather. */
static uint16_t nd_display_sky[452 * 178];
static nd_scene nd_display = {.sky = nd_display_sky, .w = 452, .h = 178, .biome = -1, .left = .09f, .step = .164f, .grow = 1.3f};
static void night_lanes(float out[8]) {
    for (unsigned i = 0; i < 8; i++) out[i] = sounding[i] * (i < 4 ? settings.texture_depth : settings.space_depth);
}
static void nd_input_rain(float x, float y, float w, float h, float level, int note, float t);   /* (0.24, garden-listen-draw.inc) */
static void atlas(void) {
    float lanes[8];
    night_lanes(lanes);
    if (device_fb) nd_draw(&nd_display, device_fb, 480, 320, 14, 96, 452, 178, lanes, (int)track, 0);
    if (device_fb && lis.on) { nd_pixels(device_fb, 480, 320); nd_input_rain(14.f, 96.f, 452.f, 178.f, lis_level, lis_note, (float)(md_status()->frames % 1764000u) / 44100.f); }
    const int count = md_active_tracks();
    for (int i = 0; i < count; i++) {
        const uint32_t c = md_state()->track[i].mute ? 0x5a6472 : (nd_track_col(i) & 0xffffffu);
        vg_text(track_labels[i], nd_display.plant_px[i] ? nd_display.plant_px[i] : 59 + i * 72, 251, VG_SMALL, i == (int)track ? 0xfffcf4 : c, VG_CENTER);
    }
}

static float pad_light(unsigned index) {
    float level=pads.pads[index].level;
    if (index>=4&&index<12&&gesture.output[index-4]>level) level=gesture.output[index-4];
    if (index>=4&&index<12&&moment_layer[index-4]>level) level=moment_layer[index-4];
    if (index>=4&&index<12&&wander_layer[index-4]>level) level=wander_layer[index-4];
    if (index==PAD_GESTURE) {
        if (gesture.mode==GESTURE_RECORDING) level=(md_status()->frames/11025u)&1u?1.f:.45f;
        else if (gesture.mode==GESTURE_LOOPING) level=.55f+.4f*(1.f-dream_gesture_progress(&gesture));
        else if (gesture.mode==GESTURE_OVERDUB) level=(md_status()->frames/11025u)&1u?1.f:.5f;
    }
    if (index==PAD_GESTURE&&gesture_armed) level=(md_status()->frames/5512u)&1u?.9f:.3f;
    if (index==PAD_THIN&&thin_mask) level=level>.6f?level:.6f;
    return level;
}

static uint32_t pad_color(unsigned index) {
    if (index==PAD_GESTURE&&gesture.mode==GESTURE_RECORDING) return 0xe8705e;
    if (index==PAD_GESTURE&&gesture.mode==GESTURE_OVERDUB) return 0xe8a25e;
    return colors[index/4];
}

static void draw_fx(void) {
    for(unsigned lane=0;lane<4;lane++) {
        const int y=100+40*(int)lane;
        static const char *const tone_rows[TONE_MODES]={"TONE","CHORD","ARP"};
        vg_text(lane==ROW_TONE?tone_rows[settings.tone_mode]:lanes[lane],22,y+10,VG_BOLD,colors[lane],VG_LEFT);
        for(unsigned col=0;col<4;col++) {
            const unsigned index=lane*4+col;
            const float level=pad_light(index);
            const int x=118+86*(int)col;
            const uint32_t ink=pad_color(index);
            vg_round(x,y,78,33,8,vg_mix(0x172131,ink,.18f+.7f*level));
            if (device_fb) {   /* (0.21) the pad's sign, as on the stage and in the Dream Console */
                nd_pixels(device_fb,480,320);
                nd_pad_sign((int)index,(float)(x+10),(float)(y+16),6.f,level>.55f?0xFF0a1719u:(ink|0xFF000000u),level,
                            (float)(md_status()->frames%2646000u)/44100.f);
            }
            const char *name=pad_names[index];
            if (index==PAD_GESTURE) name=gesture_label();
            if (latched&(1u<<index)) vg_circle((float)(x+70),(float)(y+8),3.f,0xf2fbf7);
            vg_text(name,x+46,y+8,VG_SMALL,level>.55f?0x0a1719:0xd7ebe3,VG_CENTER);
            const float bar=index==PAD_GESTURE&&gesture.mode!=GESTURE_IDLE?dream_gesture_progress(&gesture):level;
            vg_rect(x+8,y+25,(int)(62.f*bar),3,ink);
        }
    }
    vg_text(shift_held&&page==V_PADSET?"SHIFT + F1-F6 stores a moment":
            shift_held?"SHIFT: pad latches, 16 layers":
            page==V_PADSET?(moment_active>=0?"Same F key glides out":"F1-F6 recall moments"):
            gesture.mode==GESTURE_OVERDUB?"Layer: one more pass on the loop":
            gesture_armed?"Gesture starts on the next bar":
            gesture.mode==GESTURE_LOOPING?"Gesture loops - tap 16 to stop":
            gesture.mode==GESTURE_RECORDING?(settings.bars==8?"Capturing 5-12 for 8 bars":"Capturing 5-12 for 4 bars"):
            page==V_PADSET?"Pad settings - FX again: space":"FX again: pad settings",
            22,264,VG_SMALL,0xa1c6ba,VG_LEFT);
    char depth[24];
    percent_text("A TEXTURE",settings.texture_depth,depth);
    vg_text(depth,330,264,VG_SMALL,colors[ROW_TEXTURE],VG_RIGHT);
    percent_text("B SPACE",settings.space_depth,depth);
    vg_text(depth,458,264,VG_SMALL,colors[ROW_SPACE],VG_RIGHT);
}

#include "garden-draw.inc"

/* (0.32) the device's lights as part of the garden: the pads breathe like grass in a slow wind and nod on every beat,
   PLAY is the garden's pulse, the six group keys sway, the rest glows like moonlight – lit when it means something. */
static void garden_leds(void) {
    const float t = (float)(md_status()->frames % 2646000u) / 44100.f;
    const float nod = garden_nod();
    for (unsigned i = 0; i < PAD_COUNT; i++) {
        const float idle = .07f + .1f * garden_wind(i) + .1f * nod;
        vibe_led_pad(grid_id((int)i), pad_color(i), idle + (1.f - idle) * pad_light(i));
    }
    if (!show_menu && !show_controls) {
        const int grp = views[page].group;
        for (int g = 0; g < 6; g++)
            if (g != grp) vibe_led_button(VB_K1 + g, group_ink[g], .08f + .1f * (.5f + .5f * vd_sin(t * .2f - (float)g * .12f)));
    }
    const float breath = .5f + .5f * vd_sin(t * .16f);
    vibe_led_button(VB_PLAY, 0x83d2a6, md_playing() ? .35f + .6f * nod : .12f + .2f * breath);
    vibe_led_button(VB_STOP, 0xe8a25e, md_playing() ? .14f : .05f);
    static const int moon[] = {VB_SHIFT, VB_OCT_DOWN, VB_OCT_UP, VB_ENTER, VB_RETURN, VB_LEFT, VB_RIGHT};
    for (unsigned k = 0; k < sizeof moon / sizeof moon[0]; k++) vibe_led_button(moon[k], 0xc8d8f0, .06f + .03f * breath);
    vibe_led_button(VB_MENU, 0xc0a0ed, show_menu ? .8f : .1f);
    vibe_led_button(VB_HOME, 0xf0cb86, show_controls ? .8f : .1f);
}

#include "garden-link.inc"
#include "garden-editor.inc"

void vibe_on_draw(uint16_t *fb) {
    device_fb = fb;
    nd_frame_begin(2);   /* (the display: its night and the LIVE garden in the same frame) */
    vg_begin(fb);
    vg_clear(0x0a1719);
    if (!rate_ratio) {
        vg_text("Use a host rate from 44.1 to 192 kHz",20,84,VG_BIG,0xe7a77d,VG_LEFT);
        return;
    }
    {   /* 0.20: every page over the Biome's own night (garden-night.inc), the panels as glass */
        static uint16_t sky[480*320];
        static nd_scene night={.sky=sky,.w=480,.h=320,.biome=-1};
        nd_backdrop(&night,fb,480,320,0,0,480,320,.30f);   /* (0.32) less dimmed: the Biome's sky shows */
    }
    rec_pump();
    {
        const float t=md_status()->frames/44100.f;
        const int blink=((int)(t*2.f))&1;
        vibe_led_button(VB_REC,rec_state==REC_WRITING?0xe8a25e:0xe8705e,rec_state==REC_TAKING?(blink?.9f:.35f):rec_state==REC_WRITING?.7f:.08f);
    }
    if (show_menu) draw_menu();
    else if (show_controls) draw_help();          /* (0.16: the pads, Link and the Dream Console stay current under HOME) */
    else draw_view();
    garden_leds();
    grid_leds();
    if (!show_menu && !show_controls) vf_end(0xe7a77d);
    if (!show_menu && !show_controls && tour_step >= 0) tour_card(fb, 480, 320, 12.f, 196.f, 456.f, 84.f, 0);   /* (0.26) FIRST STEPS */
    if (intro_active()) draw_intro(fb,480,320,1);
    link_publish();
    link_draw(fb);
    editor_status();
}

#include "garden-stage.inc"
