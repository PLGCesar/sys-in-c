#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include "../libutilipc/utilipc.h"

#define MAX_ITEMS_PER_TAB 16
#define TOTAL_TABS 7
#define THEME_COUNT 5

#define PI 3.14159265358979323846

typedef struct {
    const char *theme_name;
    const char *border;
    const char *header_bg;
    const char *header_fg;
    const char *tab_sel_bg;
    const char *tab_sel_fg;
    const char *item_sel_bg;
    const char *item_sel_fg;
    const char *accent_pink;
    const char *accent_mint;
    const char *accent_sky;
    const char *accent_peach;
    const char *text_main;
    const char *text_muted;
} BiosTheme;

static const BiosTheme themes[THEME_COUNT] = {
    // 0: Sakura Pink
    {
        "🌸 Sakura Pink",
        "\033[38;2;203;166;247m", "\033[48;2;245;194;231m", "\033[38;2;30;30;46m",
        "\033[48;2;245;194;231m", "\033[1;38;2;30;30;46m",
        "\033[48;2;166;227;161m", "\033[1;38;2;30;30;46m",
        "\033[38;2;245;194;231m", "\033[38;2;166;227;161m", "\033[38;2;137;220;235m",
        "\033[38;2;249;226;175m", "\033[38;2;205;214;244m", "\033[38;2;108;112;134m"
    },
    // 1: Matcha Mint
    {
        "🍵 Matcha Mint",
        "\033[38;2;166;227;161m", "\033[48;2;166;227;161m", "\033[38;2;24;24;37m",
        "\033[48;2;166;227;161m", "\033[1;38;2;24;24;37m",
        "\033[48;2;249;226;175m", "\033[1;38;2;24;24;37m",
        "\033[38;2;166;227;161m", "\033[38;2;148;226;213m", "\033[38;2;137;220;235m",
        "\033[38;2;249;226;175m", "\033[38;2;205;214;244m", "\033[38;2;108;112;134m"
    },
    // 2: Lavender Dream
    {
        "🌌 Lavender Dream",
        "\033[38;2;203;166;247m", "\033[48;2;203;166;247m", "\033[38;2;17;17;27m",
        "\033[48;2;203;166;247m", "\033[1;38;2;17;17;27m",
        "\033[48;2;245;194;231m", "\033[1;38;2;17;17;27m",
        "\033[38;2;203;166;247m", "\033[38;2;166;227;161m", "\033[38;2;180;190;254m",
        "\033[38;2;249;226;175m", "\033[38;2;205;214;244m", "\033[38;2;108;112;134m"
    },
    // 3: Sunbeam Peach
    {
        "🍑 Sunbeam Peach",
        "\033[38;2;249;226;175m", "\033[48;2;249;226;175m", "\033[38;2;30;30;46m",
        "\033[48;2;249;226;175m", "\033[1;38;2;30;30;46m",
        "\033[48;2;245;194;231m", "\033[1;38;2;30;30;46m",
        "\033[38;2;249;226;175m", "\033[38;2;166;227;161m", "\033[38;2;137;220;235m",
        "\033[38;2;250;179;135m", "\033[38;2;205;214;244m", "\033[38;2;108;112;134m"
    },
    // 4: Cyber Sky
    {
        "☁️ Cyber Sky",
        "\033[38;2;137;220;235m", "\033[48;2;137;220;235m", "\033[38;2;17;17;27m",
        "\033[48;2;137;220;235m", "\033[1;38;2;17;17;27m",
        "\033[48;2;148;226;213m", "\033[1;38;2;17;17;27m",
        "\033[38;2;137;220;235m", "\033[38;2;166;227;161m", "\033[38;2;180;190;254m",
        "\033[38;2;249;226;175m", "\033[38;2;205;214;244m", "\033[38;2;108;112;134m"
    }
};

static int cur_theme_idx = 0;
#define C_RESET "\033[0m"

typedef struct {
    const char *icon;
    const char *name;
    const char *cmd;
    const char *tagline;
    const char *description;
    const char *example;
} MenuEntry;

typedef struct {
    const char *tab_icon;
    const char *tab_name;
    int item_count;
    MenuEntry items[MAX_ITEMS_PER_TAB];
} MenuTab;

