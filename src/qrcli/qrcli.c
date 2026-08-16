#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "../libutilipc/utilipc.h"

#define MAX_MODULES 45

// --- GALOIS FIELD GF(256) PARA REED-SOLOMON ---
static uint8_t gf_exp[512];
static uint8_t gf_log[256];

static void gf_init(void) {
    int x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = x;
        gf_exp[i + 255] = x;
        gf_log[x] = i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11D; // Polinômio primitivo QR: x^8 + x^4 + x^3 + x^2 + 1
    }
    gf_log[0] = 0;
}

static inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

static void rs_gen_poly(int nsym, uint8_t *poly) {
    memset(poly, 0, nsym + 1);
    poly[0] = 1;
    for (int i = 0; i < nsym; i++) {
        for (int j = i + 1; j > 0; j--) {
            poly[j] = poly[j] ^ gf_mul(poly[j - 1], gf_exp[i]);
        }
    }
}

static void rs_encode(const uint8_t *data, int data_len, uint8_t *ec, int ec_len, const uint8_t *gen_poly) {
    memset(ec, 0, ec_len);
    for (int i = 0; i < data_len; i++) {
        uint8_t factor = data[i] ^ ec[0];
        for (int j = 0; j < ec_len - 1; j++) {
            ec[j] = ec[j + 1] ^ gf_mul(gen_poly[j + 1], factor);
        }
        ec[ec_len - 1] = gf_mul(gen_poly[ec_len], factor);
    }
}

// Configuração das Versões QR (Nível L)
typedef struct {
    int version;
    int size;
    int data_cw;
    int ec_cw;
    int align_pos;
} QRVersionInfo;

static const QRVersionInfo qr_versions[] = {
    {1, 21, 19, 7,  0},
    {2, 25, 34, 10, 18},
    {3, 29, 55, 15, 22},
    {4, 33, 80, 20, 26},
    {5, 37, 108, 26, 30}
};

static void draw_finder(uint8_t grid[MAX_MODULES][MAX_MODULES], uint8_t res[MAX_MODULES][MAX_MODULES], int r0, int c0, int size) {
    for (int r = -1; r <= 7; r++) {
        for (int c = -1; c <= 7; c++) {
            int qr = r0 + r;
            int qc = c0 + c;
            if (qr >= 0 && qr < size && qc >= 0 && qc < size) {
                res[qr][qc] = 1;
                if (r >= 0 && r <= 6 && c >= 0 && c <= 6) {
                    if (r == 0 || r == 6 || c == 0 || c == 6 || (r >= 2 && r <= 4 && c >= 2 && c <= 4)) {
                        grid[qr][qc] = 1;
                    } else {
                        grid[qr][qc] = 0;
                    }
                } else {
                    grid[qr][qc] = 0; // Separator
                }
            }
        }
    }
}

static void draw_alignment(uint8_t grid[MAX_MODULES][MAX_MODULES], uint8_t res[MAX_MODULES][MAX_MODULES], int r0, int c0) {
    for (int r = -2; r <= 2; r++) {
        for (int c = -2; c <= 2; c++) {
            res[r0 + r][c0 + c] = 1;
            if (r == -2 || r == 2 || c == -2 || c == 2 || (r == 0 && c == 0)) {
                grid[r0 + r][c0 + c] = 1;
            } else {
                grid[r0 + r][c0 + c] = 0;
            }
        }
    }
}

static void render_qr_terminal(const uint8_t grid[MAX_MODULES][MAX_MODULES], int size) {
    int quiet = 2; // Borda de respiro para leitura precisa pela câmera

    // Top border
    for (int r = 0; r < quiet; r++) {
        for (int c = 0; c < size + quiet * 2; c++) printf("\033[47m  \033[0m");
        printf("\n");
    }

    // Grid data
    for (int r = 0; r < size; r++) {
        for (int q = 0; q < quiet; q++) printf("\033[47m  \033[0m");
        for (int c = 0; c < size; c++) {
            if (grid[r][c]) {
                printf("\033[40m  \033[0m"); // Módulo Preto
            } else {
                printf("\033[47m  \033[0m"); // Módulo Branco
            }
        }
        for (int q = 0; q < quiet; q++) printf("\033[47m  \033[0m");
        printf("\n");
    }

    // Bottom border
    for (int r = 0; r < quiet; r++) {
        for (int c = 0; c < size + quiet * 2; c++) printf("\033[47m  \033[0m");
        printf("\n");
    }
}

