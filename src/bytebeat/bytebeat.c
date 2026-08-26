#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include "../libutilipc/utilipc.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_OK      "\033[1;32m"
#define COLOR_TAG     "\033[1;33m"
#define COLOR_VAL     "\033[1;36m"
#define COLOR_MUTED   "\033[0;90m"

#define PI 3.14159265358979323846

#pragma pack(push, 1)
typedef struct {
    char     riff[4];        // "RIFF"
    uint32_t chunk_size;     // 36 + data_size
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t subchunk1_size; // 16 para PCM
    uint16_t audio_format;   // 1 para PCM
    uint16_t num_channels;   // 1 (Mono) ou 2 (Stereo)
    uint32_t sample_rate;    // 8000, 22050, 44100 Hz
    uint32_t byte_rate;      // sample_rate * channels * (bits/8)
    uint16_t block_align;    // channels * (bits/8)
    uint16_t bits_per_sample;// 8 ou 16 bits
    char     data[4];        // "data"
    uint32_t data_size;      // total de bytes do payload
} WavHeader;
#pragma pack(pop)

// Presets de Áudio
typedef struct {
    int id;
    int is_16bit;
    int is_stereo;
    uint32_t default_rate;
    const char *name;
    const char *author;
    const char *formula_desc;
    void (*render)(uint32_t t, double *left, double *right);
} AudioPreset;

// --- PRESETS 8-BIT CLÁSSICOS (Bytebeat) ---
static void gen_viznut(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)(t * (((t >> 12) | (t >> 8)) & (63 & (t >> 4))));
    *l = *r = (double)b;
}

static void gen_symphony(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)(((t >> 6 | t | t >> (t >> 16)) * 10 + ((t >> 11) & 7)) & 0xFF);
    *l = *r = (double)b;
}

static void gen_cosmic(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)(((t * (t >> 5 | t >> 8)) >> (t >> 16)) & 0xFF);
    *l = *r = (double)b;
}

static void gen_techno(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)(((t * 5 & t >> 7) | (t * 3 & t >> 10)) & 0xFF);
    *l = *r = (double)b;
}

static void gen_sierra(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);
    *l = *r = (double)b;
}

static void gen_harmony(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)((t * (t ^ t + (t >> 15 | 1) ^ (t - 1280 ^ t) >> 10)) & 0xFF);
    *l = *r = (double)b;
}

static void gen_alien(uint32_t t, double *l, double *r) {
    uint8_t b = (uint8_t)((((t * (t >> 11 & t >> 8 & 123 & t >> 3)) + (t >> 7 & t >> 10))) & 0xFF);
    *l = *r = (double)b;
}

// --- PRESETS 16-BIT & FLOATBEAT (Trigonometria sin/cos / 44.1kHz Hi-Fi) ---

// Preset 8: Ambient Cyberpunk Pad (Estéreo Espacial)
static void gen_ambient_pad_16(uint32_t t, double *l, double *r) {
    double time_s = (double)t / 44100.0;
    double l_val = sin(time_s * 220.0 * 2.0 * PI) * cos(time_s * 0.5 * 2.0 * PI) * 0.5 +
                   sin(time_s * 440.0 * 2.0 * PI + sin(time_s * 2.0 * PI) * 4.0) * 0.4;
    double r_val = sin(time_s * 222.0 * 2.0 * PI) * cos(time_s * 0.52 * 2.0 * PI) * 0.5 +
                   sin(time_s * 444.0 * 2.0 * PI + cos(time_s * 2.1 * PI) * 4.0) * 0.4;
    *l = l_val; *r = r_val;
}

