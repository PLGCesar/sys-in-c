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

#pragma pack(push, 1)
typedef struct {
    char     riff[4];        // "RIFF"
    uint32_t chunk_size;     // 36 + data_size
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t subchunk1_size; // 16 para PCM
    uint16_t audio_format;   // 1 para PCM
    uint16_t num_channels;   // 1 (Mono)
    uint32_t sample_rate;    // 8000 Hz
    uint32_t byte_rate;      // sample_rate * channels * bits/8
    uint16_t block_align;    // channels * bits/8
    uint16_t bits_per_sample;// 8 bits
    char     data[4];        // "data"
    uint32_t data_size;      // total de samples
} WavHeader;
#pragma pack(pop)

// Presets Clássicos de Bytebeat
typedef struct {
    int id;
    const char *name;
    const char *author;
    const char *formula_desc;
    uint8_t (*func)(uint32_t t);
} BytebeatPreset;

static uint8_t song_viznut(uint32_t t) {
    return (uint8_t)(t * (((t >> 12) | (t >> 8)) & (63 & (t >> 4))));
}

static uint8_t song_symphony(uint32_t t) {
    return (uint8_t)(((t >> 6 | t | t >> (t >> 16)) * 10 + ((t >> 11) & 7)) & 0xFF);
}

static uint8_t song_cosmic(uint32_t t) {
    return (uint8_t)(((t * (t >> 5 | t >> 8)) >> (t >> 16)) & 0xFF);
}

static uint8_t song_techno(uint32_t t) {
    return (uint8_t)(((t * 5 & t >> 7) | (t * 3 & t >> 10)) & 0xFF);
}

