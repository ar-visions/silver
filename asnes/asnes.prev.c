#include <import>

static none asnes_spc_step(spc_state spc);
static i32  asnes_step(asnes a);
static none asnes_generate_audio(asnes a);


static uint8_t cpu_read_byte(asnes a, u16 addr) {
    if (addr >= 0x2140 && addr <= 0x2143) {
        // SPC ports
        uint8_t value = a->spc->ports[addr - 0x2140];
        printf("CPU read 0x%02X from SPC port $%04X\n", value, addr);
        return value;
    } else if (addr >= 0x2100 && addr <= 0x213F) {
        // PPU registers
        printf("PPU read from $%04X\n", addr);
        if (addr == 0x2137)
            return 0x80;  // VBlank
        if (addr == 0x2138)
            return 0x00;  // OAM data
        if (addr == 0x2139)
            return 0x00;  // VRAM data low
        if (addr == 0x213A)
            return 0x00;  // VRAM data high
        return 0x00;
    } else if (addr >= 0x4000 && addr <= 0x43FF) {
        // System/DMA/Controllers
        printf("Hardware read from $%04X\n", addr);
        if (addr == 0x4212)
            return 0x80;  // H/V-Blank status
        if (addr == 0x4016)
            return 0x00;  // Controller 1
        if (addr == 0x4017)
            return 0x00;  // Controller 2
        return 0x00;
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // RAM mirror regions
        return a->cpu->memory[addr & 0x1FFF];
    } else if (addr >= 0x6000 && addr <= 0x7FFF) {
        // SRAM/Battery RAM
        return a->cpu->memory[addr];
    }
    return a->cpu->memory[addr];
}

static none cpu_write_byte(asnes a, u16 addr, u8 value) {
    if (addr >= 0x2140 && addr <= 0x2143) {
        // SPC ports
        a->spc->ports[addr - 0x2140] = value;
        printf("CPU write 0x%02X to SPC port $%04X\n", value, addr);
    } else if (addr >= 0x2100 && addr <= 0x213F) {
        // PPU registers
        printf("PPU write: $%04X = 0x%02X\n", addr, value);
        // Just acknowledge, don't implement full PPU
    } else if (addr >= 0x4000 && addr <= 0x43FF) {
        // System/DMA/Controllers
        printf("Hardware write: $%04X = 0x%02X\n", addr, value);
        // Just acknowledge
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // RAM (and mirrors)
        a->cpu->memory[addr & 0x1FFF] = value;
    } else if (addr >= 0x6000 && addr <= 0x7FFF) {
        // SRAM/Battery RAM
        a->cpu->memory[addr] = value;
    } else {
        // ROM and everything else
        a->cpu->memory[addr] = value;
    }
}

// Add these helper functions at the top of your SPC file
static inline u8 spc_read(spc_state spc, u16 addr) {
    if (addr >= 0xF4 && addr <= 0xF7) {
        return spc->ports[addr - 0xF4];
    }
    return spc->ram[addr];
}

static inline void spc_write(spc_state spc, u16 addr, u8 value) {
    if (addr >= 0xF4 && addr <= 0xF7) {
        spc->ports[addr - 0xF4] = value;
        printf("SPC700 wrote 0x%02X to port $F%X\n", value, addr & 0xF);
    } else {
        spc->ram[addr] = value;
    }
}

#define spc_R(addr)       spc_read(spc, addr)
#define spc_W(addr,value) spc_write(spc, addr, value)

#define cpu_R(addr)      cpu_read(a, addr)
#define cpu_W(addr, val) cpu_write(a, addr, val)


static inline uint16_t cpu_get_A(cpu_state cpu) {
    if (cpu->p & FLAG_M) {
        return cpu->a;
    } else {
        return cpu->a | (cpu->b << 8);
    }
}
static inline void cpu_set_A(cpu_state cpu, uint16_t v) {
    if (cpu->p & FLAG_M) {
        cpu->a = v & 0xFF;
    } else {
        cpu->a = v & 0xFF;
        cpu->b = v >> 8;
    }
}

static inline uint16_t cpu_get_X(cpu_state cpu) {
    if (cpu->p & FLAG_X) {
        return cpu->x;
    } else {
        return cpu->x | (cpu->xh << 8);
    }
}
static inline void cpu_set_X(cpu_state cpu, uint16_t v) {
    if (cpu->p & FLAG_X) {
        cpu->x = v & 0xFF;
    } else {
        cpu->x = v & 0xFF;
        cpu->xh = v >> 8;
    }
}

// and similarly for Y...

// Memory reads/writes that depend on accumulator width:
static inline uint16_t cpu_read_mem(cpu_state cpu, uint32_t addr) {
    if (cpu->p & FLAG_M) {
        return cpu_R(addr);
    } else {
        uint16_t lo = cpu_R(addr);
        uint16_t hi = cpu_R(addr + 1);
        return lo | (hi << 8);
    }
}
static inline void cpu_write_mem(cpu_state cpu, uint32_t addr, uint16_t v) {
    if (cpu->p & FLAG_M) {
        cpu_W(addr, v & 0xFF);
    } else {
        cpu_W(addr,       v & 0xFF);
        cpu_W(addr + 1, v >> 8);
    }
}


map asnes_interface(asnes a, window w) {
    return null;
}

map asnes_render(asnes a, window w) {
    if (!a->cpu) return null;

    // Step both processors
    for (int i = 0; i < 1000; i++) {  // Run 1000 cycles
        i32 cpu_cycles = asnes_step(a);           // Main CPU
        a->spc->cycle_accum += cpu_cycles * SPC_RATIO_NUM;
        while (a->spc->cycle_accum >= SPC_RATIO_DEN) {
            asnes_spc_step(a->spc);              // run one SPC cycle
            a->spc->cycle_accum -= SPC_RATIO_DEN;
        }
    }
    
    // Generate audio periodically
    asnes_generate_audio(a);

    return m(
        "background", background(
            blur,        false,
            clear_color, vec4f(0.0f, 0.1f, 0.2f, 1.0f),
            elements,    m()
        )
    );
}

