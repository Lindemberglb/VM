# Gerenciador de Memória Virtual

Implementação em C de tradução de endereços virtuais, com até 256 frames,
algoritmos de substituição FIFO ou LRU e modos com TLB (busca serial ou com
threads POSIX) e sem TLB. Por padrão, usa 128 frames e busca paralela na TLB.

## Arquivos C

- `main.c`: valida os argumentos, carrega os endereços e grava as traduções e
  estatísticas em `correct.txt`.
- `vm.c`: implementa a memória virtual, a tradução de endereços, as buscas
  serial e paralela na TLB e a substituição de páginas e entradas da TLB.
- `vm.h`: declara as estruturas, constantes e funções compartilhadas pelos
  arquivos C.

## Compilar e executar

Com `make` instalado, compile na pasta do projeto:

```sh
make
```

O comando cria o executável `vm`. Execute-o passando o arquivo de endereços,
o algoritmo de substituição de páginas e o algoritmo da TLB:

```sh
./vm addresses.txt lru fifo
```

O exemplo usa LRU para páginas e FIFO para a TLB. O programa também requer
`BACKING_STORE.bin` na pasta atual e grava o resultado em `correct.txt`.
Opções adicionais permitem escolher 128 ou 256 frames, desativar a TLB ou
fazer a busca serial:

```sh
# Sem TLB, 256 frames
./vm addresses.txt fifo fifo --no-tlb --frames 256

# Sem TLB, 128 frames com FIFO ou LRU
./vm addresses.txt fifo fifo --no-tlb
./vm addresses.txt lru fifo --no-tlb

# TLB sem threads, com FIFO ou LRU
./vm addresses.txt lru fifo --serial-tlb
./vm addresses.txt lru lru --serial-tlb
```

Quando a TLB está desativada, o campo `TLB` de cada tradução é `-1`.

## Testar

Use o arquivo de exemplo:

```sh
./vm addresses.txt lru fifo
```

Para testar as combinações dos algoritmos com a busca paralela padrão, execute
também:

```sh
./vm addresses.txt fifo fifo
./vm addresses.txt fifo lru
./vm addresses.txt lru lru
```

As traduções e estatísticas são gravadas em `correct.txt`. Para recompilar do
zero, execute `make clean` e depois `make`.