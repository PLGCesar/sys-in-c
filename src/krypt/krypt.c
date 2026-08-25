#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <errno.h>
#include "../libutilipc/utilipc.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_OK      "\033[1;32m"
#define COLOR_ERR     "\033[1;31m"
#define COLOR_TAG     "\033[1;33m"
#define COLOR_FILE    "\033[1;36m"

#define KRYPT_MAGIC "KRYPT26\0"
#define SALT_SIZE   16
#define NONCE_SIZE  12
#define HASH_SIZE   32
#define CHUNK_SZ    65536

// --- SHA-256 INTERNO ---
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef4a3f7,0xc67178f2
};

#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define ROTR(a,b) (((a) >> (b)) | ((a) << (32 - (b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SIG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ ((x) >> 10))

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[56] = (ctx->bitlen >> 56) & 0xFF;
    ctx->data[57] = (ctx->bitlen >> 48) & 0xFF;
    ctx->data[58] = (ctx->bitlen >> 40) & 0xFF;
    ctx->data[59] = (ctx->bitlen >> 32) & 0xFF;
    ctx->data[60] = (ctx->bitlen >> 24) & 0xFF;
    ctx->data[61] = (ctx->bitlen >> 16) & 0xFF;
    ctx->data[62] = (ctx->bitlen >> 8) & 0xFF;
    ctx->data[63] = (ctx->bitlen) & 0xFF;
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0xFF;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0xFF;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0xFF;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xFF;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xFF;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xFF;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xFF;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xFF;
    }
}

// Derivação de Chave PBKDF2-like (50.000 iterações de SHA-256 com Salt)
static void derive_key(const char *password, const uint8_t salt[SALT_SIZE], uint8_t key_out[32], uint8_t mac_key_out[32]) {
    uint8_t buffer[64 + SALT_SIZE];
    size_t pass_len = strlen(password);
    if (pass_len > 64) pass_len = 64;

    memcpy(buffer, password, pass_len);
    memcpy(buffer + pass_len, salt, SALT_SIZE);

    SHA256_CTX ctx;
    uint8_t current_hash[32];

    sha256_init(&ctx);
    sha256_update(&ctx, buffer, pass_len + SALT_SIZE);
    sha256_final(&ctx, current_hash);

    for (int i = 0; i < 50000; i++) {
        sha256_init(&ctx);
        sha256_update(&ctx, current_hash, 32);
        sha256_update(&ctx, salt, SALT_SIZE);
        sha256_final(&ctx, current_hash);
    }
    memcpy(key_out, current_hash, 32);

    // Chave de MAC / Integridade
    sha256_init(&ctx);
    sha256_update(&ctx, current_hash, 32);
    sha256_update(&ctx, (const uint8_t *)"INTEGRITY_KEY", 13);
    sha256_final(&ctx, mac_key_out);
}

// --- CIFRA DE FLUXO CHACHA20 ---
#define QR(a, b, c, d) \
    a += b; d ^= a; d = ROTL(d, 16); \
    c += d; b ^= c; b = ROTL(b, 12); \
    a += b; d ^= a; d = ROTL(d, 8);  \
    c += d; b ^= c; b = ROTL(b, 7);

static void chacha20_block(const uint32_t key[8], const uint32_t nonce[3], uint32_t counter, uint8_t out[64]) {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
        counter, nonce[0], nonce[1], nonce[2]
    };
    uint32_t x[16];
    memcpy(x, state, sizeof(x));

    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }

    for (int i = 0; i < 16; i++) {
        uint32_t v = x[i] + state[i];
        out[i * 4 + 0] = (v >> 0) & 0xFF;
        out[i * 4 + 1] = (v >> 8) & 0xFF;
        out[i * 4 + 2] = (v >> 16) & 0xFF;
        out[i * 4 + 3] = (v >> 24) & 0xFF;
    }
}

static void chacha20_xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t *counter, uint8_t *data, size_t len) {
    uint32_t k[8], n[3];
    for (int i = 0; i < 8; i++) k[i] = ((uint32_t *)key)[i];
    for (int i = 0; i < 3; i++) n[i] = ((uint32_t *)nonce)[i];

    uint8_t block[64];
    while (len > 0) {
        chacha20_block(k, n, *counter, block);
        (*counter)++;
        size_t chunk = (len > 64) ? 64 : len;
        for (size_t i = 0; i < chunk; i++) data[i] ^= block[i];
        data += chunk;
        len -= chunk;
    }
}