static MenuTab tabs[TOTAL_TABS] = {
    // ABA 0: GUI DESKTOP VIRTUAL
    {
        "🖥️", "GUI", 4,
        {
            {"🪟", "Abrir Monitor Desktop", "gui_desktop_cmd", "Ambiente Grafico com Wallpaper XP", "Abre a area de trabalho virtual com wallpaper matematico XP Bliss e mouse.", "Pressione Enter"},
            {"⚡", "Criar App em C",         "create_c_cmd",    "Gerador & Runner C Nativo",     "Cria um mini-aplicativo em C puro, compila com GCC e executa em janela.", "gcc app.c -O2"},
            {"🐍", "Criar App em Python",    "create_py_cmd",   "Runner JIT pythont 3.5",        "Escreve e executa script Python nativo com F-Strings e Dicionarios.", "pythont app.py"},
            {"☕", "Criar App em Java",      "create_java_cmd", "Compilador & Runner Java",      "Cria uma classe Java e executa automaticamente via JDK/JVM.", "javac App.java && java App"}
        }
    },
    // ABA 1: MAIN / QUICK LAUNCH
    {
        "✨", "Main", 6,
        {
            {"🌸", "sysinfo",   "./sysinfo",   "Diagnostico Geral do SO",      "Exibe resumo completo do sistema, memoria RAM, versao do kernel e arquitetura.", "sysinfo"},
            {"⚡", "cpuplot",   "./cpuplot",   "Dashboard de CPU e RAM",       "Graficos animados ao vivo de carga de CPU, RAM, SWAP e latencia de rede.", "cpuplot"},
            {"💻", "ltop",      "./ltop",      "Monitor Interativo de Tarefas","Gerenciador de processos em tempo real com barras de uso e raio-X de processos.", "ltop"},
            {"🔋", "pwr",       "./pwr",       "Bateria & Sensores Termicos",  "Monitora fluxo eletrico (Watts/mA), nivel de bateria e zonas termicas da CPU.", "pwr -w"},
            {"✨", "bench",     "./bench",     "Micro-Benchmark de Hardware",  "Testa operacoes/segundo (MOPs) da CPU Single/Multi-core e velocidade da RAM.", "bench"},
            {"🕹️", "chip8",     "./chip8",     "Emulador de Maquina 8-Bit",    "Maquina virtual e emulador Chip-8 retro com gerador de labirintos embutido.", "chip8"}
        }
    },
    // ABA 2: SYSTEM & STORAGE
    {
        "🛠️", "System", 7,
        {
            {"📝", "tedit",     "./tedit",     "Editor de Codigo & Servidor",  "Editor visual com syntax highlighting, busca, clipboard e servidor web embutido.", "tedit index.html"},
            {"🌳", "tree",      "./tree",      "Arvore de Pastas e Arquivos",  "Desenha a hierarquia visual de diretorios com permissoes, tamanhos e cores.", "tree -L 2"},
            {"🔍", "ffind",     "./ffind",     "Buscador Recursivo Rapido",    "Localiza arquivos pelo nome ignorando pastas de sistema e arquivos pesados.", "ffind \"main.c\""},
            {"👥", "fdup",      "./fdup",      "Detector de Duplicados",       "Varre diretorios inteiros e encontra copias identicas utilizando hash SHA-256.", "fdup ."},
            {"💾", "diskbench", "./diskbench", "Benchmark de Armazenamento",   "Mede velocidade de leitura/escrita sequencial em MB/s e IOPS aleatorios 4K.", "diskbench"},
            {"📁", "org",       "./org",       "Organizador de Arquivos",      "Agrupa arquivos automaticamente por extensao e remove pastas vazias.", "org --by-ext"},
            {"🔒", "krypt",     "./krypt",     "Cofre Criptografico ChaCha20", "Criptografa e descriptografa arquivos com chave derivada e HMAC de integridade.", "krypt -e segredo.txt"}
        }
    },
    // ABA 3: NETWORK
    {
        "🌐", "Network", 7,
        {
            {"📡", "netscan",   "./netscan",   "Radar Wi-Fi & Rede Local",     "Varre todos os IPs locais (1-254) detectando portas abertas e tipo de aparelho.", "netscan"},
            {"🚀", "speedtest", "./speedtest", "Teste de Velocidade Internet", "Mede taxa de download em Mbps e latencia em milissegundos via CDN Cloudflare.", "speedtest"},
            {"📋", "netclip",   "./netclip",   "Clipboard P2P sem Fio (Wi-Fi)","Transfere textos e arquivos diretamente entre celular e PC na mesma rede Wi-Fi.", "netclip listen"},
            {"🔎", "dnsquery",  "./dnsquery",  "Inspetor de Pacotes DNS Raw",  "Envia queries UDP RFC 1035 (A, AAAA, MX, TXT) e analisa respostas brutas.", "dnsquery google.com A"},
            {"⏰", "sntp",      "./sntp",      "Sincronizador de Hora Atomica","Calcula o desvio em milissegundos do relogio local via servidores NTP.br e GPS.", "sntp"},
            {"🚪", "portcheck", "./portcheck", "Scanner de Portas TCP",        "Testa conexoes em portas individuais ou listas com medicao de latencia.", "portcheck google.com 80,443"},
            {"🧮", "snc",       "./snc",       "Calculadora de Sub-rede & CIDR","Calcula endereco de rede, broadcast, faixa de hosts e mascara binaria.", "snc 192.168.1.0/24"}
        }
    },
    // ABA 4: MULTIMEDIA & AUDIO
    {
        "🎨", "Multimedia", 7,
        {
            {"🕹️", "raycast3d", "./raycast3d", "Motor 3D Wolfenstein FPS",     "Motor grafico 3D em primeira pessoa no terminal a 60 FPS com minimapa e arma.", "raycast3d"},
            {"🎵", "bytebeat",  "./bytebeat",  "Sintetizador 24-Bit & Guitarras","Gera audio procedural: chiptune 8-bit, guitarras Karplus, Beat Drop e agua.", "bytebeat -p 16 --play"},
            {"🌌", "asciiray",  "./asciiray",  "Motor Ray Tracer 3D em ASCII", "Renderizador 3D em tempo real com esferas, iluminacao e reflexos TrueColor.", "asciiray"},
            {"💚", "matrix",    "./matrix",    "Chuva Digital do Matrix",      "Efeito de codigo caindo no terminal com caracteres brilhantes animados.", "matrix"},
            {"📱", "qrcli",     "./qrcli",     "Gerador de QR Code com Wi-Fi", "Gera QR Codes escaneaveis para textos, links ou conexao automatica a Wi-Fi.", "qrcli wifi"},
            {"🔢", "calc",      "./calc",      "Calculadora Matematica & Bases","Avaliador de expressoes com suporte a variaveis, trigonometria e bases bin/hex.", "calc"},
            {"✨", "jsonview",  "./jsonview",  "Formatador e Colorizador JSON","Formata e colore estruturas JSON ou converte sintaxes shorthand.", "jsonview '{\"ok\":true}'"}
        }
    },
    // ABA 5: LOW-UTILS
    {
        "📦", "Low-Utils", 7,
        {
            {"🐚", "lsh",       "./lsh",       "Shell Unix Inteligente",       "Terminal de comandos com deteccao automatica de ambiente, aliases e pipes.", "lsh"},
            {"👤", "whoami",    "./whoami",    "Auditoria Completa de Processo","Inspeciona UID, grupos, limites de memoria do kernel, FDs e terminal TTY.", "whoami -uo"},
            {"📂", "ls",        "./ls",        "Listador de Pastas com Inodes","Lista arquivos e pastas com cores semanticas, datas e ordenacao inteligente.", "ls -l"},
            {"📖", "cat",       "./cat",       "Concatenador de Byte Stream",  "Exibe arquivos com formatacao, numeracao de linhas e deteccao de extensao.", "cat -sz low.h"},
            {"🗑️", "rmd",       "./rmd",       "Destruidor Seguro de Arquivos","Tritura arquivos sobrescrevendo blocos fisicos com fsync anti-forense e cripto.", "rmd -p 3 lixo.txt"},
            {"📊", "df",        "./df",        "Uso de Disco & Barras Visuais","Exibe espaco livre e utilizado das particoes do sistema em medidores visuais.", "df -T"},
            {"🛡️", "jail",      "./jail",      "Sandbox de Kernel Seccomp-BPF","Conseqüencia e isola comandos bloqueando chamadas de rede ou modificacao de disco.", "jail --no-net -- ./sysinfo"}
        }
    },
    // ABA 6: BIOS SETUP & EXIT
    {
        "⚙️", "Setup", 3,
        {
            {"🎨", "Trocar Tema",    "theme_cmd",   "Alternar Tema de Cores",     "Altera dinamicamente a paleta de cores entre tons pastel kawaii.", "Tecla 'T'"},
            {"🌸", "Abrir Shell lsh","./lsh",       "Iniciar Terminal lsh",       "Abre uma sessao interativa com o shell nativo lsh.", "lsh"},
            {"🚪", "Sair do BIOS",   "exit_cmd",    "Encerrar Sessao",            "Fecha o utilitario SYSBOX BIOS e retorna ao terminal.", "ESC / Q"}
        }
    }
};