// Preset 9: Super Nintendo FM Arpeggiator (16-bit Estéreo)
static void gen_snes_fm_16(uint32_t t, double *l, double *r) {
    double time_s = (double)t / 44100.0;
    int step = (int)(time_s * 8.0) % 8;
    static const double notes[] = {220.0, 261.63, 329.63, 392.0, 440.0, 523.25, 659.25, 783.99};
    double f = notes[step];

    double fm_mod = sin(time_s * f * 2.0 * PI * 2.0) * 2.5;
    double wave = sin(time_s * f * 2.0 * PI + fm_mod) * 0.7;
    double bass = sin(time_s * (f / 2.0) * 2.0 * PI) * 0.3;

    *l = wave * 0.8 + bass * 0.5;
    *r = wave * 0.5 + bass * 0.8;
}

// Preset 10: Pulsar Alienígena (Modulação Cruzada de Senos)
static void gen_alien_pulsar_16(uint32_t t, double *l, double *r) {
    double time_s = (double)t / 44100.0;
    double carrier = sin(time_s * 330.0 * 2.0 * PI * (1.0 + 0.1 * sin(time_s * 4.0 * PI)));
    double tremolo = cos(time_s * 8.0 * PI);
    double sound = carrier * tremolo * 0.8;
    *l = sound; *r = sound;
}

// Preset 11: Retrowave Dream (Ondas Senoidais Limpas com Harmônicos)
static void gen_retrowave_16(uint32_t t, double *l, double *r) {
    double time_s = (double)t / 44100.0;
    int chord = (int)(time_s * 2.0) % 4;
    double root = (chord == 0) ? 130.81 : (chord == 1) ? 164.81 : (chord == 2) ? 174.61 : 196.0;

    double s1 = sin(time_s * root * 2.0 * PI);
    double s2 = sin(time_s * root * 1.5 * 2.0 * PI) * 0.5;
    double s3 = sin(time_s * root * 2.0 * 2.0 * PI) * 0.25;

    double lead = (s1 + s2 + s3) * 0.6;
    *l = lead * 0.9;
    *r = lead * 0.7;
}

