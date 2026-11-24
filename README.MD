# Gerenciador de Memória Virtual

Implementação em C de tradução de endereços virtuais, com 128 frames, TLB e
algoritmos de substituição FIFO ou LRU. Desenvolvida e testada em Linux; usa
threads POSIX (`pthreads`) para buscar entradas na TLB.

## Arquivos C

- `main.c`: valida os argumentos, carrega os endereços e grava as traduções e
  estatísticas em `correct.txt`.
- `vm.c`: implementa a memória virtual, a tradução de endereços, a busca
  paralela na TLB e a substituição de páginas e entradas da TLB.
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

## Testar

Use o arquivo de exemplo:

```sh
./vm addresses.txt lru fifo
```

Para testar as combinações dos algoritmos, execute também:

```sh
./vm addresses.txt fifo fifo
./vm addresses.txt fifo lru
./vm addresses.txt lru lru
```

As traduções e estatísticas são gravadas em `correct.txt`. Para recompilar do
zero, execute `make clean` e depois `make`.