static struct termios orig_term;
static int cur_tab = 0;
static int cur_item = 0;
static int scroll_offset = 0;
static int keep_running = 1;
static char custom_args_buf[256] = "";
static int wallpaper_mode = 0; // 0 = XP Bliss, 1 = Sunset, 2 = Sakura

static const char *mascots[] = {
    "(づ｡◕‿‿◕｡)づ",
    "( •̀ ω •́ )✧",
    "(｡♥‿♥｡)",
    "(=^･ω･^=)",
    "⸜(｡˃ ᵕ ˂ )⸝"
};

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
    printf("\033[?1049l\033[?25h\033[0m\033[H\033[J");
    fflush(stdout);
    utilipc_close();
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(disable_raw_mode);

    struct termios raw = orig_term;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?1049h\033[?25l\033[H\033[J");
    fflush(stdout);
}

static void get_term_size(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    } else {
        *cols = 80;
        *rows = 24;
    }
}

static void app_runner_flow(int lang_type) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
    printf("\033[?1049l\033[?25h\033[0m\033[H\033[J");

    const char *tmp = getenv("TMPDIR");
    if (!tmp || strlen(tmp) == 0) tmp = "/tmp";

    if (lang_type == 0) { // C APP
        printf("\n\033[1;35m╭────────────────────────────────────────────────────────────╮\033[0m\n");
        printf("\033[1;35m│\033[0m  \033[1;36m⚡ CRIADOR & RUNNER DE APLICATIVOS EM C NATIVO\033[0m            \033[1;35m│\033[0m\n");
        printf("\033[1;35m╰────────────────────────────────────────────────────────────╯\033[0m\n\n");

        char c_path[512], bin_path[512];
        snprintf(c_path, sizeof(c_path), "%s/gui_app.c", tmp);
        snprintf(bin_path, sizeof(bin_path), "%s/gui_app_bin", tmp);

        FILE *fp = fopen(c_path, "w");
        if (fp) {
            fprintf(fp,
                "#include <stdio.h>\n"
                "#include <math.h>\n"
                "int main(void) {\n"
                "    printf(\"\\n\\033[1;32m[✔ Aplicativo C Executado com Sucesso!]\\033[0m\\n\");\n"
                "    printf(\"  • Calculando Raiz de 65536: %%.0f\\n\", sqrt(65536));\n"
                "    printf(\"  • Ola do ambiente Desktop do sysbox!\\n\\n\");\n"
                "    return 0;\n"
                "}\n");
            fclose(fp);
        }

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "gcc -O2 %s -o %s -lm && %s", c_path, bin_path, bin_path);
        (void)!system(cmd);
    } else if (lang_type == 1) { // PYTHON APP
        printf("\n\033[1;35m╭────────────────────────────────────────────────────────────╮\033[0m\n");
        printf("\033[1;35m│\033[0m  \033[1;32m🐍 RUNNER DE APLICATIVOS PYTHON (pythont 3.5 JIT)\033[0m          \033[1;35m│\033[0m\n");
        printf("\033[1;35m╰────────────────────────────────────────────────────────────╯\033[0m\n\n");

        char py_path[512];
        snprintf(py_path, sizeof(py_path), "%s/gui_script.py", tmp);

        FILE *fp = fopen(py_path, "w");
        if (fp) {
            fprintf(fp,
                "usuario = {'nome': 'Cesar', 'xp': 9999}\n"
                "print(f'=== Executando Python no Desktop GUI ===')\n"
                "print(f'Player: {usuario[\"nome\"]} | Level XP: {usuario[\"xp\"]}')\n"
                "numeros = [10, 20, 30, 40]\n"
                "numeros.append(50)\n"
                "print(f'Lista com append: {numeros} | Soma: {sum(numeros)}')\n");
            fclose(fp);
        }

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "./pythont %s", py_path);
        (void)!system(cmd);
    } else if (lang_type == 2) { // JAVA APP
        printf("\n\033[1;35m╭────────────────────────────────────────────────────────────╮\033[0m\n");
        printf("\033[1;35m│\033[0m  \033[1;33m☕ COMPILADOR & RUNNER DE APLICATIVOS JAVA\033[0m                \033[1;35m│\033[0m\n");
        printf("\033[1;35m╰────────────────────────────────────────────────────────────╯\033[0m\n\n");

        char java_path[512];
        snprintf(java_path, sizeof(java_path), "%s/GuiApp.java", tmp);

        FILE *fp = fopen(java_path, "w");
        if (fp) {
            fprintf(fp,
                "public class GuiApp {\n"
                "    public static void main(String[] args) {\n"
                "        System.out.println(\"\\n\\u001B[1;33m[☕ Java Micro-App Executado no Desktop GUI]\\u001B[0m\");\n"
                "        System.out.println(\"  • Memoria JVM Maxima: \" + (Runtime.getRuntime().maxMemory() / (1024*1024)) + \" MB\");\n"
                "        System.out.println(\"  • Versao do Java: \" + System.getProperty(\"java.version\") + \"\\n\");\n"
                "    }\n"
                "}\n");
            fclose(fp);
        }

        if (system("which javac >/dev/null 2>&1") == 0) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "cd %s && javac GuiApp.java && java GuiApp", tmp);
            (void)!system(cmd);
        } else {
            printf("  \033[1;31m[Aviso]: JDK (javac/java) nao encontrado no sistema.\033[0m\n\n");
        }
    }

    printf("\n\033[1;35m[✨ Pressione ENTER para retornar...] \033[0m");
    fflush(stdout);
    char dummy[16];
    if (read(STDIN_FILENO, dummy, sizeof(dummy)) <= 0) {}

    enable_raw_mode();
}