static int generate_qr_code(const char *text) {
    gf_init();
    size_t text_len = strlen(text);

    // Seleciona a menor versão QR que comporta o texto (Modo Byte, Level L)
    const QRVersionInfo *vinfo = NULL;
    for (int i = 0; i < 5; i++) {
        if (text_len + 3 <= (size_t)qr_versions[i].data_cw) {
            vinfo = &qr_versions[i];
            break;
        }
    }

    if (!vinfo) {
        printf("Erro: Texto muito longo para o gerador (máximo 105 caracteres).\n");
        return -1;
    }

    int size = vinfo->size;
    int data_cw_count = vinfo->data_cw;
    int ec_cw_count = vinfo->ec_cw;

    // 1. Bitstream Data Encoding (Byte Mode: 0100)
    uint8_t data_bytes[128] = {0};
    int bit_pos = 0;

    #define WRITE_BITS(val, count) do { \
        for (int _b = (count) - 1; _b >= 0; _b--) { \
            if ((val) & (1 << _b)) data_bytes[bit_pos / 8] |= (1 << (7 - (bit_pos % 8))); \
            bit_pos++; \
        } \
    } while (0)

    WRITE_BITS(0x4, 4);           // Byte mode indicator
    WRITE_BITS((int)text_len, 8); // Character count indicator

    for (size_t i = 0; i < text_len; i++) {
        WRITE_BITS((uint8_t)text[i], 8);
    }

    // Terminator (até 4 bits zero)
    int term_bits = (data_cw_count * 8) - bit_pos;
    if (term_bits > 4) term_bits = 4;
    WRITE_BITS(0, term_bits);

    // Alinhamento ao byte
    if (bit_pos % 8 != 0) {
        WRITE_BITS(0, 8 - (bit_pos % 8));
    }

    // Pad bytes (0xEC e 0x11 alternados)
    int current_bytes = bit_pos / 8;
    for (int i = current_bytes; i < data_cw_count; i++) {
        data_bytes[i] = (i % 2 == current_bytes % 2) ? 0xEC : 0x11;
    }

    // 2. Reed-Solomon Error Correction
    uint8_t gen_poly[64];
    uint8_t ec_bytes[64];
    rs_gen_poly(ec_cw_count, gen_poly);
    rs_encode(data_bytes, data_cw_count, ec_bytes, ec_cw_count, gen_poly);

    uint8_t all_codewords[256];
    memcpy(all_codewords, data_bytes, data_cw_count);
    memcpy(all_codewords + data_cw_count, ec_bytes, ec_cw_count);
    int total_codewords = data_cw_count + ec_cw_count;

    // 3. Montagem da Matriz QR
    uint8_t grid[MAX_MODULES][MAX_MODULES] = {0};
    uint8_t reserved[MAX_MODULES][MAX_MODULES] = {0};

    // Finders (3 cantos)
    draw_finder(grid, reserved, 0, 0, size);
    draw_finder(grid, reserved, 0, size - 7, size);
    draw_finder(grid, reserved, size - 7, 0, size);

    // Alignment pattern (se versão >= 2)
    if (vinfo->align_pos > 0) {
        draw_alignment(grid, reserved, vinfo->align_pos, vinfo->align_pos);
    }

    // Timing patterns
    for (int i = 8; i < size - 8; i++) {
        grid[6][i] = (i % 2 == 0);
        grid[i][6] = (i % 2 == 0);
        reserved[6][i] = 1;
        reserved[i][6] = 1;
    }

    // Dark Module
    grid[size - 8][8] = 1;
    reserved[size - 8][8] = 1;

    // Reserva área de Formato (BCH)
    for (int i = 0; i < 9; i++) {
        reserved[8][i] = 1;
        reserved[i][8] = 1;
        reserved[8][size - 1 - i] = 1;
        reserved[size - 1 - i][8] = 1;
    }

    // 4. Preenchimento dos dados em Zig-Zag com Máscara 0: ((r + c) % 2 == 0)
    int bit_idx = 0;
    int total_bits = total_codewords * 8;
    int dir = -1;
    int r = size - 1;

    for (int c = size - 1; c > 0; c -= 2) {
        if (c == 6) c--; // Pula coluna de timing
        for (int i = 0; i < size; i++) {
            int row = r + (dir < 0 ? -i : i);
            for (int col_offset = 0; col_offset < 2; col_offset++) {
                int col = c - col_offset;
                if (!reserved[row][col]) {
                    int bit = 0;
                    if (bit_idx < total_bits) {
                        bit = (all_codewords[bit_idx / 8] >> (7 - (bit_idx % 8))) & 1;
                        bit_idx++;
                    }
                    // Aplica máscara 0
                    if ((row + col) % 2 == 0) bit ^= 1;
                    grid[row][col] = bit;
                }
            }
        }
        r = (dir < 0) ? 0 : size - 1;
        dir = -dir;
    }

    // 5. Escreve Informação de Formato: Level L + Mask 0 = 0x77C4 (BCH 15 bits)
    uint16_t format_bits = 0x77C4;
    for (int i = 0; i < 15; i++) {
        int b = (format_bits >> i) & 1;
        // Top-left
        if (i <= 5) grid[8][i] = b;
        else if (i == 6) grid[8][7] = b;
        else if (i == 7) grid[8][8] = b;
        else if (i == 8) grid[7][8] = b;
        else grid[14 - i][8] = b;

        // Cantos
        if (i < 8) grid[size - 1 - i][8] = b;
        else grid[8][size - 15 + i] = b;
    }

    render_qr_terminal(grid, size);
    return 0;
}