static void get_hidden_password(char *out_pass, size_t max_len) {
    printf("  %sDigite a senha mestre:%s ", COLOR_TAG, COLOR_RESET);
    fflush(stdout);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (!fgets(out_pass, max_len, stdin)) out_pass[0] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");

    size_t l = strlen(out_pass);
    while (l > 0 && (out_pass[l-1] == '\r' || out_pass[l-1] == '\n')) out_pass[--l] = '\0';
}

static int encrypt_file(const char *in_file, const char *out_file, const char *pass) {
    int in_fd = open(in_file, O_RDONLY);
    if (in_fd < 0) {
        fprintf(stderr, "  %s[ERRO]%s Nao foi possivel abrir '%s': %s\n", COLOR_ERR, COLOR_RESET, in_file, strerror(errno));
        return -1;
    }

    uint8_t salt[SALT_SIZE];
    uint8_t nonce[NONCE_SIZE];
    int rand_fd = open("/dev/urandom", O_RDONLY);
    if (rand_fd < 0 || read(rand_fd, salt, SALT_SIZE) != SALT_SIZE || read(rand_fd, nonce, NONCE_SIZE) != NONCE_SIZE) {
        fprintf(stderr, "  %s[ERRO]%s Falha ao gerar salt seguro de /dev/urandom\n", COLOR_ERR, COLOR_RESET);
        if (rand_fd >= 0) close(rand_fd);
        close(in_fd);
        return -1;
    }
    close(rand_fd);

    uint8_t key[32], mac_key[32];
    derive_key(pass, salt, key, mac_key);

    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0) {
        fprintf(stderr, "  %s[ERRO]%s Nao foi possivel criar '%s': %s\n", COLOR_ERR, COLOR_RESET, out_file, strerror(errno));
        close(in_fd);
        return -1;
    }

    // Escreve cabeçalho: MAGIC + Salt + Nonce
    write(out_fd, KRYPT_MAGIC, 8);
    write(out_fd, salt, SALT_SIZE);
    write(out_fd, nonce, NONCE_SIZE);

    uint8_t chunk[CHUNK_SZ];
    ssize_t n = 0;
    uint32_t counter = 1;

    SHA256_CTX mac_ctx;
    sha256_init(&mac_ctx);
    sha256_update(&mac_ctx, mac_key, 32);

    while ((n = read(in_fd, chunk, sizeof(chunk))) > 0) {
        chacha20_xor(key, nonce, &counter, chunk, n);
        sha256_update(&mac_ctx, chunk, n);
        write(out_fd, chunk, n);
    }

    uint8_t final_mac[32];
    sha256_final(&mac_ctx, final_mac);
    write(out_fd, final_mac, 32);

    close(in_fd);
    close(out_fd);

    printf("  %s[✔ SUCESSO]%s Arquivo criptografado em: %s%s%s (ChaCha20 + HMAC)\n",
           COLOR_OK, COLOR_RESET, COLOR_FILE, out_file, COLOR_RESET);
    return 0;
}