/* =========================================================================
 *  DESKTOP GUI INTERATIVO OTIMIZADO (DIRTY REDRAW + CURSOR)
 * ========================================================================= */
static void render_desktop_monitor_frame(int box_w, int box_h, int start_x, int start_y, int cursor_item) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char clock_str[32];
    strftime(clock_str, sizeof(clock_str), "%H:%M", tm_info);

    const char *wall_names[] = {"Windows XP Bliss", "Cyber Sunset", "Sakura Pastel"};

    // 1. Borda Superior do Monitor Retrô
    printf("\033[%d;%dH\033[38;2;137;220;235m╭", start_y, start_x);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╮\033[0m");

    // Barra de Título do Monitor
    printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", start_y + 1, start_x);
    printf("\033[1;37;48;2;36;94;218m 🖥️ VIRTUAL PC MONITOR — [%s] \033[0m%*s",
           wall_names[wallpaper_mode], (int)(box_w - 36 - strlen(wall_names[wallpaper_mode])), "");
    printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", start_y + 1, start_x + box_w - 1);

    int inner_h = box_h - 4;
    int inner_w = box_w - 2;

    int win_w = 46;
    int win_h = 10;
    int win_x = (inner_w - win_w) / 2;
    int win_y = (inner_h - win_h) / 2;

    const char *app_icons[] = {
        " [1] ⚡ App em C Nativo (GCC)",
        " [2] 🐍 Script Python (pythont 3.5)",
        " [3] ☕ Aplicativo Java (JDK)",
        " [4] 🎨 Trocar Papel de Parede",
        " [5] 🚪 Retornar ao BIOS Setup"
    };

    // 2. Renderização do Interior da Tela
    for (int r = 0; r < inner_h; r++) {
        int line_y = start_y + 2 + r;
        double ny = (double)r / (double)inner_h;

        printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", line_y, start_x);

        // Se estiver fora da janela flutuante, desenha o Wallpaper
        int is_in_window = (r >= win_y && r < win_y + win_h);

        if (!is_in_window) {
            if (wallpaper_mode == 0) { // XP Bliss
                double hill_y = 0.55 + 0.12 * sin((double)r * 0.4);
                if (ny < hill_y) {
                    // Céu Azul
                    printf("\033[48;2;60;140;230m%*s\033[0m", inner_w, "");
                } else {
                    // Colina Verde
                    printf("\033[48;2;76;175;80m%*s\033[0m", inner_w, "");
                }
            } else if (wallpaper_mode == 1) { // Sunset
                printf("\033[48;2;180;60;120m%*s\033[0m", inner_w, "");
            } else { // Sakura
                printf("\033[48;2;245;194;231m%*s\033[0m", inner_w, "");
            }
        } else {
            // Desenha o fundo antes da janela
            if (wallpaper_mode == 0) printf("\033[48;2;60;140;230m%*s\033[0m", win_x, "");
            else printf("\033[48;2;180;60;120m%*s\033[0m", win_x, "");

            // Conteúdo da Janela Estilo XP
            int win_row = r - win_y;
            if (win_row == 0) {
                // Barra de título XP
                printf("\033[1;37;48;2;36;94;218m 🪟 Windows XP Studio [ _ ] [ × ] \033[0m%*s", (int)(win_w - 33), "");
            } else if (win_row == 1) {
                printf("\033[1;30;48;2;240;242;245m Escolha o aplicativo ou ambiente: %*s\033[0m", (int)(win_w - 36), "");
            } else if (win_row >= 3 && win_row <= 7) {
                int item_idx = win_row - 3;
                int is_sel = (item_idx == cursor_item);

                if (is_sel) {
                    printf("\033[1;30;48;2;166;227;161m 🖰 %-38.38s ◄\033[0m", app_icons[item_idx]);
                } else {
                    printf("\033[0;30;48;2;240;242;245m   %-39.39s\033[0m", app_icons[item_idx]);
                }
                int rem_w = win_w - 42;
                if (rem_w > 0) printf("\033[48;2;240;242;245m%*s\033[0m", rem_w, "");
            } else {
                printf("\033[48;2;240;242;245m%*s\033[0m", win_w, "");
            }

            // Fundo depois da janela
            int right_rem = inner_w - win_x - win_w;
            if (right_rem > 0) {
                if (wallpaper_mode == 0) printf("\033[48;2;60;140;230m%*s\033[0m", right_rem, "");
                else printf("\033[48;2;180;60;120m%*s\033[0m", right_rem, "");
            }
        }

        printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", line_y, start_x + box_w - 1);
    }

    // 3. Barra de Tarefas XP no Fundo do Monitor
    printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", start_y + inner_h + 2, start_x);
    printf("\033[1;37;48;2;36;94;218m 🌸 Iniciar \033[0m\033[1;37;48;2;24;60;160m [ W: Trocar Fundo ] \033[0m%*s\033[1;37;48;2;20;50;140m 🕒 %s \033[0m",
           (int)(inner_w - 38), "", clock_str);
    printf("\033[%d;%dH\033[38;2;137;220;235m│\033[0m", start_y + inner_h + 2, start_x + box_w - 1);

    // Borda Inferior
    printf("\033[%d;%dH\033[38;2;137;220;235m╰", start_y + inner_h + 3, start_x);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╯\033[0m");

    fflush(stdout);
}

static void run_desktop_gui(void) {
    int gui_running = 1;
    int cursor_item = 0; // 0..4

    while (gui_running) {
        int cols, rows;
        get_term_size(&cols, &rows);

        int box_w = cols - 2;
        if (box_w > 96) box_w = 96;
        if (box_w < 64) box_w = 64;

        int box_h = rows - 2;
        if (box_h > 26) box_h = 26;
        if (box_h < 18) box_h = 18;

        int start_x = (cols - box_w) / 2;
        if (start_x < 1) start_x = 1;
        int start_y = (rows - box_h) / 2;
        if (start_y < 1) start_y = 1;

        render_desktop_monitor_frame(box_w, box_h, start_x, start_y, cursor_item);

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 }; // Atualiza a cada 1s apenas para o relógio

        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            char seq[8];
            ssize_t n = read(STDIN_FILENO, seq, sizeof(seq));
            if (n <= 0) break;
            char c = seq[0];

            if (c == 'q' || c == 'Q' || c == 27) {
                if (n == 1) { gui_running = 0; break; }
                if (n >= 3 && seq[1] == '[') {
                    if (seq[2] == 'A') { // Seta CIMA
                        if (cursor_item > 0) cursor_item--;
                        else cursor_item = 4;
                    } else if (seq[2] == 'B') { // Seta BAIXO
                        if (cursor_item < 4) cursor_item++;
                        else cursor_item = 0;
                    }
                }
            } else if (c == 'w' || c == 'W') {
                wallpaper_mode = (wallpaper_mode + 1) % 3;
            } else if (c == '1') {
                app_runner_flow(0);
            } else if (c == '2') {
                app_runner_flow(1);
            } else if (c == '3') {
                app_runner_flow(2);
            } else if (c == '4') {
                wallpaper_mode = (wallpaper_mode + 1) % 3;
            } else if (c == '5') {
                gui_running = 0;
            } else if (c == '\r' || c == '\n' || c == ' ') {
                if (cursor_item == 0) app_runner_flow(0);
                else if (cursor_item == 1) app_runner_flow(1);
                else if (cursor_item == 2) app_runner_flow(2);
                else if (cursor_item == 3) wallpaper_mode = (wallpaper_mode + 1) % 3;
                else if (cursor_item == 4) gui_running = 0;
            }
        }
    }
}