static bool asnes_reset_spc(asnes a) {
    static u8   spc_boot_rom[64] = { };
    static bool loaded           = false;
    if (!loaded) {
        FILE*  f    = fopen(cstring(a->spc_rom), "rb");
        size_t size = 0;
        verify(f, "could not open spc-rom file: %o", a->spc_rom);
        fseek (f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        verify(size == 64, "spc program not found; provide with --spc_rom (expected %i byte program, found %i bytes)",
            64, size);
        verify(fread(spc_boot_rom, size, 1, f) == 1, "failed to read %i bytes", (int)size);
        fclose(f);
        loaded = true;
    }

    spc_state spc = a->spc = calloc(1, sizeof(struct spc_state));
    cpu_state cpu = a->cpu;

    // copy boot ROM to high memory
    memcpy(spc->ram + 0xFFC0, spc_boot_rom, 64);
    
    // Initial SPC700 registers (similar to your CPU init)
    spc->pc           = 0xFFC0;  // Boot ROM start
    spc->sp           = 0xFF;    // Stack starts at top
    spc->psw          = 0x02;    // Initial flags
    spc->a            = 0;
    spc->x            = 0;
    spc->y            = 0;
    spc->sample_rate  = 32000;
    spc->buffer_size  = 1024;  // Adjust for latency
    spc->audio_buffer = calloc(1, spc->buffer_size * 2 * sizeof(u16));  // 16-bit stereo
    
    int err = snd_pcm_open (&spc->pcm_handle, "pulse", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) return false;
    
    // PCM parameters
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca      (&params);
    snd_pcm_hw_params_any         (spc->pcm_handle, params);
    snd_pcm_hw_params_set_access  (spc->pcm_handle, params,  SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format  (spc->pcm_handle, params,  SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(spc->pcm_handle, params, 2);
    snd_pcm_hw_params_set_rate    (spc->pcm_handle, params, spc->sample_rate, 0);

    memset(cpu->ports, 0, 4);
    memset(spc->ports, 0, 4);
    spc->ports[0] = 0xAA;  // SPC ready signal
    spc->ports[1] = 0xBB;  // Boot ROM ready

    /// result boolean result
    bool success = snd_pcm_hw_params(spc->pcm_handle, params) >= 0;
    if (success) {
        int err = snd_pcm_prepare(spc->pcm_handle);
        if (err < 0) {
            print("ALSA prepare failed: %s", snd_strerror(err));
            success = false;
        }
    }
    return success;
}

bool asnes_load(asnes a) {
    FILE* f = fopen(cstring(a->rom), "rb");
    verify(f, "could not open ROM file");

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    verify(size < MEM_SIZE, "ROM too large");

    cpu_state cpu = a->cpu = calloc(1, sizeof(struct cpu_state));
    cpu->memory   = calloc(1, MEM_SIZE);

    fread(cpu->memory + ROM_START, 1, size, f);
    // Mirror ROM to lower half (0x0000-0x7FFF)
    memcpy(cpu->memory, cpu->memory + 0x8000, 0x8000);
    fclose(f);

    print("loaded rom: %s (%zu bytes)", cstring(a->rom), size);

    // fetch reset vector from bank 0x00:0xFFFC/0xFFFD
    u16 reset_vector = cpu->memory[0xFFFC] | (cpu->memory[0xFFFD] << 8);
    print("reset vector: 0x%04X", reset_vector);
    print("first op: 0x%02X", cpu->memory[reset_vector]);

    printf("IRQ vector at 0xFFFE/0xFFFF: 0x%02X%02X\n",
        cpu->memory[0xFFFF], cpu->memory[0xFFFE]);
    // set PC and initial CPU state
    cpu->pc      = reset_vector;  // pc to reset_vector
    cpu->sp      = 0x01FF & 0xFF; // stack pointer 8-bit mode
    cpu->p       = 0x34;          // IRQ disable, 8-bit A and index
    
    asnes_reset_spc(a);
    return true;
}

none asnes_init(asnes a) {
    trinity t  = a->t = trinity();
    int width  = 256 * 2;
    int height = 224 * 2;

    a->w = window(
        t,              t,
        title,          string("asnes"),
        width,          width,
        height,         height);
    
    initialize(a, a->w);
}

none asnes_dealloc(asnes a) {
    free(a->cpu);
    free(a->spc);
}

int main(int argc, cstr argv[]) {
    asnes emu = hold(asnes(argv));
    if (emu->rom)
        verify(load(emu), "failed to load rom: %o", emu->rom);
    return run(emu);
}

static const i16 gauss_table[512] = {
     -3,   -3,   -3,   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1,    0,    0,    0,    1,    1,
      1,    2,    2,    2,    3,    3,    4,    4,    5,    5,    6,    6,    7,    8,    8,    9,
     10,   10,   11,   12,   13,   14,   15,   16,   17,   18,   20,   21,   22,   24,   25,   27,
     28,   30,   32,   33,   35,   37,   39,   41,   43,   45,   47,   50,   52,   54,   57,   59,
     62,   65,   67,   70,   73,   76,   79,   82,   85,   88,   91,   94,   98,  101,  104,  108,
    111,  115,  118,  122,  126,  129,  133,  137,  141,  145,  149,  153,  157,  161,  165,  169,
    174,  178,  182,  187,  191,  196,  200,  205,  209,  214,  219,  223,  228,  233,  238,  243,
    248,  253,  258,  263,  268,  273,  278,  283,  288,  293,  299,  304,  309,  314,  319,  324,
    329,  334,  340,  345,  350,  355,  360,  365,  370,  375,  380,  385,  390,  395,  400,  405,
    410,  415,  420,  425,  430,  435,  440,  445,  450,  455,  459,  464,  469,  474,  479,  483,
    488,  493,  497,  502,  507,  511,  516,  520,  525,  529,  534,  538,  542,  547,  551,  555,
    560,  564,  568,  572,  576,  580,  584,  588,  592,  596,  600,  603,  607,  611,  614,  618,
    621,  625,  628,  631,  634,  638,  641,  644,  646,  649,  652,  655,  657,  660,  662,  664,
    667,  669,  671,  673,  675,  677,  679,  681,  683,  684,  686,  687,  689,  690,  691,  692,
    693,  694,  695,  696,  697,  698,  698,  699,  699,  700,  700,  700,  700,  700,  700,  700,
    700,  699,  699,  698,  698,  697,  696,  695,  694,  693,  692,  691,  690,  689,  687,  686,
    684,  683,  681,  679,  677,  675,  673,  671,  669,  667,  664,  662,  660,  657,  655,  652,
    649,  646,  644,  641,  638,  634,  631,  628,  625,  621,  618,  614,  611,  607,  603,  600,
    596,  592,  588,  584,  580,  576,  572,  568,  564,  560,  555,  551,  547,  542,  538,  534,
    529,  525,  520,  516,  511,  507,  502,  497,  493,  488,  483,  479,  474,  469,  464,  459,
    455,  450,  445,  440,  435,  430,  425,  420,  415,  410,  405,  400,  395,  390,  385,  380,
    375,  370,  365,  360,  355,  350,  345,  340,  334,  329,  324,  319,  314,  309,  304,  299,
    293,  288,  283,  278,  273,  268,  263,  258,  253,  248,  243,  238,  233,  228,  223,  219,
    214,  209,  205,  200,  196,  191,  187,  182,  178,  174,  169,  165,  161,  157,  153,  149,
    145,  141,  137,  133,  129,  126,  122,  118,  115,  111,  108,  104,  101,   98,   94,   91,
     88,   85,   82,   79,   76,   73,   70,   67,   65,   62,   59,   57,   54,   52,   50,   47,
     45,   43,   41,   39,   37,   35,   33,   32,   30,   28,   27,   25,   24,   22,   21,   20,
     18,   17,   16,   15,   14,   13,   12,   11,   10,   10,    9,    8,    8,    7,    6,    6,
      5,    5,    4,    4,    3,    3,    2,    2,    2,    1,    1,    1,    0,    0,    0,   -1,
     -1,   -1,   -1,   -2,   -2,   -2,   -2,   -3,   -3,   -3
};

i16 gaussian_interpolate(int frac, i16* s) {
    frac &= 0xFF;

    int32_t result = 0;
    result += gauss_table[  0 + frac] * s[3];  // s[-3]
    result += gauss_table[128 + frac] * s[2];  // s[-2]
    result += gauss_table[256 - frac] * s[1];  // s[-1]
    result += gauss_table[384 - frac] * s[0];  // s[ 0]

    result >>= 11;

    if (result >  32767) result =  32767;
    if (result < -32768) result = -32768;

    return (i16)result;
}

none echo_processing(spc_state spc, f32* left_mix, f32* right_mix) {
    u8  echo_delay    = spc_R(0xF0 + 0x7D); // EDL (echo delay length)

    // echo buffer size
    u16 echo_length = echo_delay * 0x800; // Each delay unit = 2KB
    if (echo_length == 0) return;

    u8  echo_enable   = spc_R(0xF0 + 0x4D); // EON register (which channels feed echo)
    u8  echo_volume_l = spc_R(0xF0 + 0x2C); // EVOL_L
    u8  echo_volume_r = spc_R(0xF0 + 0x3C); // EVOL_R
    u8  echo_feedback = spc_R(0xF0 + 0x0D); // EFB (echo feedback)
    u16 echo_start    = spc_R(0xF0 + 0x6D) << 8; // ESA (echo start address)

    // Accumulate dry signal from channels that feed echo
    float echo_input_l = 0;
    float echo_input_r = 0;

    for (int ch = 0; ch < 8; ch++) {
        if (echo_enable & (1 << ch)) {
            echo_input_l += spc->channels[ch].output_l;
            echo_input_r += spc->channels[ch].output_r;
        }
    }

    // Read old samples from echo buffer in SPC RAM
    u16 echo_addr_l = echo_start + (spc->echo_pos * 4);     // Left sample
    u16 echo_addr_r = echo_start + (spc->echo_pos * 4) + 2; // Right sample

    i16 old_echo_l = spc_R(echo_addr_l) | (spc_R(echo_addr_l + 1) << 8);
    i16 old_echo_r = spc_R(echo_addr_r) | (spc_R(echo_addr_r + 1) << 8);

    // Convert to float
    float old_echo_l_f = old_echo_l / 32768.0f;
    float old_echo_r_f = old_echo_r / 32768.0f;

    // Apply 8-tap FIR filter to old echo samples
    // SPC700 has 8 FIR coefficients at $0F + 0x0F, 0x1F, 0x2F, etc.
    float filtered_l = 0;
    float filtered_r = 0;

    for (int tap = 0; tap < 8; tap++) {
        i8 coeff = (i8)spc_R(0xF0 + 0x0F + (tap * 0x10)); // FIR coefficients
        
        // Read samples from different positions in echo buffer
        u16 tap_pos = (spc->echo_pos + tap * (echo_length / 8)) % (echo_length / 4);
        u16 tap_addr_l = echo_start + (tap_pos * 4);
        u16 tap_addr_r = echo_start + (tap_pos * 4) + 2;
        
        i16 tap_sample_l = spc_R(tap_addr_l) | (spc_R(tap_addr_l + 1) << 8);
        i16 tap_sample_r = spc_R(tap_addr_r) | (spc_R(tap_addr_r + 1) << 8);
        
        filtered_l += (tap_sample_l / 32768.0f) * (coeff / 128.0f);
        filtered_r += (tap_sample_r / 32768.0f) * (coeff / 128.0f);
    }

    // Calculate new echo output (old echo + feedback)
    float echo_out_l = filtered_l + (spc->echo_history_l * (i8)echo_feedback / 128.0f);
    float echo_out_r = filtered_r + (spc->echo_history_r * (i8)echo_feedback / 128.0f);

    // Clamp echo output
    echo_out_l = clampf(echo_out_l, -1.0f, 1.0f);
    echo_out_r = clampf(echo_out_r, -1.0f, 1.0f);

    // Store new echo input + feedback into echo buffer
    float new_echo_l = echo_input_l + echo_out_l;
    float new_echo_r = echo_input_r + echo_out_r;

    i16 new_echo_l_i16 = (i16)(new_echo_l * 32767);
    i16 new_echo_r_i16 = (i16)(new_echo_r * 32767);

    // Write back to SPC RAM echo buffer
    spc_W(echo_addr_l,     new_echo_l_i16 & 0xFF);
    spc_W(echo_addr_l + 1, new_echo_l_i16 >> 8);
    spc_W(echo_addr_r,     new_echo_r_i16 & 0xFF);
    spc_W(echo_addr_r + 1, new_echo_r_i16 >> 8);

    // Advance echo buffer position
    spc->echo_pos = (spc->echo_pos + 1) % (echo_length / 4);

    // Save echo output for next frame's feedback
    spc->echo_history_l = echo_out_l;
    spc->echo_history_r = echo_out_r;

    // mix echo into final output
    *left_mix  += echo_out_l * ((i8)echo_volume_l / 128.0f);
    *right_mix += echo_out_r * ((i8)echo_volume_r / 128.0f);
}

static none asnes_generate_audio(asnes a) {
    spc_state    spc      = a->spc;
    static float phase    = 0;
    i16*         buffer   = (i16*)a->spc->audio_buffer;
    dsp_channel* channels = a->spc->channels;
    int samples_per_frame = a->spc->sample_rate / 60; // todo: support N frame-rate
    snd_pcm_sframes_t avail = snd_pcm_avail(a->spc->pcm_handle);
    if (avail < 0) {
        if (avail == -EPIPE) {
            print("audio underrun, recovering...");
            snd_pcm_recover(a->spc->pcm_handle, avail, 0);
            snd_pcm_prepare(a->spc->pcm_handle);
            return; // Skip this frame, try again next time
        } else {
            print("ALSA error: %s", snd_strerror(avail));
            return;
        }
    }

    for (int i = 0; i < a->spc->buffer_size; i++) {
        f32 left  = 0.0f;
        f32 right = 0.0f;

        for (u16 ch = 0; ch < 8; ch++) {
            // channel enable check
            u8 key_on = spc_R(0xF0 + 0x4C);             // KON register
            if (!(key_on & (1 << ch))) continue;        // if channel not enabled, continue to next

            u8 ch_base = ch * 0x10;
            u8 vol_l   = spc_R(0xF0 + ch_base + 0x00);
            u8 vol_r   = spc_R(0xF0 + ch_base + 0x01);
            u8 pitch_l = spc_R(0xF0 + ch_base + 0x02); // pitch low
            u8 pitch_h = spc_R(0xF0 + ch_base + 0x03); // pitch high
            u8 src_n   = spc_R(0xF0 + ch_base + 0x04); // source number
            u8 adsr1   = spc_R(0xF0 + ch_base + 0x05);
            u8 adsr2   = spc_R(0xF0 + ch_base + 0x06);
            u8 gain    = spc_R(0xF0 + ch_base + 0x07);

            /// 14bit pitch
            u16 pitch = pitch_l | ((pitch_h & 0x3F) << 8);
            if (pitch == 0) continue;

            dsp_channel* chan = &channels[ch];

            // get sample source directory
            u8 dir_page = spc_R(0xF0 + 0x5D);  // DIR register
            u16 src_dir = (dir_page << 8) + (src_n * 4);

            // read BRR sample start address
            u16 sample_start = spc_R(src_dir)     | (spc_R(src_dir + 1) << 8);
            u16 sample_loop  = spc_R(src_dir + 2) | (spc_R(src_dir + 3) << 8);
            
            // Advance sample position based on pitch
            chan->frac += pitch;
            while (chan->frac >= 0x1000) {
                chan->frac -= 0x1000;
                chan->pos  ++;

                u16 block_pos    = sample_start + (chan->pos / 16) * 9;
                u8  block_header = spc_R(block_pos);
                u8  sample_idx   = chan->pos % 16;

                if (sample_idx < 2) {
                    // read header byte for new block
                    u8 shift     = (block_header >> 4) & 0xF;
                    u8 filter    = (block_header >> 2) & 0x3;
                    u8 loop_flag = (block_header >> 0) & 0x1;
                    u8 end_flag  = (block_header >> 1) & 0x1;

                    if (end_flag) {
                        if (loop_flag) {
                            chan->pos = (sample_loop - sample_start) * 16;
                        } else {
                            continue;
                        }
                    }
                }

                u8 sample_byte = spc_R(block_pos + 1 + (sample_idx / 2));
                i8 sample_4bit = (sample_idx & 1) ? (sample_byte >> 4) & 0xF :
                                                    (sample_byte >> 0) & 0xF;
                if (sample_4bit > 7) sample_4bit -= 16;
                
                i32 dec_sample = sample_4bit << (block_header >> 4);
                u8  filter     = (block_header >> 2) & 0x3;
                switch (filter) {
                    case 0: // no-filter
                        break; 
                    case 1: // simple filter
                        dec_sample += chan->samples[0] * 15 / 16;
                        break;
                    case 2: // more complex
                        dec_sample += (chan->samples[0] * 61 / 32) - (chan->samples[1] * 15 / 16);
                        break;
                    case 3: // most complex
                        dec_sample += (chan->samples[0] * 115 / 64) - (chan->samples[1] * 13 / 16);
                        break;
                }

                if (dec_sample >  32767) dec_sample =  32767;
                if (dec_sample < -32768) dec_sample = -32768;

                chan->samples[3] = chan->samples[2];
                chan->samples[2] = chan->samples[1];
                chan->samples[1] = chan->samples[0];
                chan->samples[0] = dec_sample;
            }

            u8  frac   = chan->frac >> 4;
            i16 interp = gaussian_interpolate(frac, chan->samples);

            // adsr envelope process
            if (adsr1 & 0x80) {
                u8 attack_rate   =  (adsr1 >> 1) & 0xF;
                u8 decay_rate    = ((adsr1 & 0x7) << 1) | ((adsr2 >> 7) & 1);
                u8 sustain_level =  (adsr2 >> 5) & 0x7;
                u8 release_rate  =  (adsr2 >> 0) & 0x1;

                switch (chan->env_state) {
                    case 0: // attack
                        chan->envelope += attack_rate * 32;
                        if (chan->envelope >= 32767) {
                            chan->envelope  = 32767;
                            chan->env_state = 1; // move to decay
                        }
                        break;
                    case 1: // decay
                        chan->envelope -= decay_rate * 8;
                        if (chan->envelope <= (sustain_level * 4096))
                            chan->env_state = 2; // move to sustain
                        break;
                    case 2: // sustain
                        break;
                    case 3: // release
                        chan->envelope -= release_rate * 8;
                        if (chan->envelope < 0)
                            chan->envelope = 0;
                        break;
                }
            } else {
                chan->envelope = gain << 7; // direct gain (?)
            }

            // apply envelope and volume
            i32 final_sample = (interp * chan->envelope) >> 15;

            // apply channel volume
            float l = (final_sample * (i8)vol_l) / 32768.0f;
            float r = (final_sample * (i8)vol_r) / 32768.0f;
            
            // store for echo processing
            chan->output_l = l;
            chan->output_r = r;
        
            // apply channel volume and mix to stereo
            left  += l;
            right += r;
        }

        // master volume
        u8 mvol_l = spc_R(0xF0 + 0x0C);  // master volume left
        u8 mvol_r = spc_R(0xF0 + 0x1C);  // master volume right
        
        left  *= (i8)mvol_l / 128.0f;
        right *= (i8)mvol_r / 128.0f;

        echo_processing(spc, &left, &right);

        left  = clampf(left,  -1.0f, 1.0f);
        right = clampf(right, -1.0f, 1.0f);

        buffer[i * 2]     = left  * 16384; // left  output channel
        buffer[i * 2 + 1] = right * 16384; // right output channel
    }
    
    // Send to ALSA
    snd_pcm_writei(a->spc->pcm_handle, buffer, a->spc->buffer_size);
}




































































































static none asnes_spc_step(spc_state spc) {
    u8 opcode = spc_R(spc->pc++);
    
    switch (opcode) {
    // Data Movement Instructions
    case 0x8F: { // MOV dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        spc_W(dp, imm);
        break;
    }

    case 0xCD: { // MOV #imm, X  
        spc->x = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xBD: { // MOV SP, X
        spc->sp = spc->x;
        break;
    }

    case 0x9D: { // MOV X, SP
        spc->x = spc->sp;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0x7D: { // MOV A, X
        spc->a = spc->x;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x5D: { // MOV X, A
        spc->x = spc->a;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xDD: { // MOV A, Y
        spc->a = spc->y;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xFD: { // MOV Y, A
        spc->y = spc->a;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    // Memory to Register Loads
    case 0xE4: { // MOV A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF4: { // MOV A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE5: { // MOV A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF5: { // MOV A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF6: { // MOV A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE6: { // MOV A, (X)
        spc->a = spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xBF: { // MOV A, (X)+
        spc->a = spc_R(spc->x);
        spc->x++;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE7: { // MOV A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF7: { // MOV A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a = spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE8: { // MOV X, #imm
        spc->x = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xF8: { // MOV X, dp
        u8 dp = spc_R(spc->pc++);
        spc->x = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xF9: { // MOV X, dp+Y
        u8 dp = spc_R(spc->pc++);
        spc->x = spc_R((dp + spc->y) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xE9: { // MOV X, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->x = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0x8D: { // MOV Y, #imm (duplicate of 0xCD but for Y)
        spc->y = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xEB: { // MOV Y, dp
        u8 dp = spc_R(spc->pc++);
        spc->y = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xFB: { // MOV Y, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->y = spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xEC: { // MOV Y, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->y = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    // Register to Memory Stores
    case 0xC4: { // MOV dp, A
        u8 dp = spc_R(spc->pc++);
        spc_W(dp, spc->a);
        break;
    }

    case 0xD4: { // MOV dp+X, A
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->x) & 0xFF, spc->a);
        break;
    }

    case 0xC5: { // MOV abs, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->a);
        break;
    }

    case 0xD5: { // MOV abs+X, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W((addr + spc->x) & 0xFFFF, spc->a);
        break;
    }

    case 0xD6: { // MOV abs+Y, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W((addr + spc->y) & 0xFFFF, spc->a);
        break;
    }

    case 0xC6: { // MOV (X), A
        spc_W(spc->x, spc->a);
        break;
    }

    case 0xAF: { // MOV (X)+, A
        spc_W(spc->x, spc->a);
        spc->x++;
        break;
    }

    case 0xC7: { // MOV [dp+X], A
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc_W(addr, spc->a);
        break;
    }

    case 0xD7: { // MOV [dp]+Y, A
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc_W((addr + spc->y) & 0xFFFF, spc->a);
        break;
    }

    case 0xC9: { // MOV abs, X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->x);
        break;
    }

    case 0xD9: { // MOV dp+Y, X
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->y) & 0xFF, spc->x);
        break;
    }

    case 0xDB: { // MOV dp+X, Y
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->x) & 0xFF, spc->y);
        break;
    }

    case 0xCB: { // MOV dp, Y
        u8 dp = spc_R(spc->pc++);
        spc_W(dp, spc->y);
        break;
    }

    case 0xCC: { // MOV abs, Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->y);
        break;
    }

    // Memory to Memory
    case 0xFA: { // MOV dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 re = spc_R(src);
        spc_W(dst, re);
        break;
    }

    // Arithmetic Operations
    case 0x84: { // ADC A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x94: { // ADC A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x85: { // ADC A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x95: { // ADC A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x96: { // ADC A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x86: { // ADC A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x97: { // ADC A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x87: { // ADC A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x88: { // ADC A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x99: { // ADC (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val + y_val + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((x_val ^ result) & (y_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(spc->x, result & 0xFF);
        break;
    }

    case 0x89: { // ADC dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val + src_val + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((dst_val ^ result) & (src_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dst, result & 0xFF);
        break;
    }

    case 0x98: { // ADC dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val + imm + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((dp_val ^ result) & (imm ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dp, result & 0xFF);
        break;
    }

    // SBC operations (similar pattern but subtracting)
    case 0xA4: { // SBC A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    // SBC Operations (remaining)
    case 0xB4: { // SBC A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA5: { // SBC A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB5: { // SBC A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB6: { // SBC A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA6: { // SBC A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB7: { // SBC A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA7: { // SBC A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA8: { // SBC A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB9: { // SBC (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val - y_val - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((x_val ^ y_val) & (x_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(spc->x, result & 0xFF);
        break;
    }

    case 0xA9: { // SBC dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val - src_val - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((dst_val ^ src_val) & (dst_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dst, result & 0xFF);
        break;
    }

    case 0xB8: { // SBC dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val - imm - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((dp_val ^ imm) & (dp_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dp, result & 0xFF);
        break;
    }

    // Comparison Operations
    case 0x64: { // CMP A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x74: { // CMP A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x65: { // CMP A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x75: { // CMP A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x76: { // CMP A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x66: { // CMP A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x77: { // CMP A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x67: { // CMP A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x68: { // CMP A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x79: { // CMP (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val - y_val;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x69: { // CMP dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val - src_val;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x78: { // CMP dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val - imm;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0xC8: { // CMP X, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x3E: { // CMP X, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x1E: { // CMP X, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0xAD: { // CMP Y, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x7E: { // CMP Y, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x5E: { // CMP Y, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    // Logical Operations - AND
    case 0x24: { // AND A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a &= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x34: { // AND A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a &= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x25: { // AND A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x35: { // AND A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x36: { // AND A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x26: { // AND A, (X)
        spc->a &= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x37: { // AND A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a &= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x27: { // AND A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a &= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x28: { // AND A, #imm
        spc->a &= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x39: { // AND (X), (Y)
        u8 result = spc_R(spc->x) & spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x29: { // AND dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) & spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x38: { // AND dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) & imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Logical Operations - OR
    case 0x04: { // OR A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a |= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x14: { // OR A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a |= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x05: { // OR A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x15: { // OR A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x16: { // OR A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x06: { // OR A, (X)
        spc->a |= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x17: { // OR A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a |= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x07: { // OR A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a |= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x08: { // OR A, #imm
        spc->a |= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x19: { // OR (X), (Y)
        u8 result = spc_R(spc->x) | spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x09: { // OR dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) | spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x18: { // OR dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) | imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // EOR Operations
    case 0x44: { // EOR A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a ^= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x54: { // EOR A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a ^= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x45: { // EOR A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x55: { // EOR A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x56: { // EOR A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x46: { // EOR A, (X)
        spc->a ^= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x57: { // EOR A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a ^= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x47: { // EOR A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a ^= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x48: { // EOR A, #imm
        spc->a ^= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x59: { // EOR (X), (Y)
        u8 result = spc_R(spc->x) ^ spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x49: { // EOR dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) ^ spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x58: { // EOR dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) ^ imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Increment/Decrement
    case 0xBC: { // INC A
        spc->a++;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x3D: { // INC X
        spc->x++;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xFC: { // INC Y
        spc->y++;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xAB: { // INC dp
        u8 dp = spc_R(spc->pc++);
        u8 result = spc_R(dp) + 1;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0xBB: { // INC dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 result = spc_R(addr) + 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0xAC: { // INC abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 result = spc_R(addr) + 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x9C: { // DEC A
        spc->a--;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x1D: { // DEC X
        spc->x--;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xDC: { // DEC Y
        spc->y--;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0x8B: { // DEC dp
        u8 dp = spc_R(spc->pc++);
        u8 result = spc_R(dp) - 1;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x9B: { // DEC dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 result = spc_R(addr) - 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x8C: { // DEC abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 result = spc_R(addr) - 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Shift/Rotate Operations
    case 0x1C: { // ASL A
        u8 carry = (spc->a & 0x80) ? 0x01 : 0;
        spc->a <<= 1;
        spc->psw = (spc->psw & ~0x83) | carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x0B: { // ASL dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x1B: { // ASL dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x0C: { // ASL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x5C: { // LSR A
        u8 carry = spc->a & 0x01;
        spc->a >>= 1;
        spc->psw = (spc->psw & ~0x83) | carry | (spc->a == 0 ? 0x02 : 0);
        break;
    }

    case 0x4B: { // LSR dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x5B: { // LSR dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x4C: { // LSR abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x3C: { // ROL A
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (spc->a & 0x80) ? 0x01 : 0;
        spc->a = (spc->a << 1) | old_carry;
        spc->psw = (spc->psw & ~0x83) | new_carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x2B: { // ROL dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x3B: { // ROL dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x2C: { // ROL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x7C: { // ROR A
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = spc->a & 0x01;
        spc->a = (spc->a >> 1) | (old_carry << 7);
        spc->psw = (spc->psw & ~0x83) | new_carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x6B: { // ROR dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x7B: { // ROR dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x6C: { // ROR abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    // Branch Instructions
    case 0x10: { // BPL rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x80)) spc->pc += offset;
        break;
    }

    case 0x30: { // BMI rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x80) spc->pc += offset;
        break;
    }

    case 0x50: { // BVC rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x40)) spc->pc += offset;
        break;
    }

    case 0x70: { // BVS rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x40) spc->pc += offset;
        break;
    }

    case 0x90: { // BCC rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x01)) spc->pc += offset;
        break;
    }

    case 0xB0: { // BCS rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x01) spc->pc += offset;
        break;
    }

    case 0xD0: { // BNE rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x02)) spc->pc += offset;
        break;
    }

    case 0xF0: { // BEQ rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x02) spc->pc += offset;
        break;
    }

    case 0x2F: { // BRA rel
        i8 offset = (i8)spc_R(spc->pc++);
        spc->pc += offset;
        break;
    }

    // Bit Operations
    case 0x0A: { // OR1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw |= 0x01;
        }
        break;
    }

    case 0x2A: { // OR1 C, /mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (!(spc_R(addr) & (1 << bit))) {
            spc->psw |= 0x01;
        }
        break;
    }

    case 0x4A: { // AND1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (!(spc_R(addr) & (1 << bit))) {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0x6A: { // AND1 C, /mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0x8A: { // EOR1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw ^= 0x01;
        }
        break;
    }

    case 0xAA: { // MOV1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw |= 0x01;
        } else {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0xCA: { // MOV1 mem.bit, C
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        u8   r = spc_R(addr);
        if (spc->psw & 0x01) {
            spc_W(addr, r | (1 << bit));
        } else {
            spc_W(addr, r & ~(1 << bit));
        }
        break;
    }

    case 0xEA: { // NOT1 mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        u8   r = spc_R(addr);
        spc_W(addr, r ^ (1 << bit));
        break;
    }

    // Set/Clear Bits
    case 0x02: { // SET1 dp.0
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x01);
        break;
    }

    case 0x22: { // SET1 dp.1
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x02);
        break;
    }

    case 0x42: { // SET1 dp.2
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x04);
        break;
    }

    case 0x62: { // SET1 dp.3
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x08);
        break;
    }

    case 0x82: { // SET1 dp.4
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x10);
        break;
    }

    case 0xA2: { // SET1 dp.5
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x20);
        break;
    }

    case 0xC2: { // SET1 dp.6
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x40);
        break;
    }

    case 0xE2: { // SET1 dp.7
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x80);
        break;
    }

    case 0x12: { // CLR1 dp.0
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x01);
        break;
    }

    case 0x32: { // CLR1 dp.1
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x02);
        break;
    }

    case 0x52: { // CLR1 dp.2
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x04);
        break;
    }

    case 0x72: { // CLR1 dp.3
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x08);
        break;
    }

    case 0x92: { // CLR1 dp.4
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x10);
        break;
    }

    case 0xB2: { // CLR1 dp.5
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x20);
        break;
    }

    case 0xD2: { // CLR1 dp.6
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x40);
        break;
    }

    case 0xF2: { // CLR1 dp.7
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x80);
        break;
    }

    // Branch on Bit
    case 0x03: { // BBS dp.0, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x01) spc->pc += offset;
        break;
    }

    case 0x23: { // BBS dp.1, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x02) spc->pc += offset;
        break;
    }

    case 0x43: { // BBS dp.2, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x04) spc->pc += offset;
        break;
    }

    case 0x63: { // BBS dp.3, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x08) spc->pc += offset;
        break;
    }

    case 0x83: { // BBS dp.4, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x10) spc->pc += offset;
        break;
    }

    case 0xA3: { // BBS dp.5, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x20) spc->pc += offset;
        break;
    }

    case 0xC3: { // BBS dp.6, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x40) spc->pc += offset;
        break;
    }

    case 0xE3: { // BBS dp.7, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x80) spc->pc += offset;
        break;
    }

    case 0x13: { // BBC dp.0, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x01)) spc->pc += offset;
        break;
    }

    case 0x33: { // BBC dp.1, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x02)) spc->pc += offset;
        break;
    }

    case 0x53: { // BBC dp.2, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x04)) spc->pc += offset;
        break;
    }

    case 0x73: { // BBC dp.3, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x08)) spc->pc += offset;
        break;
    }

    case 0x93: { // BBC dp.4, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x10)) spc->pc += offset;
        break;
    }

    case 0xB3: { // BBC dp.5, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x20)) spc->pc += offset;
        break;
    }

    case 0xD3: { // BBC dp.6, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x40)) spc->pc += offset;
        break;
    }

    case 0xF3: { // BBC dp.7, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x80)) spc->pc += offset;
        break;
    }

    // Jump/Call Instructions
    case 0x5F: { // JMP abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->pc = addr;
        break;
    }

    case 0x1F: { // JMP [abs+X]
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        addr += spc->x;
        spc->pc = spc_R(addr) | (spc_R(addr + 1) << 8);
        break;
    }

    case 0x3F: { // CALL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x4F: { // PCALL up
        u8 page = spc_R(spc->pc++);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = 0xFF00 | page;
        break;
    }

    case 0x01: { // TCALL 0
        u16 addr = spc_R(0xFFDE) | (spc_R(0xFFDF) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x11: { // TCALL 1
        u16 addr = spc_R(0xFFDC) | (spc_R(0xFFDD) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x21: { // TCALL 2
        u16 addr = spc_R(0xFFDA) | (spc_R(0xFFDB) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x31: { // TCALL 3
        u16 addr = spc_R(0xFFD8) | (spc_R(0xFFD9) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x41: { // TCALL 4
        u16 addr = spc_R(0xFFD6) | (spc_R(0xFFD7) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x51: { // TCALL 5
        u16 addr = spc_R(0xFFD4) | (spc_R(0xFFD5) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x61: { // TCALL 6
        u16 addr = spc_R(0xFFD2) | (spc_R(0xFFD3) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x71: { // TCALL 7
        u16 addr = spc_R(0xFFD0) | (spc_R(0xFFD1) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x81: { // TCALL 8
        u16 addr = spc_R(0xFFCE) | (spc_R(0xFFCF) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x91: { // TCALL 9
        u16 addr = spc_R(0xFFCC) | (spc_R(0xFFCD) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xA1: { // TCALL 10
        u16 addr = spc_R(0xFFCA) | (spc_R(0xFFCB) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xB1: { // TCALL 11
        u16 addr = spc_R(0xFFC8) | (spc_R(0xFFC9) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xC1: { // TCALL 12
        u16 addr = spc_R(0xFFC6) | (spc_R(0xFFC7) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xD1: { // TCALL 13
        u16 addr = spc_R(0xFFC4) | (spc_R(0xFFC5) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xE1: { // TCALL 14
        u16 addr = spc_R(0xFFC2) | (spc_R(0xFFC3) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xF1: { // TCALL 15
        u16 addr = spc_R(0xFFC0) | (spc_R(0xFFC1) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x6F: { // RET
        u8 lo = spc_R(0x0100 + ++spc->sp);
        u8 hi = spc_R(0x0100 + ++spc->sp);
        spc->pc = lo | (hi << 8);
        break;
    }

    case 0x7F: { // RETI
        spc->psw = spc_R(0x0100 + ++spc->sp);
        u8 lo = spc_R(0x0100 + ++spc->sp);
        u8 hi = spc_R(0x0100 + ++spc->sp);
        spc->pc = lo | (hi << 8);
        break;
    }

    // Stack Operations
    case 0x2D: { // PUSH A
        spc_W(0x0100 + spc->sp--, spc->a);
        break;
    }

    case 0x4D: { // PUSH X
        spc_W(0x0100 + spc->sp--, spc->x);
        break;
    }

    case 0x6D: { // PUSH Y
        spc_W(0x0100 + spc->sp--, spc->y);
        break;
    }

    case 0x0D: { // PUSH PSW
        spc_W(0x0100 + spc->sp--, spc->psw);
        break;
    }

    case 0xAE: { // POP A
        spc->a = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0xCE: { // POP X
        spc->x = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0xEE: { // POP Y
        spc->y = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0x8E: { // POP PSW
        spc->psw = spc_R(0x0100 + ++spc->sp);
        break;
    }

    // Flag Operations
    case 0x60: { // CLRC
        spc->psw &= ~0x01;
        break;
    }

    case 0x80: { // SETC
        spc->psw |= 0x01;
        break;
    }

    case 0xE0: { // CLRV
        spc->psw &= ~0x40;
        break;
    }

    case 0x20: { // CLRP
        spc->psw &= ~0x20;
        break;
    }

    case 0x40: { // SETP
        spc->psw |= 0x20;
        break;
    }

    case 0xA0: { // EI
        spc->psw &= ~0x04;
        break;
    }

    case 0xC0: { // DI
        spc->psw |= 0x04;
        break;
    }

    // Special Operations
    case 0x9F: { // XCN A (already implemented above, but included here for completeness)
        spc->a = (spc->a << 4) | (spc->a >> 4);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xDF: { // DAA A
        if ((spc->a & 0x0F) > 9 || (spc->psw & 0x08)) {
            spc->a += 6;
        }
        if ((spc->a & 0xF0) > 0x90 || (spc->psw & 0x01)) {
            spc->a += 0x60;
            spc->psw |= 0x01;
        } else {
            spc->psw &= ~0x01;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xBE: { // DAS A
        if (!(spc->psw & 0x08) && (spc->a & 0x0F) > 9) {
            spc->a -= 6;
        }
        if (!(spc->psw & 0x01) && spc->a > 0x99) {
            spc->a -= 0x60;
            spc->psw &= ~0x01;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xED: { // NOTC
        spc->psw ^= 0x01;
        break;
    }

    case 0x9E: { // DIV YA, X
        u16 ya = spc->a | (spc->y << 8);
        if (spc->x == 0) {
            spc->psw |= 0x40; // Set overflow
            spc->a = 0xFF;
            spc->y = 0xFF;
        } else {
            spc->psw &= ~0x40;
            spc->a = ya / spc->x;
            spc->y = ya % spc->x;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xCF: { // MUL YA
        u16 result = spc->a * spc->y;
        spc->a = result & 0xFF;
        spc->y = (result >> 8) & 0xFF;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }


    case 0xDA: { // MOVW YA, dp
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R(dp);
        spc->y = spc_R(dp + 1);
        u16 ya = spc->a | (spc->y << 8);
        spc->psw = (spc->psw & ~0x82) | (ya == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }
    case 0xBA: { // MOVW dp, YA
        u8 dp = spc_R(spc->pc++);
        spc_W(dp,   spc->a);
        spc_W(dp+1, spc->y);
        break; 
    }

    // System
    case 0xFF: { // STOP
        // Halt the SPC700 processor
        // For emulation purposes, we can set a flag or just return
        print("SPC700 STOP instruction executed at PC: 0x%04X", spc->pc - 1);
        return; // Stop execution
    }

    case 0xEF: { // SLEEP
        // Put SPC700 to sleep until interrupt
        // For now, just continue (would normally wait for timer or other interrupt)
        break;
    }

    case 0x00: { // NOP
        // No operation
        break;
    }

    // Undefined/Invalid opcodes (handle as NOP or error)
    default:   // Handle undefined opcodes
        printf("unhandled spc opcode %02X at %04X\n", opcode, spc->pc - 1);
        break;
    }
}

















































































// SNES step function (more than 30 lines)
static i32 asnes_step(asnes a) {
    cpu_state cpu = a->cpu;
    spc_state spc = a->spc;
    u8* memory    = cpu->memory;
    u8  opcode    = cpu_R(cpu->pc++);
    i32  cycles   = 0;
    u16  old_pc   = cpu->pc;
    bool ualign   =  (cpu->dp & 0xFF)   != 0;
    bool zclear   = !(cpu->p  & 0x02);  // Z flag is clear
    #define cross    (base    & 0xFF00) != (addr & 0xFF00);
    #define cross_pc ((old_pc & 0xFF00) != (cpu->pc & 0xFF00));
    bool native   = !(cpu->p  & 0x100);
    bool is_16bit = !(cpu->p  & 0x20);

    print("asnes_step op: 0x%02X", opcode);

    switch (opcode) {
    case 0x1A: { // INC A (Increment Accumulator)
        cpu->a++;
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0x3A: { // DEC A (Decrement Accumulator)
        cpu->a--;
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0x9B: { // TXY (Transfer X to Y)
        cpu->y = cpu->x;
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 2;
        break;
    }
    
    case 0xBB: { // TYX (Transfer Y to X)
        cpu->x = cpu->y;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
    }

    // Load/Store Accumulator
    case 0xA5: { // LDA dp (direct page)
        u8 addr = cpu_R(cpu->pc++);
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 3 + ualign;
        break;
    }
    
    case 0xAD: { // LDA abs (absolute)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }
    
    case 0xBD: { // LDA abs,X (absolute indexed with X)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 base = (lo | (hi << 8));
        u16 addr = base + cpu->x;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }
    
    case 0xB9: { // LDA abs,Y (absolute indexed with Y)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 base = (lo | (hi << 8));
        u16 addr = base + cpu->y;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }
    
    case 0xA1: { // LDA (dp,X) (indexed indirect)
        u8 dp = cpu_R(cpu->pc++);
        u16 ptr = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }
    
    case 0xB1: { // LDA (dp),Y (indirect indexed)
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + cross + ualign;
        break;
    }

    // Store Accumulator
    case 0x85: { // STA dp (direct page)
        u8 addr = cpu_R(cpu->pc++);
        cpu_W(addr, cpu->a);
        cycles = 3 + ualign;
        break;
    }
    
    case 0x95: { // STA dp,X (direct page indexed with X)
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu_W(addr, cpu->a);
        cycles = 4 + ualign;
        break;
    }
    
    case 0x9D: { // STA abs,X (absolute indexed with X)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->x;
        cpu_W(addr, cpu->a);
        cycles = 5 + ualign;
        break;
    }
    
    case 0x99: { // STA abs,Y (absolute indexed with Y)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->y;
        cpu_W(addr, cpu->a);
        cycles = 5;
        break;
    }
    
    case 0x81: { // STA (dp,X) (indexed indirect)
        u8 dp = cpu_R(cpu->pc++);
        u16 ptr = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        cpu_W(addr, cpu->a);
        cycles = 6 + ualign;
        break;
    }
    
    case 0x91: { // STA (dp),Y (indirect indexed)
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u16 addr = base + cpu->y;
        cpu_W(addr, cpu->a);
        cycles = 6 + ualign;
        break;
    }

    // Load/Store X, Y
    case 0xA6: { // LDX dp (direct page)
        u8 addr = cpu_R(cpu->pc++);
        cpu->x = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 3 + ualign;
        break;
    }
    
    case 0xAE: { // LDX abs (absolute)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu->x = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 4;
        break;
    }
    
    case 0xB6: { // LDX dp,Y (direct page indexed with Y)
        u8 addr = (cpu_R(cpu->pc++) + cpu->y) & 0xFF;
        cpu->x = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 4 + ualign;
        break;
    }
    
    case 0xBE: { // LDX abs,Y (absolute indexed with Y)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 base = (lo | (hi << 8));
        u16 addr = base + cpu->y;
        cpu->x = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0xA0: { // LDY #imm8 (immediate)
        cpu->y = cpu_R(cpu->pc++);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 2;
        break;
    }
    
    case 0xA4: { // LDY dp (direct page)
        u8 addr = cpu_R(cpu->pc++);
        cpu->y = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 3;
        break;
    }
    
    case 0xAC: { // LDY abs (absolute)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu->y = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 4;
        break;
    }
    
    case 0xB4: { // LDY dp,X (direct page indexed with X)
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu->y = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 4 + ualign;
        break;
    }
    
    case 0xBC: { // LDY abs,X (absolute indexed with X)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 base = (lo | (hi << 8));
        u16 addr = base + cpu->x;
        cpu->y = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 4 + cross;
        break;
    }

    // Transfers
    case 0xAA: // TAX (Transfer A to X)
        cpu->x = cpu->a;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
        
    case 0xA8: // TAY (Transfer A to Y)
        cpu->y = cpu->a;
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 2;
        break;
        
    case 0x8A: // TXA (Transfer X to Au)
        cpu->a = cpu->x;
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
        
    case 0x98: // TYA (Transfer Y to Au)
        cpu->a = cpu->y;
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;

    // Stack Operations
    case 0x48: // PHA (Push Accumulator)
        cpu_W(0x0100 + cpu->sp--, cpu->a);
        cycles = 3;
        break;
        
    case 0x68: // PLA (Pull Accumulator)
        cpu->a = cpu_R(0x0100 + ++cpu->sp);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
        
    case 0x08: // PHP (Push Processor Status)
        cpu_W(0x0100 + cpu->sp--, cpu->p);
        cycles = 3;
        break;
        
    case 0x28: // PLP (Pull Processor Status)
        cpu->p = cpu_R(0x0100 + ++cpu->sp);
        cycles = 4;
        break;

    // Arithmetic
    case 0x69: { // ADC #imm (Add with Carry immediate)
        u8 operand = cpu_R(cpu->pc++);
        u16 result = cpu->a + operand + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                    (result > 0xFF ? 0x01 : 0) |  // Carry
                    ((result & 0xFF) == 0 ? 0x02 : 0) |  // Zero
                    (result & 0x80) |  // Negative
                    (((cpu->a ^ result) & (operand ^ result) & 0x80) ? 0x40 : 0);  // Overflow
        cpu->a = result & 0xFF;
        cycles = 2;
        break;
    }
    
    case 0x65: { // ADC dp (Add with Carry direct page)
        u8 addr = cpu_R(cpu->pc++);
        u8 operand = cpu_R(addr);
        u16 result = cpu->a + operand + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                    (result > 0xFF ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((cpu->a ^ result) & (operand ^ result) & 0x80) ? 0x40 : 0);
        cpu->a = result & 0xFF;
        cycles = 3 + ualign;
        break;
    }
    
    case 0x6D: { // ADC abs (Add with Carry absolute)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        u8 operand = cpu_R(addr);
        u16 result = cpu->a + operand + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                    (result > 0xFF ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((cpu->a ^ result) & (operand ^ result) & 0x80) ? 0x40 : 0);
        cpu->a = result & 0xFF;
        cycles = 4;
        break;
    }
    
    case 0xE9: { // SBC #imm (Subtract with Carry immediate)
        u8 operand = cpu_R(cpu->pc++);
        u16 result = cpu->a - operand - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                    (result < 0x100 ? 0x01 : 0) |  // Carry (inverted for SBC)
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((cpu->a ^ operand) & (cpu->a ^ result) & 0x80) ? 0x40 : 0);
        cpu->a = result & 0xFF;
        cycles = 2;
        break;
    }

    // Increment/Decrement
    case 0xE8: // INX (Increment X)
        cpu->x++;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
        
    case 0xCA: // DEX (Decrement X)
        cpu->x--;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
        
    case 0xC8: // INY (Increment Y)
        cpu->y++;
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 2;
        break;
        
    case 0x88: // DEY (Decrement Y)
        cpu->y--;
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 2;
        break;

    // Logical Operations
    case 0x29: { // AND #imm (AND with Accumulator immediate)
        cpu->a &= cpu_R(cpu->pc++);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }
    
    case 0x09: { // ORA #imm (OR with Accumulator immediate)
        cpu->a |= cpu_R(cpu->pc++);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }
    
    case 0x49: { // EOR #imm (Exclusive OR with Accumulator immediate)
        cpu->a ^= cpu_R(cpu->pc++);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    // Branch Instructions
    case 0xD0: { // BNE (Branch if Not Equal)
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (!(cpu->p & 0x02)) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0xF0: { // BEQ (Branch if Equal)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (cpu->p & 0x02) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0x10: { // BPL (Branch if Positive)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (!(cpu->p & 0x80)) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0x30: { // BMI (Branch if Minus)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (cpu->p & 0x80) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0x90: { // BCC (Branch if Carry Clear)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (!(cpu->p & 0x01)) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0xB0: { // BCS (Branch if Carry Set)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (cpu->p & 0x01) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0x50: { // BVC (Branch if Overflow Clear)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (!(cpu->p & 0x40)) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }
    
    case 0x70: { // BVS (Branch if Overflow Set)
        u16 old_pc = cpu->pc;
        i8 offset = (i8)cpu_R(cpu->pc++);
        if (cpu->p & 0x40) cpu->pc += offset;
        cycles = 2 + taken + cross_pc;
        break;
    }

    // Jump Instructions
    case 0x6C: { // JMP (abs) (Jump Indirect)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 ptr = lo | (hi << 8);
        cpu->pc = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        cycles = 5;
        break;
    }
    
    case 0x40: { // RTI (Return from Interrupt)
        cpu->p = cpu_R(0x0100 + ++cpu->sp);
        u8 lo = cpu_R(0x0100 + ++cpu->sp);
        u8 hi = cpu_R(0x0100 + ++cpu->sp);
        cpu->pc = lo | (hi << 8);
        cycles = 6;
        break;
    }

    // Flag Operations
    case 0x38: // SEC (Set Carry Flag)
        cpu->p |= 0x01;
        cycles = 2;
        break;
        
    case 0x58: // CLI (Clear Interrupt Disable)
        cpu->p &= ~0x04;
        cycles = 2;
        break;
        
    case 0xB8: // CLV (Clear Overflow Flag)
        cpu->p &= ~0x40;
        cycles = 2;
        break;

    // NOP
    case 0xEA: // NOP (No Operation)
        cycles = 2;
        break;

    // System Control
    case 0x00: // BRK (Break)
        cpu->pc += 2;
        cpu_W(0x0100 + --cpu->sp, (cpu->pc >> 8) & 0xFF);
        cpu_W(0x0100 + --cpu->sp, cpu->pc & 0xFF);
        cpu_W(0x0100 + --cpu->sp, cpu->p | 0x10);
        cpu->p |= 0x04;
        cpu->pc = cpu_R(0xFFFE) | (cpu_R(0xFFFF) << 8);
        cycles = 7 + native;
        break;
    
    // Already implemented instructions from your original code
    case 0x78: // SEI
        cpu->p |= 0x04;
        cycles = 2;
        break;

    case 0x18: // CLC
        cpu->p &= ~0x01;
        cycles = 2;
        break;

    case 0xA9: { // LDA #imm
        u8 imm = cpu_R(cpu->pc++);
        cpu->a = imm;
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0xA2: { // LDX #imm
        u8 imm = cpu_R(cpu->pc++);
        cpu->x = imm;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
    }

    case 0x8D: { // STA abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu_W(addr, cpu->a);
        cycles = 2;
        break;
    }

    case 0x8F: { // STA long
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = lo | (hi << 8) | (bank << 16);
        cpu_W(addr & 0xFFFFFF, cpu->a);
        cycles = 2;
        break;
    }

    case 0x8E: { // STX abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu_W(addr, cpu->x);
        cycles = 2;
        break;
    }

    case 0x4C: { // JMP abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        cpu->pc = lo | (hi << 8);
        cycles = 2;
        break;
    }

    case 0x20: { // JSR abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 ret = cpu->pc - 1;
        // JSR pushes PC-1 (last byte of JSR instruction)
        cpu_W(0x0100 + cpu->sp--, (ret >> 8) & 0xFF);
        cpu_W(0x0100 + cpu->sp--, ret & 0xFF);
        cpu->pc = lo | (hi << 8);
        cycles = 2;
        break;
    }
    case 0x60: { // RTS
        u8 lo = cpu_R(0x0100 + ++cpu->sp);
        u8 hi = cpu_R(0x0100 + ++cpu->sp);
        cpu->pc = (hi << 8) | lo;
        cpu->pc++;
        cycles = 2;
        break;
    }

    case 0x80: { // BRA (PC relative)
        i8 offset = (i8)cpu_R(cpu->pc++);
        cpu->pc += offset;
        cycles = 2 + cross_pc + ualign;
        break;
    }

    // Missing LDA addressing modes
    case 0xB5: { // LDA dp,X
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + ualign;
        break;
    }

    case 0xB2: { // LDA (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0xAF: { // LDA long (24-bit address)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = lo | (hi << 8) | (bank << 16);
        cpu->a = cpu_R(addr & 0xFFFFFF); // 24-bit address space
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }

    case 0xBF: { // LDA long,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = (lo | (hi << 8) | (bank << 16)) + cpu->x;
        cpu->a = cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }
    
    case 0xA3: { // LDA sr,S (stack relative)
        u8 offset = cpu_R(cpu->pc++);
        u16 addr = 0x0100 + cpu->sp + offset;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0xB3: { // LDA (sr,S),Y (stack relative indirect indexed)
        u8 offset = cpu_R(cpu->pc++);
        u16 ptr = 0x0100 + cpu->sp + offset;
        u16 base = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a = cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 7;
        break;
    }

    case 0xA7: { // LDA [dp] (direct page indirect long)
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        cpu->a = cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6;
        break;
    }

    case 0xB7: { // LDA [dp],Y (direct page indirect long indexed)
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        cpu->a = cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6;
        break;
    }

    // Missing STA addressing modes
    case 0x92: { // STA (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu_W(addr, cpu->a);
        cycles = 5 + ualign;
        break;
    }

    case 0x9F: { // STA long,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = (lo | (hi << 8) | (bank << 16)) + cpu->x;
        cpu_W(addr & 0xFFFFFF, cpu->a);
        cycles = 5;
        break;
    }

    case 0x83: { // STA sr,S
        u8 offset = cpu_R(cpu->pc++);
        u16 addr = 0x0100 + cpu->sp + offset;
        cpu_W(addr, cpu->a);
        cycles = 4;
        break;
    }

    case 0x93: { // STA (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 ptr = 0x0100 + cpu->sp + offset;
        u16 base = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u16 addr = base + cpu->y;
        cpu_W(addr, cpu->a);
        cycles = 7;
        break;
    }

    case 0x87: { // STA [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        cpu_W(addr & 0xFFFFFF, cpu->a);
        cycles = 6;
        break;
    }

    case 0x97: { // STA [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        cpu_W(addr & 0xFFFFFF, cpu->a);
        cycles = 6;
        break;
    }

    // Missing STX variants
    case 0x86: { // STX dp
        u8 addr = cpu_R(cpu->pc++);
        cpu_W(addr, cpu->x);
        cycles = 3;
        break;
    }

    case 0x96: { // STX dp,Y
        u8 addr = (cpu_R(cpu->pc++) + cpu->y) & 0xFF;
        cpu_W(addr, cpu->x);
        cycles = 4;
        break;
    }

    // Missing STY variants
    case 0x8C: { // STY abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu_W(addr, cpu->y);
        cycles = 4;
        break;
    }

    case 0x84: { // STY dp
        u8 addr = cpu_R(cpu->pc++);
        cpu_W(addr, cpu->y);
        cycles = 3 + ualign;
        break;
    }

    case 0x94: { // STY dp,X
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu_W(addr, cpu->y);
        cycles = 4 + ualign;
        break;
    }

    // Missing AND addressing modes
    case 0x25: { // AND dp
        u8 addr = cpu_R(cpu->pc++);
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 3 + ualign;
        break;
    }

    case 0x2D: { // AND abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0x35: { // AND dp,X
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + ualign;
        break;
    }

    case 0x3D: { // AND abs,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->x;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0x39: { // AND abs,Y
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->y;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0x21: { // AND (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u16 ptr = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x31: { // AND (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign + cross;
        break;
    }

    case 0x32: { // AND (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x2F: { // AND long
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = lo | (hi << 8) | (bank << 16);
        cpu->a &= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }

    case 0x3F: { // AND long,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = (lo | (hi << 8) | (bank << 16)) + cpu->x;
        cpu->a &= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }

    case 0x23: { // AND sr,S
        u8 offset = cpu_R(cpu->pc++);
        u16 addr = 0x0100 + cpu->sp + offset;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0x33: { // AND (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 ptr = 0x0100 + cpu->sp + offset;
        u16 base = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a &= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 7;
        break;
    }

    case 0x27: { // AND [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        cpu->a &= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x37: { // AND [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        cpu->a &= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    // Missing ORA addressing modes
    case 0x05: { // ORA dp
        u8 addr = cpu_R(cpu->pc++);
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 3 + ualign;
        break;
    }

    case 0x0D: { // ORA abs
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = lo | (hi << 8);
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0x15: { // ORA dp,X
        u8 addr = (cpu_R(cpu->pc++) + cpu->x) & 0xFF;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + ualign;
        break;
    }

    case 0x1D: { // ORA abs,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->x;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0x19: { // ORA abs,Y
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u16 addr = (lo | (hi << 8)) + cpu->y;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0x01: { // ORA (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u16 ptr = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x11: { // ORA (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign + cross;
        break;
    }

    case 0x12: { // ORA (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x0F: { // ORA long
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = lo | (hi << 8) | (bank << 16);
        cpu->a |= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }

    case 0x1F: { // ORA long,X
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        uint32_t addr = (lo | (hi << 8) | (bank << 16)) + cpu->x;
        cpu->a |= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }

    case 0x03: { // ORA sr,S
        u8 offset = cpu_R(cpu->pc++);
        u16 addr = 0x0100 + cpu->sp + offset;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0x13: { // ORA (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 ptr = 0x0100 + cpu->sp + offset;
        u16 base = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u16 addr = base + cpu->y;
        cpu->a |= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 7;
        break;
    }
    
    case 0x07: { // ORA [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        cpu->a |= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x17: { // ORA [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        cpu->a |= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }
    
    // EOR (Exclusive OR) addressing modes
    case 0x45: { // EOR dp
        u8 dp = cpu_R(cpu->pc++);
        cpu->a ^= cpu_R(dp);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 3 + ualign;
        break;
    }

    case 0x4D: { // EOR abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu->a ^= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }

    case 0x55: { // EOR dp,X
        u8 dp = cpu_R(cpu->pc++);
        cpu->a ^= cpu_R((dp + cpu->x) & 0xFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + ualign;
        break;
    }

    case 0x5D: { // EOR abs,X
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu->a ^= cpu_R(addr + cpu->x);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }
    
    case 0x59: { // EOR abs,Y
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu->a ^= cpu_R(addr + cpu->y);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0x41: { // EOR (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u8 addr_base = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(addr_base) | (cpu_R(addr_base + 1) << 8);
        cpu->a ^= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x51: { // EOR (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu->a ^= cpu_R(base + cpu->y);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign + cross;
        break;
    }
    case 0x52: { // EOR (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        cpu->a ^= cpu_R(addr);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x4F: { // EOR long
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        cpu->a ^= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }
    case 0x5F: { // EOR long,X
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        cpu->a ^= cpu_R((addr + cpu->x) & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 5;
        break;
    }
    case 0x43: { // EOR sr,S
        u8 offset = cpu_R(cpu->pc++);
        cpu->a ^= cpu_R(cpu->sp + offset);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 4;
        break;
    }
    case 0x53: { // EOR (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 base = cpu_R(cpu->sp + offset) | (cpu_R(cpu->sp + offset + 1) << 8);
        cpu->a ^= cpu_R(base + cpu->y);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 7;
        break;
    }
    case 0x47: { // EOR [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        cpu->a ^= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }
    case 0x57: { // EOR [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        cpu->a ^= cpu_R(addr & 0xFFFFFF);
        cpu->p = (cpu->p & ~0x82) | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 6 + ualign;
        break;
    }

    // ADC (Add with Carry) addressing modes
    case 0x75: { // ADC dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R((dp + cpu->x) & 0xFF);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + ualign;
        break;
    }
    case 0x7D: { // ADC abs,X
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->x);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + cross;
        break;
    }
    case 0x79: { // ADC abs,Y
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->y);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + cross;
        break;
    }
    case 0x61: { // ADC (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u8 addr_base = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(addr_base) | (cpu_R(addr_base + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }
    case 0x71: { // ADC (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u8 value = cpu_R(base + cpu->y);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5 + ualign + cross;
        break;
    }
    case 0x72: { // ADC (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5 + ualign;
        break;
    }
    case 0x6F: { // ADC long
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5;
        break;
    }
    case 0x7F: { // ADC long,X
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        u8 value = cpu_R((addr + cpu->x) & 0xFFFFFF);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5;
        break;
    }
    case 0x63: { // ADC sr,S
        u8 offset = cpu_R(cpu->pc++);
        u8 value = cpu_R(cpu->sp + offset);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4;
        break;
    }
    case 0x73: { // ADC (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 base = cpu_R(cpu->sp + offset) | (cpu_R(cpu->sp + offset + 1) << 8);
        u8 value = cpu_R(base + cpu->y);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 7;
        break;
    }
    case 0x67: { // ADC [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }
    case 0x77: { // ADC [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a + value + (cpu->p & 0x01);
        cpu->p = (cpu->p & ~0xC3) | 
                (result > 0xFF ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }

    // SBC (Subtract with Carry) addressing modes
    case 0xE5: { // SBC dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 3 + ualign;
        break;
    }
    case 0xED: { // SBC abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 3 + ualign;
        break;
    }
    case 0xF5: { // SBC dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R((dp + cpu->x) & 0xFF);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + ualign;
        break;
    }
    case 0xFD: { // SBC abs,X
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->x);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + cross;
        break;
    }
    case 0xF9: { // SBC abs,Y
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->y);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4 + cross;
        break;
    }
    case 0xE1: { // SBC (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u8 addr_base = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(addr_base) | (cpu_R(addr_base + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }
    case 0xF1: { // SBC (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u8 value = cpu_R(base + cpu->y);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5 + ualign + cross;
        break;
    }
    case 0xF2: { // SBC (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5 + ualign;
        break;
    }
    case 0xEF: { // SBC long
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 5;
        break;
    }
    case 0xE3: { // SBC sr,S
        u8 offset = cpu_R(cpu->pc++);
        u8 value = cpu_R(cpu->sp + offset);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 4;
        break;
    }
    case 0xF3: { // SBC (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 base = cpu_R(cpu->sp + offset) | (cpu_R(cpu->sp + offset + 1) << 8);
        u8 value = cpu_R(base + cpu->y);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 7;
        break;
    }
    case 0xE7: { // SBC [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }
    case 0xF7: { // SBC [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value - (1 - (cpu->p & 0x01));
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0) |
                (result & 0x80);
        cpu->a = result & 0xFF;
        cycles = 6 + ualign;
        break;
    }

    // Compare operations
    case 0xC9: { // CMP #imm
        u8 value = cpu_R(cpu->pc++);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 2;
        break;
    }
    case 0xC5: { // CMP dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 3 + ualign;
        break;
    }
    case 0xCD: { // CMP abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4;
        break;
    }
    case 0xD5: { // CMP dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R((dp + cpu->x) & 0xFF);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4 + ualign;
        break;
    }
    case 0xDD: { // CMP abs,X
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->x);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                (result == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4 + cross;
        break;
    }

    // CMP variants (Compare with Accumulator)
    case 0xD9: { // CMP abs,Y
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr + cpu->y);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4 + cross;
        break;
    }

    case 0xC1: { // CMP (dp,X)
        u8 dp = cpu_R(cpu->pc++);
        u16 ptr = (dp + cpu->x) & 0xFF;
        u16 addr = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0xD1: { // CMP (dp),Y
        u8 dp = cpu_R(cpu->pc++);
        u16 base = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u16 addr = base + cpu->y;
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 5 + ualign + cross;
        break;
    }

    case 0xD2: { // CMP (dp)
        u8 dp = cpu_R(cpu->pc++);
        u16 addr = cpu_R(dp) | (cpu_R(dp + 1) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0xCF: { // CMP long
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 5;
        break;
    }

    case 0xDF: { // CMP long,X
        uint32_t addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8) | (cpu_R(cpu->pc++) << 16);
        u8 value = cpu_R((addr + cpu->x) & 0xFFFFFF);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 5;
        break;
    }

    case 0xC3: { // CMP sr,S
        u8 offset = cpu_R(cpu->pc++);
        u16 addr = 0x0100 + cpu->sp + offset;
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 3;
        break;
    }

    case 0xD3: { // CMP (sr,S),Y
        u8 offset = cpu_R(cpu->pc++);
        u16 ptr = 0x0100 + cpu->sp + offset;
        u16 base = cpu_R(ptr) | (cpu_R(ptr + 1) << 8);
        u16 addr = base + cpu->y;
        u8 value = cpu_R(addr);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 7;
        break;
    }

    case 0xC7: { // CMP [dp]
        u8 dp = cpu_R(cpu->pc++);
        uint32_t addr = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0xD7: { // CMP [dp],Y
        u8 dp = cpu_R(cpu->pc++);
        uint32_t base = cpu_R(dp) | (cpu_R(dp + 1) << 8) | (cpu_R(dp + 2) << 16);
        uint32_t addr = base + cpu->y;
        u8 value = cpu_R(addr & 0xFFFFFF);
        u16 result = cpu->a - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 6 + ualign;
        break;
    }

    // CPX variants (Compare with X)
    case 0xE0: { // CPX #imm
        u8 value = cpu_R(cpu->pc++);
        u16 result = cpu->x - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 2;
        break;
    }

    case 0xE4: { // CPX dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u16 result = cpu->x - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 3 + ualign;
        break;
    }

    case 0xEC: { // CPX abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->x - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4;
        break;
    }

    // CPY variants (Compare with Y)
    case 0xC0: { // CPY #imm
        u8 value = cpu_R(cpu->pc++);
        u16 result = cpu->y - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 2;
        break;
    }

    case 0xC4: { // CPY dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u16 result = cpu->y - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 3 + ualign;
        break;
    }

    case 0xCC: { // CPY abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u16 result = cpu->y - value;
        cpu->p = (cpu->p & ~0x83) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80);
        cycles = 4;
        break;
    }

    // Shift and rotate operations
    case 0x0A: { // ASL A (Arithmetic Shift Left Accumulator)
        u8 carry = (cpu->a & 0x80) ? 0x01 : 0;
        cpu->a <<= 1;
        cpu->p = (cpu->p & ~0x83) | carry | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0x06: { // ASL dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x0E: { // ASL abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6;
        break;
    }

    case 0x16: { // ASL dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x1E: { // ASL abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 7;
        break;
    }

    case 0x4A: { // LSR A (Logical Shift Right Accumulator)
        u8 carry = cpu->a & 0x01;
        cpu->a >>= 1;
        cpu->p = (cpu->p & ~0x83) | carry | (cpu->a == 0 ? 0x02 : 0);
        cycles = 2;
        break;
    }

    case 0x46: { // LSR dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 carry = value & 0x01;
        value >>= 1;
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        cycles = 5 + ualign;
        break;
    }

    case 0x4E: { // LSR abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        cycles = 6;
        break;
    }

    case 0x56: { // LSR dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        cycles = 6 + ualign;
        break;
    }

    case 0x5E: { // LSR abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        cycles = 7;
        break;
    }

    case 0x2A: { // ROL A (Rotate Left Accumulator)
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = (cpu->a & 0x80) ? 0x01 : 0;
        cpu->a = (cpu->a << 1) | old_carry;
        cpu->p = (cpu->p & ~0x83) | new_carry | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0x26: { // ROL dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x2E: { // ROL abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6;
        break;
    }

    case 0x36: { // ROL dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x3E: { // ROL abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 7;
        break;
    }

    case 0x6A: { // ROR A (Rotate Right Accumulator)
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = cpu->a & 0x01;
        cpu->a = (cpu->a >> 1) | (old_carry << 7);
        cpu->p = (cpu->p & ~0x83) | new_carry | (cpu->a == 0 ? 0x02 : 0) | (cpu->a & 0x80);
        cycles = 2;
        break;
    }

    case 0x66: { // ROR dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0x6E: { // ROR abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6;
        break;
    }

    case 0x76: { // ROR dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0x7E: { // ROR abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr);
        u8 old_carry = cpu->p & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 7;
        break;
    }

    // Increment/Decrement memory
    case 0xE6: { // INC dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp) + 1;
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0xEE: { // INC abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr) + 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6;
        break;
    }

    case 0xF6: { // INC dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr) + 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0xFE: { // INC abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr) + 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 7;
        break;
    }

    case 0xC6: { // DEC dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp) - 1;
        cpu_W(dp, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 5 + ualign;
        break;
    }

    case 0xCE: { // DEC abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr) - 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6;
        break;
    }

    case 0xD6: { // DEC dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr) - 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 6 + ualign;
        break;
    }

    case 0xDE: { // DEC abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr) - 1;
        cpu_W(addr, value);
        cpu->p = (cpu->p & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
        cycles = 7;
        break;
    }

    // More stack operations
    case 0xDA: { // PHX (Push X)
        cpu_W(0x0100 + cpu->sp--, cpu->x);
        cycles = 3;
        break;
    }

    case 0xFA: { // PLX (Pull X)
        cpu->x = cpu_R(0x0100 + ++cpu->sp);
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 4;
        break;
    }

    case 0x5A: { // PHY (Push Y)
        cpu_W(0x0100 + cpu->sp--, cpu->y);
        cycles = 3;
        break;
    }

    case 0x7A: { // PLY (Pull Y)
        cpu->y = cpu_R(0x0100 + ++cpu->sp);
        cpu->p = (cpu->p & ~0x82) | (cpu->y == 0 ? 0x02 : 0) | (cpu->y & 0x80);
        cycles = 4;
        break;
    }

    // More flag operations
    case 0xD8: { // CLD (Clear Decimal Mode)
        cpu->p &= ~0x08;
        cycles = 2;
        break;
    }

    case 0xF8: { // SED (Set Decimal Mode)
        cpu->p |= 0x08;
        cycles = 2;
        break;
    }

    // Test operations
    case 0x24: { // BIT dp
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 result = cpu->a & value;
        cpu->p = (cpu->p & ~0xC2) | 
                (value & 0xC0) |  // Copy bits 7 and 6 from memory
                (result == 0 ? 0x02 : 0);
        cycles = 3 + ualign;
        break;
    }

    case 0x2C: { // BIT abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 result = cpu->a & value;
        cpu->p = (cpu->p & ~0xC2) | 
                (value & 0xC0) |  // Copy bits 7 and 6 from memory
                (result == 0 ? 0x02 : 0);
        cycles = 4;
        break;
    }

    case 0x34: { // BIT dp,X
        u8 dp = cpu_R(cpu->pc++);
        u8 addr = (dp + cpu->x) & 0xFF;
        u8 value = cpu_R(addr);
        u8 result = cpu->a & value;
        cpu->p = (cpu->p & ~0xC2) | 
                (value & 0xC0) |  // Copy bits 7 and 6 from memory
                (result == 0 ? 0x02 : 0);
        cycles = 4 + ualign;
        break;
    }

    case 0x3C: { // BIT abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        u8 value = cpu_R(addr);
        u8 result = cpu->a & value;
        cpu->p = (cpu->p & ~0xC2) | 
                (value & 0xC0) |  // Copy bits 7 and 6 from memory
                (result == 0 ? 0x02 : 0);
        cycles = 4 + cross;
        break;
    }

    case 0x89: { // BIT #imm
        u8 value = cpu_R(cpu->pc++);
        u8 result = cpu->a & value;
        cpu->p = (cpu->p & ~0x02) | (result == 0 ? 0x02 : 0);
        // Note: Immediate mode doesn't affect N and V flags
        cycles = 2;
        break;
    }

    // Transfer operations
    case 0x9A: { // TXS (Transfer X to Stack Pointer)
        cpu->sp = cpu->x;
        cycles = 2;
        break;
    }

    case 0xBA: { // TSX (Transfer Stack Pointer to X)
        cpu->x = cpu->sp;
        cpu->p = (cpu->p & ~0x82) | (cpu->x == 0 ? 0x02 : 0) | (cpu->x & 0x80);
        cycles = 2;
        break;
    }

    case 0x5B: { // TCD (Transfer C accumulator to Direct page register)
        // For now, we'll skip this as it requires 16-bit accumulator mode
        // and direct page register implementation
        cpu->pc++; // Skip for now
        cycles = 2;
        break;
    }

    case 0x7B: { // TDC (Transfer Direct page register to C accumulator)
        // Skip for now
        cpu->pc++; // Skip for now
        cycles = 2;
        break;
    }

    case 0x1B: { // TCS (Transfer C accumulator to Stack pointer)
        // Skip for now - requires 16-bit mode
        cycles = 2;
        break;
    }

    case 0x3B: { // TSC (Transfer Stack pointer to C accumulator)
        // Skip for now - requires 16-bit mode
        cycles = 2;
        break;
    }

    // 65816 specific opcodes
    case 0xEB: { // XBA (Exchange B and A - swap high/low bytes of Au)
        // In 8-bit mode, this would swap with hidden B register
        // For now, just swap with itself (no-op in 8-bit mode)
        cycles = 3;
        break;
    }

    case 0x42: { // WDM (Reserved for future expansion)
        // This is a 2-byte NOP
        cpu->pc++;
        cycles = 2;
        break;
    }

    case 0xDB: { // STP (Stop the Clock)
        // Halt the processor - for emulation, we'll just stop
        printf("STP instruction at %04X - processor halted\n", cpu->pc - 1);
        cycles = 3;
        break;
    }

    case 0xCB: { // WAI (Wait for Interrupt)
        // Wait for interrupt - for now, just continue
        cycles = 3;
        break;
    }

    case 0x44: { // MVP (Block Move Positive)
        u8 src_bank = cpu_R(cpu->pc++);
        u8 dest_bank = cpu_R(cpu->pc++);
        // This is complex and requires 16-bit index registers
        // Skip for now
        cycles = 7;
        break;
    }

    case 0x54: { // MVN (Block Move Negative)
        u8 src_bank = cpu_R(cpu->pc++);
        u8 dest_bank = cpu_R(cpu->pc++);
        // This is complex and requires 16-bit index registers
        // Skip for now
        cycles = 7;
        break;
    }

    case 0x8B: { // PHB (Push Data Bank Register)
        // For now, push 0 as we don't have bank register implemented
        cpu_W(0x0100 + cpu->sp--, 0);
        cycles = 3;
        break;
    }

    case 0xAB: { // PLB (Pull Data Bank Register)
        // For now, just pull and discard
        ++cpu->sp;
        cycles = 4;
        break;
    }

    case 0x0B: { // PHD (Push Direct Page Register)
        // Push 0 for now as we don't have DP register
        cpu_W(0x0100 + cpu->sp--, 0);
        cpu_W(0x0100 + cpu->sp--, 0);
        cycles = 4;
        break;
    }

    case 0x2B: { // PLD (Pull Direct Page Register)
        // Pull and discard for now
        cpu->sp += 2;
        cycles = 5;
        break;
    }

    case 0x4B: { // PHK (Push Program Bank Register)
        // Push 0 for now as we're not using banks
        cpu_W(0x0100 + cpu->sp--, 0);
        cycles = 3;
        break;
    }

    // Mode switching
    case 0xC2: { // REP (Reset Processor Status Bits)
        u8 mask = cpu_R(cpu->pc++);
        cpu->p &= ~mask;
        cycles = 3;
        break;
    }

    case 0xE2: { // SEP (Set Processor Status Bits)
        u8 mask = cpu_R(cpu->pc++);
        cpu->p |= mask;
        cycles = 3;
        break;
    }

    // More jump/branch variants
    case 0x82: { // BRL (Branch Long)
        i16 offset = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu->pc += offset;
        cycles = 4;
        break;
    }

    case 0x6B: { // RTL (Return from subroutine Long)
        u8 lo = cpu_R(0x0100 + ++cpu->sp);
        u8 hi = cpu_R(0x0100 + ++cpu->sp);
        u8 bank = cpu_R(0x0100 + ++cpu->sp);
        cpu->pc = (hi << 8) | lo;
        cpu->pc++;
        // Ignore bank for now
        break;
    }

    case 0x22: { // JSL (Jump to Subroutine Long)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        // Push return address (24-bit)
        uint32_t ret = cpu->pc - 1;
        cpu_W(0x0100 + cpu->sp--, 0); // Bank byte (always 0 for now)
        cpu_W(0x0100 + cpu->sp--, (ret >> 8) & 0xFF);
        cpu_W(0x0100 + cpu->sp--, ret & 0xFF);
        cpu->pc = lo | (hi << 8);
        // Ignore bank for now
        cycles = 8;
        break;
    }

    case 0x7C: { // JMP (abs,X)
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        addr += cpu->x;
        cpu->pc = cpu_R(addr) | (cpu_R(addr + 1) << 8);
        cycles = 6;
        break;
    }

    case 0xDC: { // JML [abs] (Jump Long Indirect)
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu->pc = cpu_R(addr) | (cpu_R(addr + 1) << 8);
        // Ignore bank byte at addr+2 for now
        cycles = 6;
        break;
    }

    case 0x5C: { // JML abs (Jump Long)
        u8 lo = cpu_R(cpu->pc++);
        u8 hi = cpu_R(cpu->pc++);
        u8 bank = cpu_R(cpu->pc++);
        cpu->pc = lo | (hi << 8);
        // Ignore bank for now
        cycles = 4;
        break;
    }

    case 0xFC: { // JSR (abs,X)
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        addr += cpu->x;
        u16 ret = cpu->pc - 1;
        cpu_W(0x0100 + cpu->sp--, (ret >> 8) & 0xFF);
        cpu_W(0x0100 + cpu->sp--, ret & 0xFF);
        cpu->pc = cpu_R(addr) | (cpu_R(addr + 1) << 8);
        cycles = 8;
        break;
    }

    // Misc missing opcodes
    case 0x02: { // COP (Coprocessor Enable)
        u8 signature = cpu_R(cpu->pc++);
        // Push PC and P, set interrupt flag
        cpu_W(0x0100 + cpu->sp--, (cpu->pc >> 8) & 0xFF);
        cpu_W(0x0100 + cpu->sp--, cpu->pc & 0xFF);
        cpu_W(0x0100 + cpu->sp--, cpu->p);
        cpu->p |= 0x04; // Set interrupt disable
        // Jump to COP vector at 0xFFF4
        cpu->pc = cpu_R(0xFFF4) | (cpu_R(0xFFF5) << 8);
        cycles = 7 + native;
        break;
    }

    case 0x62: { // PER (Push Effective Relative Address)
        u16 offset = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u16 addr = cpu->pc + offset;
        cpu_W(0x0100 + cpu->sp--, (addr >> 8) & 0xFF);
        cpu_W(0x0100 + cpu->sp--, addr & 0xFF);
        cycles = 6;
        break;
    }

    case 0x14: { // TRB dp (Test and Reset Bits)
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 result = cpu->a & value;
        cpu_W(dp, value & ~cpu->a);
        cpu->p = (cpu->p & ~0x02) | (result == 0 ? 0x02 : 0);
        cycles = 5 + ualign;
        break;
    }

    case 0x1C: { // TRB abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 result = cpu->a & value;
        cpu_W(addr, value & ~cpu->a);
        cpu->p = (cpu->p & ~0x02) | (result == 0 ? 0x02 : 0);
        cycles = 6;
        break;
    }

    case 0x04: { // TSB dp (Test and Set Bits)
        u8 dp = cpu_R(cpu->pc++);
        u8 value = cpu_R(dp);
        u8 result = cpu->a & value;
        cpu_W(dp, value | cpu->a);
        cpu->p = (cpu->p & ~0x02) | (result == 0 ? 0x02 : 0);
        cycles = 5 + ualign;
        break;
    }

    case 0x0C: { // TSB abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        u8 value = cpu_R(addr);
        u8 result = cpu->a & value;
        cpu_W(addr, value | cpu->a);
        cpu->p = (cpu->p & ~0x02) | (result == 0 ? 0x02 : 0);
        cycles = 6;
        break;
    }

    case 0x64: { // STZ dp (Store Zero)
        u8 dp = cpu_R(cpu->pc++);
        cpu_W(dp, 0);
        cycles = 3 + ualign;
        break;
    }

    case 0x74: { // STZ dp,X
        u8 dp = cpu_R(cpu->pc++);
        cpu_W((dp + cpu->x) & 0xFF, 0);
        cycles = 4 + ualign;
        break;
    }

    case 0x9C: { // STZ abs
        u16 addr = cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8);
        cpu_W(addr, 0);
        cycles = 4;
        break;
    }

    case 0x9E: { // STZ abs,X
        u16 addr = (cpu_R(cpu->pc++) | (cpu_R(cpu->pc++) << 8)) + cpu->x;
        cpu_W(addr, 0);
        cycles = 5;
        break;
    }

    case 0xFB: { // XCE (Exchange Carry with Emulation flag)
        // Switch between 6502 emulation mode and 65816 native mode
        uint8_t old_carry = cpu->p & 0x01;
        if (cpu->p & 0x100) {  // If we were in emulation mode
            cpu->p &= ~0x100;  // Switch to native mode
            cpu->p |= old_carry;
        } else {  // If we were in native mode  
            cpu->p |= 0x100;   // Switch to emulation mode
            cpu->p = (cpu->p & ~0x01) | old_carry;
        }
        cycles = 2;
        break;
    }

    case 0xFF: { // SBC long,X
        uint8_t  lo    = cpu_R(cpu->pc++);
        uint8_t  hi    = cpu_R(cpu->pc++); 
        uint8_t  bank  = cpu_R(cpu->pc++);
        uint32_t addr  = lo | (hi << 8) | (bank << 16);
        uint8_t  value = cpu_R((addr + cpu->x) & 0xFFFFFF);
        // SBC calculation with proper borrow
        uint16_t result = cpu->a - value - (1 - (cpu->p & 0x01));
        
        // Set flags properly
        cpu->p = (cpu->p & ~0xC3) | 
                (result < 0x100 ? 0x01 : 0) |                    // Carry (no borrow)
                ((result & 0xFF) == 0 ? 0x02 : 0) |              // Zero  
                (result & 0x80) |                                 // Negative
                (((cpu->a ^ value) & (cpu->a ^ result) & 0x80) ? 0x40 : 0); // Overflow
                
        cpu->a = result & 0xFF;
        break;
    }

    default:
        printf("unhandled cpu opcode %02X at %04X\n", opcode, cpu->pc - 1);
        break;
    }

    if (!(cpu->p & 0x20)) {
        printf("WARNING: In 16-bit A/M mode at PC=%04X after opcode %02X\n", cpu->pc, opcode);
    }
    if (!(cpu->p & 0x10)) {
        printf("WARNING: In 16-bit X/Y mode at PC=%04X after opcode %02X\n", cpu->pc, opcode);
    }
    return cycles;
}


define_class(asnes, app)