static const AudioPreset presets[] = {
    // 8-bit Clássicos
    {1,  0, 0, 8000,  "Viznut Classic (8-bit)",     "Ville-Matias Heikkilä", "t * (((t>>12)|(t>>8))&(63&(t>>4)))",                       gen_viznut},
    {2,  0, 0, 8000,  "Bit Symphony / 42 (8-bit)",  "Rygan & Kragen",        "(t>>6|t|t>>(t>>16))*10+((t>>11)&7)",                        gen_symphony},
    {3,  0, 0, 8000,  "Lost in Space (8-bit)",      "Micro Chiptune",        "(t*(t>>5|t>>8))>>(t>>16)",                                  gen_cosmic},
    {4,  0, 0, 8000,  "Techno Rave Beat (8-bit)",   "Demoscene 8-Bit",       "(t*5&t>>7)|(t*3&t>>10)",                                    gen_techno},
    {5,  0, 0, 8000,  "Sierra Arpeggiator (8-bit)", "Experimental Wave",     "((t*(t>>8|t>>9)&46&t>>8))^(t&t>>13|t>>6)",                  gen_sierra},
    {6,  0, 0, 8000,  "Complex Harmony (8-bit)",    "Algorithmic Chiptune",  "t*(t^t+(t>>15|1)^(t-1280^t)>>10)",                          gen_harmony},
    {7,  0, 0, 8000,  "Alien Organ (8-bit)",        "8-Bit Synth Engine",    "((t*(t>>11&t>>8&123&t>>3))+(t>>7&t>>10))",                  gen_alien},

    // 16-bit Hi-Fi Trigonometric (sin / cos / FM Synthesis)
    {8,  1, 1, 44100, "Ambient Cyber Pad (16-bit)", "Trigonometric Floatbeat","sin(220*t)*cos(0.5*t) + sin(440*t + sin(2*t)*4) [Stereo]", gen_ambient_pad_16},
    {9,  1, 1, 44100, "SNES FM Synth (16-bit)",    "16-bit Super Nintendo", "FM Modulation: sin(f*t + sin(2f*t)*2.5) [44.1 kHz Stereo]", gen_snes_fm_16},
    {10, 1, 0, 44100, "Alien Pulsar (16-bit)",      "Harmonic Tremolo",      "sin(330*t * (1 + 0.1*sin(4*t))) * cos(8*t)",                gen_alien_pulsar_16},
    {11, 1, 1, 44100, "Retrowave Dream (16-bit)",   "Hi-Fi Chord Synth",     "Tri-Harmonic Sine Chords (C3, E3, F3, G3) [Stereo]",        gen_retrowave_16}
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static void print_help(void) {
    printf("%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ bytebeat 2.0 - 8-Bit Chiptune & 16-Bit Hi-Fi Trigonometric Audio Synthesizer ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("Usage:\n");
    printf("  bytebeat [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -p, --preset <1-11>    Select audio preset (1-7: 8-bit, 8-11: 16-bit Hi-Fi) [Default: 9]\n");
    printf("  -d, --duration <SECS>  Duration in seconds [Default: 10s]\n");
    printf("  -r, --rate <HERTZ>     Sample rate (8000, 22050, 44100) [Default: por preset]\n");
    printf("  -o <ARQUIVO.wav>       Output WAV filename [Default: musica.wav]\n");
    printf("  --play                 Generate and auto-play in background\n");
    printf("  --list                 List all available equations (8-bit and 16-bit sin/cos)\n");
    printf("  --help                 Display this formatted help guide\n\n");
    printf("Exemplos:\n");
    printf("  bytebeat -p 9 --play                     (Toca o sintetizador SNES 16-bit)\n");
    printf("  bytebeat -p 8 -d 15 -o ambient.wav       (Gera Pad Estéreo espacial de 15s)\n");
    printf("  bytebeat -p 2                            (Gera clássico 8-bit Bit Symphony)\n");
    printf("%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
}

static void list_presets(void) {
    printf("\n%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ Banco de Músicas Matemáticas: 8-bit Chiptune & 16-bit Hi-Fi (sin / cos) ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s=================================================================================%s\n\n", COLOR_TITLE, COLOR_RESET);

    printf("  \033[1;35m--- 16-BIT HI-FI TRIGONOMETRIC SYNTHESIZERS (sin / cos / 44.1 kHz) ---\033[0m\n");
    for (size_t i = 7; i < PRESET_COUNT; i++) {
        printf("  %s[%2d]%s \033[1;36m%-30s\033[0m \033[1;32m[16-bit %s]\033[0m (%s)\n",
               COLOR_TAG, presets[i].id, COLOR_RESET, presets[i].name, presets[i].is_stereo ? "Stereo" : "Mono", presets[i].author);
        printf("       \033[0;90mFórmula:\033[0m \033[1;33m%s\033[0m\n\n", presets[i].formula_desc);
    }

    printf("  \033[1;35m--- 8-BIT RETRO CHIPTUNE & BYTEBEAT CLÁSSICO (8000 Hz) ---\033[0m\n");
    for (size_t i = 0; i < 7; i++) {
        printf("  %s[%2d]%s \033[1;36m%-30s\033[0m \033[0;90m[8-bit Mono]\033[0m (%s)\n",
               COLOR_TAG, presets[i].id, COLOR_RESET, presets[i].name, presets[i].author);
        printf("       \033[0;90mFórmula:\033[0m \033[1;33m%s\033[0m\n\n", presets[i].formula_desc);
    }
}

// Osciloscópio ASCII no Terminal
static void draw_waveform_16(const int16_t *samples, size_t count, int is_stereo) {
    static const char *bars[] = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    printf("  %sOsciloscópio ASCII da Forma de Onda (16-Bit Hi-Fi):%s\n  \033[1;32m", COLOR_TAG, COLOR_RESET);

    size_t step = count / 70;
    if (step == 0) step = 1;

    for (size_t i = 0; i < 70 && (i * step) < count; i++) {
        int16_t val = samples[i * step * (is_stereo ? 2 : 1)];
        // Converte -32768..32767 para 0..8
        int idx = (int)(((val + 32768) * 8) / 65535);
        if (idx < 0) idx = 0;
        if (idx > 8) idx = 8;
        printf("%s", bars[idx]);
    }
    printf("\033[0m\n\n");
}

int main(int argc, char *argv[]) {
    utilipc_init();

    int preset_idx = 8; // Padrão: Preset 9 (SNES FM 16-bit Hi-Fi!)
    int duration_sec = 10;
    int force_rate = 0;
    uint32_t sample_rate = 44100;
    const char *out_filename = "musica.wav";
    int auto_play = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            utilipc_close();
            return 0;
        }
        if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            list_presets();
            utilipc_close();
            return 0;
        }

        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--preset") == 0) && i + 1 < argc) {
            int p = atoi(argv[++i]);
            if (p >= 1 && p <= (int)PRESET_COUNT) preset_idx = p - 1;
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--duration") == 0) && i + 1 < argc) {
            duration_sec = atoi(argv[++i]);
            if (duration_sec < 1) duration_sec = 1;
            if (duration_sec > 120) duration_sec = 120;
        } else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) && i + 1 < argc) {
            sample_rate = (uint32_t)atoi(argv[++i]);
            force_rate = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_filename = argv[++i];
        } else if (strcmp(argv[i], "--play") == 0) {
            auto_play = 1;
        }
    }

    const AudioPreset *pr = &presets[preset_idx];
    if (!force_rate) sample_rate = pr->default_rate;

    int is_16 = pr->is_16bit;
    int is_stereo = pr->is_stereo;
    int channels = is_stereo ? 2 : 1;
    size_t total_frames = (size_t)duration_sec * sample_rate;
    size_t bytes_per_sample = is_16 ? 2 : 1;
    size_t total_bytes = total_frames * channels * bytes_per_sample;

    printf("\n%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ bytebeat 2.0 - Sintetizador Matemático %s ]%s\n", COLOR_TITLE, is_16 ? "16-Bit Hi-Fi (sin/cos)" : "8-Bit Chiptune", COLOR_RESET);
    printf("%s=================================================================================%s\n\n", COLOR_TITLE, COLOR_RESET);
    printf("  • %sPreset / Música  :%s %s[%d] %s%s (Autor: %s)\n", COLOR_TAG, COLOR_RESET, COLOR_VAL, pr->id, pr->name, COLOR_RESET, pr->author);
    printf("  • %sEquação          :%s \033[1;33m%s\033[0m\n", COLOR_TAG, COLOR_RESET, pr->formula_desc);
    printf("  • %sFormato de Áudio :%s %s%d-bit %s @ %u Hz%s (%d s | %zu bytes)\n",
           COLOR_TAG, COLOR_RESET, COLOR_OK, is_16 ? 16 : 8, is_stereo ? "Estéreo (2 Canais)" : "Mono", sample_rate, COLOR_RESET,
           duration_sec, total_bytes);
    printf("  • %sArquivo Gerado   :%s \033[1;36m%s\033[0m\n\n", COLOR_TAG, COLOR_RESET, out_filename);

    uint8_t *raw_buf_8 = NULL;
    int16_t *raw_buf_16 = NULL;

    if (is_16) {
        raw_buf_16 = malloc(total_frames * channels * sizeof(int16_t));
        if (!raw_buf_16) { fprintf(stderr, "Erro de alocacao de memoria\n"); return 1; }

        for (uint32_t t = 0; t < total_frames; t++) {
            double l_val = 0.0, r_val = 0.0;
            pr->render(t, &l_val, &r_val);

            // Clamp [-1.0, 1.0] e escala para int16 (-32767..32767)
            if (l_val > 1.0) l_val = 1.0; if (l_val < -1.0) l_val = -1.0;
            if (r_val > 1.0) r_val = 1.0; if (r_val < -1.0) r_val = -1.0;

            if (is_stereo) {
                raw_buf_16[t * 2 + 0] = (int16_t)(l_val * 32760.0);
                raw_buf_16[t * 2 + 1] = (int16_t)(r_val * 32760.0);
            } else {
                raw_buf_16[t] = (int16_t)(l_val * 32760.0);
            }
        }
        draw_waveform_16(raw_buf_16, total_frames, is_stereo);
    } else {
        raw_buf_8 = malloc(total_frames);
        if (!raw_buf_8) { fprintf(stderr, "Erro de alocacao de memoria\n"); return 1; }

        for (uint32_t t = 0; t < total_frames; t++) {
            double l_val = 0.0, r_val = 0.0;
            pr->render(t, &l_val, &r_val);
            raw_buf_8[t] = (uint8_t)l_val;
        }
    }

    // Monta o cabeçalho WAVE oficial
    WavHeader hdr;
    memcpy(hdr.riff, "RIFF", 4);
    hdr.chunk_size = sizeof(WavHeader) - 8 + total_bytes;
    memcpy(hdr.wave, "WAVE", 4);
    memcpy(hdr.fmt, "fmt ", 4);
    hdr.subchunk1_size = 16;
    hdr.audio_format = 1; // PCM linear
    hdr.num_channels = channels;
    hdr.sample_rate = sample_rate;
    hdr.bits_per_sample = is_16 ? 16 : 8;
    hdr.byte_rate = sample_rate * channels * (hdr.bits_per_sample / 8);
    hdr.block_align = channels * (hdr.bits_per_sample / 8);
    memcpy(hdr.data, "data", 4);
    hdr.data_size = total_bytes;

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) {
        fprintf(stderr, "bytebeat: falha ao gravar '%s': %s\n", out_filename, strerror(errno));
        if (raw_buf_16) free(raw_buf_16);
        if (raw_buf_8) free(raw_buf_8);
        utilipc_close();
        return 1;
    }

    fwrite(&hdr, 1, sizeof(WavHeader), fp);
    if (is_16) {
        fwrite(raw_buf_16, sizeof(int16_t), total_frames * channels, fp);
        free(raw_buf_16);
    } else {
        fwrite(raw_buf_8, 1, total_frames, fp);
        free(raw_buf_8);
    }
    fclose(fp);

    printf("  \033[1;32m✔ Arquivo WAV gerado com sucesso em: %s\033[0m\n", out_filename);

    if (auto_play) {
        char play_cmd[256];
        if (system("which termux-media-player >/dev/null 2>&1") == 0) {
            snprintf(play_cmd, sizeof(play_cmd), "termux-media-player play %s >/dev/null 2>&1 &", out_filename);
            printf("  \033[1;36m[Tocando via Termux Media Player...]\033[0m\n");
            (void)!system(play_cmd);
        } else if (system("which aplay >/dev/null 2>&1") == 0) {
            snprintf(play_cmd, sizeof(play_cmd), "aplay %s >/dev/null 2>&1 &", out_filename);
            printf("  \033[1;36m[Tocando via ALSA aplay...]\033[0m\n");
            (void)!system(play_cmd);
        } else if (system("which play >/dev/null 2>&1") == 0) {
            snprintf(play_cmd, sizeof(play_cmd), "play %s >/dev/null 2>&1 &", out_filename);
            printf("  \033[1;36m[Tocando via SoX play...]\033[0m\n");
            (void)!system(play_cmd);
        }
    }

    printf("%s=================================================================================%s\n\n", COLOR_TITLE, COLOR_RESET);

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "bytebeat: generated %s (Preset %d - %s %d-bit)", out_filename, pr->id, pr->name, is_16 ? 16 : 8);
    utilipc_write_status(-1, -1, -1, log_msg);

    utilipc_close();
    return 0;
}