static void launch_tool(const char *cmd, const char *args) {
    if (strcmp(cmd, "exit_cmd") == 0) {
        keep_running = 0;
        return;
    }
    if (strcmp(cmd, "theme_cmd") == 0) {
        cur_theme_idx = (cur_theme_idx + 1) % THEME_COUNT;
        return;
    }
    if (strcmp(cmd, "gui_desktop_cmd") == 0) {
        run_desktop_gui();
        return;
    }
    if (strcmp(cmd, "create_c_cmd") == 0) {
        app_runner_flow(0);
        return;
    }
    if (strcmp(cmd, "create_py_cmd") == 0) {
        app_runner_flow(1);
        return;
    }
    if (strcmp(cmd, "create_java_cmd") == 0) {
        app_runner_flow(2);
        return;
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
    printf("\033[?1049l\033[?25h\033[0m\033[H\033[J");

    const BiosTheme *th = &themes[cur_theme_idx];

    char full_exec[512];
    if (args && strlen(args) > 0) {
        snprintf(full_exec, sizeof(full_exec), "%s %s", cmd, args);
    } else {
        snprintf(full_exec, sizeof(full_exec), "%s", cmd);
    }

    printf("\n%s╭────────────────────────────────────────────────────────────────────────────╮%s\n", th->border, C_RESET);
    printf("%s│%s  %s🌸 SYSBOX BIOS:%s Executando: %s%-45.45s%s %s│%s\n",
           th->border, C_RESET, th->accent_pink, C_RESET, th->accent_mint, full_exec, C_RESET, th->border, C_RESET);
    printf("%s╰────────────────────────────────────────────────────────────────────────────╯%s\n\n", th->border, C_RESET);

    utilipc_send_msg("sysbox", cmd, "Launched tool from BIOS Menu");

    (void)!system(full_exec);

    printf("\n\n%s[✨ Execucao finalizada. Pressione ENTER para retornar ao SYSBOX...]%s ", th->accent_pink, C_RESET);
    fflush(stdout);
    char buf[16];
    if (read(STDIN_FILENO, buf, sizeof(buf)) <= 0) {}

    enable_raw_mode();
}

static void prompt_custom_arguments(void) {
    int term_cols, term_rows;
    get_term_size(&term_cols, &term_rows);
    const BiosTheme *th = &themes[cur_theme_idx];

    int box_w = term_cols - 2;
    if (box_w > 98) box_w = 98;
    int box_h = term_rows - 2;
    if (box_h > 28) box_h = 28;
    int start_x = (term_cols - box_w) / 2;
    int start_y = (term_rows - box_h) / 2;

    int prompt_y = start_y + box_h - 2;

    printf("\033[%d;%dH%s│%s %sParametros para '%s':%s ",
           prompt_y, start_x, th->border, C_RESET, th->accent_peach, tabs[cur_tab].items[cur_item].name, C_RESET);
    printf("\033[?25h");
    fflush(stdout);

    char buf[256] = "";
    size_t idx = 0;

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        if (c == 27 || c == 3) {
            buf[0] = '\0';
            break;
        }
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == 127 || c == '\b') {
            if (idx > 0) {
                buf[--idx] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (isprint((unsigned char)c) && idx < sizeof(buf) - 1) {
            buf[idx++] = c;
            buf[idx] = '\0';
            putchar(c);
            fflush(stdout);
        }
    }

    printf("\033[?25l");
    if (strlen(buf) > 0) {
        strncpy(custom_args_buf, buf, sizeof(custom_args_buf) - 1);
        launch_tool(tabs[cur_tab].items[cur_item].cmd, custom_args_buf);
        custom_args_buf[0] = '\0';
    }
}

static void wrap_text(const char *src, char lines_out[3][128], int max_w) {
    lines_out[0][0] = '\0';
    lines_out[1][0] = '\0';
    lines_out[2][0] = '\0';
    if (!src || max_w <= 4) return;

    int line_idx = 0;
    int col = 0;
    const char *p = src;

    while (*p && line_idx < 3) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char *word_start = p;
        while (*p && *p != ' ') p++;
        int word_len = p - word_start;

        if (col + word_len >= max_w) {
            line_idx++;
            col = 0;
            if (line_idx >= 3) break;
        }

        if (col > 0) {
            strcat(lines_out[line_idx], " ");
            col++;
        }

        strncat(lines_out[line_idx], word_start, word_len);
        col += word_len;
    }
}