static void run_wifi_wizard(void) {
    char ssid[128] = "";
    char pass[128] = "";

    printf("\n\033[1;35m==========================================\n");
    printf("[ Assistente de Conexão Wi-Fi (qrcli) ]\n");
    printf("==========================================\033[0m\n");

    printf("  \033[1;36m• Nome da Rede (SSID):\033[0m ");
    fflush(stdout);
    if (!fgets(ssid, sizeof(ssid), stdin)) return;
    ssid[strcspn(ssid, "\r\n")] = '\0';

    printf("  \033[1;36m• Senha do Wi-Fi:\033[0m ");
    fflush(stdout);
    if (!fgets(pass, sizeof(pass), stdin)) return;
    pass[strcspn(pass, "\r\n")] = '\0';

    if (strlen(ssid) == 0) {
        printf("\n\033[1;31m[Erro: Nome da rede não pode ser vazio]\033[0m\n");
        return;
    }

    char wifi_payload[300];
    if (strlen(pass) > 0) {
        snprintf(wifi_payload, sizeof(wifi_payload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
    } else {
        snprintf(wifi_payload, sizeof(wifi_payload), "WIFI:T:nopass;S:%s;;", ssid);
    }

    printf("\n\033[1;32m[QR Code Oficial de Wi-Fi Gerado com Sucesso!]\033[0m\n");
    printf("  Rede : \033[1;33m%s\033[0m\n", ssid);
    printf("  Senha: \033[1;33m%s\033[0m\n\n", strlen(pass) > 0 ? pass : "(Rede Aberta)");

    generate_qr_code(wifi_payload);

    printf("\n\033[0;32mAponte a câmera do celular para conectar automaticamente!\033[0m\n\n");
}

int main(int argc, char *argv[]) {
    utilipc_init();

    if (argc < 2) {
        printf("Usage:\n");
        printf("  qrcli \"<url_ou_texto>\"\n");
        printf("  qrcli \"nome_da_rede:senha\"        (Atalho Rápido Wi-Fi)\n");
        printf("  qrcli wifi                         (Assistente Interativo Wi-Fi)\n");
        printf("Examples:\n");
        printf("  qrcli \"https://google.com\"\n");
        printf("  qrcli \"MinhaCasa:senha12345\"\n");
        printf("  qrcli wifi\n");
        utilipc_close();
        return 1;
    }

    const char *arg = argv[1];

    // 1. Comando Interativo Wi-Fi
    if (strcmp(arg, "wifi") == 0 || strcmp(arg, "-w") == 0 || strcmp(arg, "--wifi") == 0) {
        if (argc >= 4) {
            char wifi_payload[300];
            snprintf(wifi_payload, sizeof(wifi_payload), "WIFI:T:WPA;S:%s;P:%s;;", argv[2], argv[3]);
            printf("\n\033[1;32m[Wi-Fi: Rede='%s', Senha='%s']\033[0m\n\n", argv[2], argv[3]);
            generate_qr_code(wifi_payload);
            utilipc_close();
            return 0;
        }
        run_wifi_wizard();
        utilipc_close();
        return 0;
    }

    // 2. Detecção automática de atalho "nome:senha"
    const char *colon = strchr(arg, ':');
    if (colon != NULL &&
        strncmp(arg, "http://", 7) != 0 &&
        strncmp(arg, "https://", 8) != 0 &&
        strncmp(arg, "WIFI:", 5) != 0) {

        char ssid[128] = "";
        char pass[128] = "";
        size_t ssid_len = colon - arg;
        if (ssid_len < sizeof(ssid)) {
            strncpy(ssid, arg, ssid_len);
            ssid[ssid_len] = '\0';
            strncpy(pass, colon + 1, sizeof(pass) - 1);

            char wifi_payload[300];
            snprintf(wifi_payload, sizeof(wifi_payload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);

            printf("\n\033[1;32m[Atalho Wi-Fi Detectado: Rede='%s' | Senha='%s']\033[0m\n\n", ssid, pass);
            generate_qr_code(wifi_payload);
            printf("\n\033[0;32mAponte a câmera do celular para conectar automaticamente!\033[0m\n\n");

            char log_msg[UTILIPC_MAX_MSG];
            snprintf(log_msg, sizeof(log_msg), "qrcli: generated Wi-Fi QR for '%s'", ssid);
            utilipc_write_status(-1, -1, -1, log_msg);

            utilipc_close();
            return 0;
        }
    }

    // 3. Modo Padrão (URL ou Texto livre)
    printf("\n\033[1;35m==========================================\n");
    printf("[ qrcli - QR Code Generator ]\n");
    printf("==========================================\033[0m\n");
    printf("  Conteúdo: \033[1;36m%s\033[0m\n\n", arg);

    generate_qr_code(arg);

    printf("\n\033[0;32mAponte a câmera do celular para escanear!\033[0m\n\n");

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "qrcli: generated QR for '%s'", arg);
    utilipc_write_status(-1, -1, -1, log_msg);

    utilipc_close();
    return 0;
}
