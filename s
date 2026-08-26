#!/bin/bash

echo "[*] 1/3 - Criando exemplo.py na raiz do repositório..."

cat << 'EOF' > exemplo.py
# Exemplo de Script Python para Transpilar para C Nativo

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

print("=== Teste de Execucao pythont ===")

total = 0
for i in range(1, 11):
    total += i

print("Soma de 1 a 10:", total)
print("Fibonacci de 10:", fibonacci(10))

for step in range(0, 15, 3):
    print("Contador com passo 3:", step)
EOF

echo "[*] 2/3 - Criando exemplo2.py com Listas e Built-ins..."

cat << 'EOF' > exemplo2.py
# Exemplo Avançado de Python para o pythont 2.0

print("=== Testando pythont 2.0 (Listas & Built-ins) ===")

numeros = [10, 20, 30, 40, 50]
print("Primeiro elemento:", numeros[0])
print("Terceiro elemento:", numeros[2])

numeros[2] = 99
print("Terceiro elemento modificado:", numeros[2])

soma = 0
for i in range(1, 6):
    soma += i
print("Soma com += (1 a 5):", soma)

fatorial = 1
for x in range(1, 6):
    fatorial *= x
print("Fatorial de 5 com *= :", fatorial)

val_a = 42
val_b = 100
val_neg = -500

print("Maximo entre 42 e 100:", max(val_a, val_b))
print("Minimo entre 42 e 100:", min(val_a, val_b))
print("Valor absoluto de -500:", abs(val_neg))

print("Contando com break:")
for n in range(1, 10):
    if n == 4:
        break
    print("Contagem:", n)
EOF

echo "[*] 3/3 - Atualizando .github/workflows/ci.yml com criação dinâmica de testes..."
mkdir -p .github/workflows

cat << 'EOF' > .github/workflows/ci.yml
name: sys-in-c CI Pipeline

on:
  push:
    branches: [ main, master ]
  pull_request:
    branches: [ main, master ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc, clang]

    steps:
    - name: Checkout Código-Fonte
      uses: actions/checkout@v3

    - name: Configurar Dependências de Compilação
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential ${{ matrix.compiler }} libpthread-stubs0-dev

    - name: Compilar Todo o Projeto
      run: |
        make clean
        make CC=${{ matrix.compiler }}

    - name: Testar Execução de Utilitários Principais
      run: |
        echo "=== 1. Testando Calc ==="
        ./calc "10 + 20 * 2"

        echo "=== 2. Testando Base64 ==="
        ./b64 -e "sys-in-c"
        ./b64 -d "c3lzLWluLWM="

        echo "=== 3. Testando Hashcalc ==="
        ./hashcalc -s "sys-in-c test"

        echo "=== 4. Testando SNC ==="
        ./snc 192.168.1.100/24

        echo "=== 5. Testando Pythont ==="
        echo 'print("Hello do GitHub Actions via Pythont!", 40 + 2)' > ci_test.py
        ./pythont ci_test.py
        if [ -f exemplo.py ]; then ./pythont exemplo.py; fi
        if [ -f exemplo2.py ]; then ./pythont exemplo2.py; fi

        echo "=== 6. Testando Low-Utils (whoami, chmod, magic, stat, tree, df) ==="
        ./whoami -s
        ./whoami -uo
        ./chmod 755 Makefile
        ./magic Makefile
        ./stat Makefile
        ./tree -L 2
        ./df

        echo "=== 7. Testando Bytebeat ==="
        ./bytebeat -p 2 -d 2 -o test_symphony.wav
        ./bytebeat -p 9 -d 2 -o test_snes.wav

        echo "=== 8. Testando Bench & Diskbench ==="
        ./bench
        ./diskbench
EOF

echo ""
echo "[✔] Corrigido! Agora você pode dar git add, commit e push:"
git add .
git commit -m "Adiciona suporte multiplataforma, CI com GitHub Actions e pythont"
git push