static int decrypt_file(const char *in_file, const char *out_file, const char *pass) {
    int in_fd = open(in_file, O_RDONLY);
    if (in_fd < 0) {
        fprintf(stderr, "  %s[ERRO]%s Nao foi possivel abrir '%s': %s\n", COLOR_ERR, COLOR_RESET, in_file, strerror(errno));
        return -1;
    }

    char magic[8];
    uint8_t salt[SALT_SIZE];
    uint8_t nonce[NONCE_SIZE];

    if (read(in_fd, magic, 8) != 8 || strcmp(magic, KRYPT_MAGIC) != 0) {
        fprintf(stderr, "  %s[ERRO]%s O arquivo '%s' nao e um arquivo criptografado valido pelo krypt!\n", COLOR_ERR, COLOR_RESET, in_file);
        close(in_fd);
        return -1;
    }

    read(in_fd, salt, SALT_SIZE);
    read(in_fd, nonce, NONCE_SIZE);

    uint8_t key[32], mac_key[32];
    derive_key(pass, salt, key, mac_key);

    off_t total_sz = lseek(in_fd, 0, SEEK_END);
    off_t payload_sz = total_sz - 8 - SALT_SIZE - NONCE_SIZE - 32;

    if (payload_sz < 0) {
        fprintf(stderr, "  %s[ERRO]%s Arquivo corrompido ou truncado!\n", COLOR_ERR, COLOR_RESET);
        close(in_fd);
        return -1;
    }

    // 1. Verificação de Integridade e Senha
    lseek(in_fd, 8 + SALT_SIZE + NONCE_SIZE, SEEK_SET);
    SHA256_CTX mac_ctx;
    sha256_init(&mac_ctx);
    sha256_update(&mac_ctx, mac_key, 32);

    uint8_t chunk[CHUNK_SZ];
    off_t remaining = payload_sz;
    while (remaining > 0) {
        size_t to_r = (remaining > (off_t)sizeof(chunk)) ? sizeof(chunk) : remaining;
        ssize_t n = read(in_fd, chunk, to_r);
        if (n <= 0) break;
        sha256_update(&mac_ctx, chunk, n);
        remaining -= n;
    }

    uint8_t calc_mac[32], file_mac[32];
    sha256_final(&mac_ctx, calc_mac);
    read(in_fd, file_mac, 32);

    if (memcmp(calc_mac, file_mac, 32) != 0) {
        fprintf(stderr, "\n  %s[ERRO DE AUTENTICACAO]%s Senha incorreta ou arquivo adulterado!\n\n", COLOR_ERR, COLOR_RESET);
        close(in_fd);
        return -1;
    }

    // 2. Descriptografia
    lseek(in_fd, 8 + SALT_SIZE + NONCE_SIZE, SEEK_SET);
    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        close(in_fd);
        return -1;
    }

    remaining = payload_sz;
    uint32_t counter = 1;
    while (remaining > 0) {
        size_t to_r = (remaining > (off_t)sizeof(chunk)) ? sizeof(chunk) : remaining;
        ssize_t n = read(in_fd, chunk, to_r);
        if (n <= 0) break;
        chacha20_xor(key, nonce, &counter, chunk, n);
        write(out_fd, chunk, n);
        remaining -= n;
    }

    close(in_fd);
    close(out_fd);

    printf("  %s[✔ SUCESSO]%s Arquivo descriptografado com integridade verificada em: %s%s%s\n",
           COLOR_OK, COLOR_RESET, COLOR_FILE, out_file, COLOR_RESET);
    return 0;
}

int main(int argc, char *argv[]) {
    utilipc_init();

    if (argc < 3 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
        printf("%s[ krypt - Pure C ChaCha20 File Encryption Vault ]%s\n", COLOR_TITLE, COLOR_RESET);
        printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
        printf("Usage:\n");
        printf("  krypt -e <ARQUIVO> [-o <DESTINO.kr>]    (Criptografar)\n");
        printf("  krypt -d <ARQUIVO.kr> [-o <DESTINO>]    (Descriptografar)\n\n");
        printf("Exemplos:\n");
        printf("  krypt -e segredo.txt\n");
        printf("  krypt -d segredo.txt.kr -o recuperado.txt\n");
        printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
        utilipc_close();
        return 0;
    }

    int mode_encrypt = (strcmp(argv[1], "-e") == 0);
    int mode_decrypt = (strcmp(argv[1], "-d") == 0);
    const char *in_file = argv[2];
    char out_file[512] = "";

    if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
        strncpy(out_file, argv[4], sizeof(out_file) - 1);
    } else {
        if (mode_encrypt) snprintf(out_file, sizeof(out_file), "%s.kr", in_file);
        else {
            strncpy(out_file, in_file, sizeof(out_file) - 1);
            char *dot = strstr(out_file, ".kr");
            if (dot) *dot = '\0';
            else strcat(out_file, ".dec");
        }
    }

    char pass[128];
    get_hidden_password(pass, sizeof(pass));

    if (strlen(pass) == 0) {
        fprintf(stderr, "krypt: senha nao pode ser vazia!\n");
        utilipc_close();
        return 1;
    }

    int res = 0;
    if (mode_encrypt) {
        res = encrypt_file(in_file, out_file, pass);
    } else if (mode_decrypt) {
        res = decrypt_file(in_file, out_file, pass);
    }

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "krypt: %s '%s'", mode_encrypt ? "encrypted" : "decrypted", in_file);
    utilipc_write_status(-1, -1, -1, log_msg);

    utilipc_close();
    return res;
}