static uint8_t song_sierra(uint32_t t) {
    return (uint8_t)((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);
}

static uint8_t song_harmony(uint32_t t) {
    return (uint8_t)((t * (t ^ t + (t >> 15 | 1) ^ (t - 1280 ^ t) >> 10)) & 0xFF);
}

static uint8_t song_alien(uint32_t t) {
    return (uint8_t)((((t * (t >> 11 & t >> 8 & 123 & t >> 3)) + (t >> 7 & t >> 10))) & 0xFF);
}

static const BytebeatPreset presets[] = {
    {1, "Viznut Classic",     "Ville-Matias Heikkilä", "t * (((t>>12)|(t>>8))&(63&(t>>4)))",               song_viznut},
    {2, "Bit Symphony / 42",  "Rygan & Kragen",        "(t>>6|t|t>>(t>>16))*10+((t>>11)&7)",                song_symphony},
    {3, "Lost in Space",      "Micro Chiptune",        "(t*(t>>5|t>>8))>>(t>>16)",                          song_cosmic},
    {4, "Techno Rave Beat",   "Demoscene 8-Bit",       "(t*5&t>>7)|(t*3&t>>10)",                            song_techno},
    {5, "Sierra Arpeggiator", "Experimental Wave",     "((t*(t>>8|t>>9)&46&t>>8))^(t&t>>13|t>>6)",          song_sierra},
    {6, "Complex Harmony",    "Algorithmic Chiptune",  "t*(t^t+(t>>15|1)^(t-1280^t)>>10)",                  song_harmony},
    {7, "Alien Organ",        "8-Bit Synth Engine",    "((t*(t>>11&t>>8&123&t>>3))+(t>>7&t>>10))",          song_alien}
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static void print_help(void) {
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ bytebeat - 8-Bit Algorithmic Music & .WAV Synthesizer ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("Usage:\n");
    printf("  bytebeat [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -p, --preset <1-7>     Select musical formula preset [Default: 1]\n");
    printf("  -d, --duration <SECS>  Duration in seconds [Default: 12s]\n");
    printf("  -r, --rate <HERTZ>     Sample rate (8000, 11025, 16000, 44100) [Default: 8000]\n");
    printf("  -o <ARQUIVO.wav>       Output WAV filename [Default: musica.wav]\n");
    printf("  --play                 Generate and auto-play in background\n");
    printf("  --list                 List all available mathematical formulas\n");
    printf("  --help                 Display this formatted help guide\n\n");
    printf("Exemplos:\n");
    printf("  bytebeat -p 2 -d 15 -o sinfonia.wav\n");
    printf("  bytebeat -p 4 --play\n");
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
}

static void list_presets(void) {
    printf("\n%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ Músicas Matemáticas Disponíveis no Bytebeat ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        printf("  %s[%d]%s \033[1;36m%-24s\033[0m (%s)\n",
               COLOR_TAG, presets[i].id, COLOR_RESET, presets[i].name, presets[i].author);
        printf("      \033[0;90mEquação:\033[0m \033[1;33m%s\033[0m\n\n", presets[i].formula_desc);
    }
}

// Desenha o Osciloscópio / Forma de Onda no Terminal
static void draw_waveform_preview(const uint8_t *samples, size_t total_samples) {
    static const char *bars[] = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    printf("  %sOsciloscópio ASCII da Forma de Onda (Primeiros Ciclos):%s\n  \033[1;32m", COLOR_TAG, COLOR_RESET);

    size_t step = total_samples / 70;
    if (step == 0) step = 1;

    for (size_t i = 0; i < 70 && (i * step) < total_samples; i++) {
        uint8_t val = samples[i * step];
        int idx = (val * 8) / 255;
        if (idx < 0) idx = 0;
        if (idx > 8) idx = 8;
        printf("%s", bars[idx]);
    }
    printf("\033[0m\n\n");
}

int main(int argc, char *argv[]) {
    utilipc_init();

    int preset_idx = 0;
    int duration_sec = 12;
    uint32_t sample_rate = 8000;
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
            if (sample_rate < 4000) sample_rate = 8000;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_filename = argv[++i];
        } else if (strcmp(argv[i], "--play") == 0) {
            auto_play = 1;
        }
    }

    size_t total_samples = (size_t)duration_sec * sample_rate;
    uint8_t *audio_buffer = malloc(total_samples);
    if (!audio_buffer) {
        fprintf(stderr, "bytebeat: erro de memoria ao alocar buffer de audio\n");
        utilipc_close();
        return 1;
    }

    const BytebeatPreset *pr = &presets[preset_idx];

    printf("\n%s=================================================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ bytebeat - Sintetizador de Áudio Algorítmico 8-Bit ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s=================================================================================%s\n\n", COLOR_TITLE, COLOR_RESET);
    printf("  • %sMúsica / Preset  :%s %s[%d] %s%s (Autor: %s)\n", COLOR_TAG, COLOR_RESET, COLOR_VAL, pr->id, pr->name, COLOR_RESET, pr->author);
    printf("  • %sEquação Matemática:%s \033[1;33m%s\033[0m\n", COLOR_TAG, COLOR_RESET, pr->formula_desc);
    printf("  • %sConfiguração     :%s %d segundos @ %u Hz (8-bit Mono PCM | Total: %zu bytes)\n",
           COLOR_TAG, COLOR_RESET, duration_sec, sample_rate, total_samples);
    printf("  • %sArquivo de Saída :%s \033[1;36m%s\033[0m\n\n", COLOR_TAG, COLOR_RESET, out_filename);

    // 1. Gera o sinal de áudio pela equação matemática
    for (uint32_t t = 0; t < total_samples; t++) {
        audio_buffer[t] = pr->func(t);
    }

    // 2. Desenha o osciloscópio ASCII no terminal
    draw_waveform_preview(audio_buffer, total_samples);

    // 3. Monta o cabeçalho WAVE (.wav) canônico de 44 bytes
    WavHeader hdr;
    memcpy(hdr.riff, "RIFF", 4);
    hdr.chunk_size = sizeof(WavHeader) - 8 + total_samples;
    memcpy(hdr.wave, "WAVE", 4);
    memcpy(hdr.fmt, "fmt ", 4);
    hdr.subchunk1_size = 16;
    hdr.audio_format = 1; // PCM
    hdr.num_channels = 1; // Mono
    hdr.sample_rate = sample_rate;
    hdr.bits_per_sample = 8;
    hdr.byte_rate = sample_rate * 1 * (8 / 8);
    hdr.block_align = 1 * (8 / 8);
    memcpy(hdr.data, "data", 4);
    hdr.data_size = total_samples;

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) {
        fprintf(stderr, "bytebeat: falha ao criar arquivo '%s': %s\n", out_filename, strerror(errno));
        free(audio_buffer);
        utilipc_close();
        return 1;
    }

    fwrite(&hdr, 1, sizeof(WavHeader), fp);
    fwrite(audio_buffer, 1, total_samples, fp);
    fclose(fp);
    free(audio_buffer);

    printf("  \033[1;32m✔ Arquivo de áudio WAV gerado com sucesso em: %s\033[0m\n", out_filename);

    // 4. Reprodução automática caso solicitada
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
    snprintf(log_msg, sizeof(log_msg), "bytebeat: generated %s (Preset %d - %ds)", out_filename, pr->id, duration_sec);
    utilipc_write_status(-1, -1, -1, log_msg);

    utilipc_close();
    return 0;
}
