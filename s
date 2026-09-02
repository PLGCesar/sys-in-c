echo "[*] Criando pasta realeases e subpasta pythont-realese-1.0v..."

mkdir -p realeases/pythont-realese-1.0v

echo "[*] Copiando src/pythont/pythont.c para realeases/pythont-realese-1.0v/pythont-1.0v.c..."
cp src/pythont/pythont.c realeases/pythont-realese-1.0v/pythont-1.0v.c

if [ -f src/pythont/pythont_doc.txt ]; then
    cp src/pythont/pythont_doc.txt realeases/pythont-realese-1.0v/
fi

echo "[*] Criando Makefile proprio em realeases/Makefile..."

cat << 'EOF' > realeases/Makefile
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -fPIC -I../src -I..
LDFLAGS ?= -L.. -lutilipc -Wl,-rpath,.. -Wl,-rpath,. -lpthread -lm

TARGET = pythont-1.0v
SRC = pythont-realese-1.0v/pythont-1.0v.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
EOF

echo "[*] Compilando a release 1.0v via realeases/Makefile..."
make -C realeases

echo "[✔] Estrutura criada com sucesso!"
echo "[*] Testando o binario gerado na pasta realeases..."
./realeases/pythont-1.0v examples/exemplo_1_0.py