static void render_bios_setup(void) {
    int term_cols, term_rows;
    get_term_size(&term_cols, &term_rows);

    const BiosTheme *th = &themes[cur_theme_idx];

    int box_w = term_cols - 2;
    if (box_w > 98) box_w = 98;
    if (box_w < 66) box_w = 66;

    int box_h = term_rows - 2;
    if (box_h > 28) box_h = 28;
    if (box_h < 19) box_h = 19;

    int start_x = (term_cols - box_w) / 2;
    if (start_x < 1) start_x = 1;
    int start_y = (term_rows - box_h) / 2;
    if (start_y < 1) start_y = 1;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    utilipc_data_t ipc_data;
    memset(&ipc_data, 0, sizeof(ipc_data));
    utilipc_read_status(&ipc_data);

    // 1. TOPO: Borda Superior
    printf("\033[%d;%dH%s╭", start_y, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╮%s", C_RESET);

    // Linha do Título Principal
    printf("\033[%d;%dH%s│%s", start_y + 1, start_x, th->border, C_RESET);
    printf("%s%s  🌸 SYSBOX BIOS SETUP UTILITY — v5.5 ✨  [%s]    [%s]%s",
           th->header_bg, th->header_fg, th->theme_name, time_str, C_RESET);
    printf("\033[%d;%dH%s│%s", start_y + 1, start_x + box_w - 1, th->border, C_RESET);

    // 2. LINHA DE ABAS (Tabs)
    printf("\033[%d;%dH%s│%s  ", start_y + 2, start_x, th->border, C_RESET);
    for (int t = 0; t < TOTAL_TABS; t++) {
        if (t == cur_tab) {
            printf("%s%s %s %-7s %s  ", th->tab_sel_bg, th->tab_sel_fg, tabs[t].tab_icon, tabs[t].tab_name, C_RESET);
        } else {
            printf("%s %s %-7s %s  ", th->accent_sky, tabs[t].tab_icon, tabs[t].tab_name, C_RESET);
        }
    }
    printf("\033[%d;%dH%s│%s", start_y + 2, start_x + box_w - 1, th->border, C_RESET);

    // Divisória horizontal
    printf("\033[%d;%dH%s├", start_y + 3, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("┤%s", C_RESET);

    // 3. CORPO CENTRAL: 2 Colunas
    int split_x = (box_w * 44) / 100;
    int body_h = box_h - 6;
    int visible_items = body_h - 2;
    if (visible_items < 3) visible_items = 3;

    MenuTab *tab = &tabs[cur_tab];

    if (cur_item < scroll_offset) scroll_offset = cur_item;
    if (cur_item >= scroll_offset + visible_items) scroll_offset = cur_item - visible_items + 1;

    MenuEntry *sel = &tab->items[cur_item];

    char desc_lines[3][128];
    int right_content_w = box_w - split_x - 5;
    wrap_text(sel->description, desc_lines, right_content_w);

    for (int r = 0; r < body_h; r++) {
        int line_y = start_y + 4 + r;

        printf("\033[%d;%dH%s│%*s│%*s│%s",
               line_y, start_x,
               th->border, split_x - 1, "",
               box_w - split_x - 2, "",
               C_RESET);

        // --- COLUNA ESQUERDA ---
        if (r == 0 && scroll_offset > 0) {
            printf("\033[%d;%dH%s   ▲ Mais itens acima...%s", line_y, start_x + 2, th->accent_peach, C_RESET);
        } else if (r == visible_items + 1 && (scroll_offset + visible_items < tab->item_count)) {
            printf("\033[%d;%dH%s   ▼ Mais itens abaixo...%s", line_y, start_x + 2, th->accent_peach, C_RESET);
        } else {
            int item_idx = scroll_offset + (r > 0 ? r - 1 : r);
            if (item_idx >= 0 && item_idx < tab->item_count && (r - 1 < visible_items || tab->item_count <= visible_items)) {
                if (tab->item_count <= visible_items) item_idx = r;
                if (item_idx < tab->item_count) {
                    MenuEntry *e = &tab->items[item_idx];
                    int is_sel = (item_idx == cur_item);

                    printf("\033[%d;%dH", line_y, start_x + 2);
                    if (is_sel) {
                        printf("%s%s %s %-12.12s ◄ %s", th->item_sel_bg, th->item_sel_fg, e->icon, e->name, C_RESET);
                    } else {
                        printf("%s %s %-12.12s   %s", th->text_main, e->icon, e->name, C_RESET);
                    }
                }
            }
        }

        // --- COLUNA DIREITA ---
        int right_start_x = start_x + split_x + 2;
        printf("\033[%d;%dH", line_y, right_start_x);

        if (r == 0) {
            printf("%s[ 🌸 Informações da Ferramenta ]%s", th->accent_peach, C_RESET);
        } else if (r == 1) {
            printf("%s• Nome      :%s %s%-16.16s%s", th->accent_sky, C_RESET, th->accent_pink, sel->name, C_RESET);
        } else if (r == 2) {
            printf("%s• Categoria :%s %s%-16.16s%s", th->accent_sky, C_RESET, th->accent_mint, tab->tab_name, C_RESET);
        } else if (r == 3) {
            printf("%s• Resumo    :%s %s%-32.32s%s", th->accent_sky, C_RESET, th->text_main, sel->tagline, C_RESET);
        } else if (r == 4 && strlen(desc_lines[0]) > 0) {
            printf("%s%-44.44s%s", th->text_main, desc_lines[0], C_RESET);
        } else if (r == 5 && strlen(desc_lines[1]) > 0) {
            printf("%s%-44.44s%s", th->text_main, desc_lines[1], C_RESET);
        } else if (r == 6 && strlen(sel->example) > 0) {
            printf("%s• Exemplo   :%s %s$ %-30.30s%s", th->accent_sky, C_RESET, th->accent_peach, sel->example, C_RESET);
        } else if (r == 7) {
            printf("%s[ ⚡ Telemetria & Memória SHM (.SO) ]%s", th->accent_peach, C_RESET);
        } else if (r == 8) {
            printf("%s• Blocos SHM:%s ", th->accent_sky, C_RESET);
            for (int m = 0; m < 16; m++) {
                uint8_t st = ipc_data.mem_slots[m];
                if (st == MEM_SLOT_PINNED) printf("\033[1;35m▣\033[0m");
                else if (st == MEM_SLOT_KERNEL) printf("\033[1;34m■\033[0m");
                else if (st == MEM_SLOT_ACTIVE) printf("\033[1;32m■\033[0m");
                else if (st == MEM_SLOT_CACHE)  printf("\033[1;33m▲\033[0m");
                else printf("\033[0;90m░\033[0m");
            }
            printf(" ");
            for (int m = 16; m < 32; m++) {
                uint8_t st = ipc_data.mem_slots[m];
                if (st == MEM_SLOT_ACTIVE) printf("\033[1;32m■\033[0m");
                else if (st == MEM_SLOT_CACHE) printf("\033[1;33m▲\033[0m");
                else printf("\033[0;90m░\033[0m");
            }
        } else if (r == 9) {
            printf("%s• Última IPC:%s %s%-12.12s%s (PID: %d | %u calls)",
                   th->accent_sky, C_RESET, th->accent_mint,
                   strlen(ipc_data.last_writer) > 0 ? ipc_data.last_writer : "core", C_RESET,
                   ipc_data.last_caller_pid, ipc_data.total_ipc_calls);
        } else if (r == 10) {
            printf("%s• Ação .SO  :%s %s%-34.34s%s",
                   th->accent_sky, C_RESET, th->text_main,
                   strlen(ipc_data.last_action) > 0 ? ipc_data.last_action : "Pronto", C_RESET);
        } else if (r == body_h - 2) {
            const char *mascot = mascots[(cur_item + cur_tab) % 5];
            printf("%s🐾 %s  [ Enter: Run | 'A': Args ]%s", th->accent_pink, mascot, C_RESET);
        }
    }

    // 4. RODAPÉ DE NAVEGAÇÃO
    printf("\033[%d;%dH%s├", start_y + 4 + body_h, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("┤%s", C_RESET);

    printf("\033[%d;%dH%s│%s", start_y + 5 + body_h, start_x, th->border, C_RESET);
    printf("%s%s  [←/→/TAB] Abas | [↑/↓] Rolar Lista | [Enter] Rodar | ['A'] Args | ['T'] Tema | [Q] Sair %s",
           th->header_bg, th->header_fg, C_RESET);
    printf("\033[%d;%dH%s│%s", start_y + 5 + body_h, start_x + box_w - 1, th->border, C_RESET);

    printf("\033[%d;%dH%s╰", start_y + 6 + body_h, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╯%s", C_RESET);

    fflush(stdout);
}

int main(void) {
    utilipc_init();
    enable_raw_mode();

    while (keep_running) {
        render_bios_setup();

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };

        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            char seq[8];
            ssize_t n = read(STDIN_FILENO, seq, sizeof(seq));
            if (n <= 0) break;

            char c = seq[0];

            if (c == 'q' || c == 'Q') {
                break;
            }

            if (c == 't' || c == 'T') {
                cur_theme_idx = (cur_theme_idx + 1) % THEME_COUNT;
                continue;
            }

            if (c == 'a' || c == 'A') {
                prompt_custom_arguments();
                continue;
            }

            if (c == '\t') {
                cur_tab = (cur_tab + 1) % TOTAL_TABS;
                cur_item = 0;
                scroll_offset = 0;
                continue;
            }

            if (c == '\r' || c == '\n') {
                launch_tool(tabs[cur_tab].items[cur_item].cmd, NULL);
                continue;
            }

            // Sequência de Escape / Setas do Teclado
            if (c == 27) {
                if (n == 1) { // Tecla ESC sozinha
                    if (cur_tab != TOTAL_TABS - 1) {
                        cur_tab = TOTAL_TABS - 1;
                        cur_item = 0;
                        scroll_offset = 0;
                    } else {
                        break;
                    }
                    continue;
                }

                if (n >= 3 && seq[1] == '[') {
                    if (seq[2] == 'A') { // Seta CIMA
                        if (cur_item > 0) cur_item--;
                        else {
                            cur_item = tabs[cur_tab].item_count - 1;
                        }
                    } else if (seq[2] == 'B') { // Seta BAIXO
                        if (cur_item + 1 < tabs[cur_tab].item_count) cur_item++;
                        else {
                            cur_item = 0;
                            scroll_offset = 0;
                        }
                    } else if (seq[2] == 'C') { // Seta DIREITA
                        cur_tab = (cur_tab + 1) % TOTAL_TABS;
                        cur_item = 0;
                        scroll_offset = 0;
                    } else if (seq[2] == 'D') { // Seta ESQUERDA
                        cur_tab = (cur_tab - 1 + TOTAL_TABS) % TOTAL_TABS;
                        cur_item = 0;
                        scroll_offset = 0;
                    }
                }
            }
        }
    }

    return 0;
}
