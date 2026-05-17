# Estatísticas reais do alocador

Todas as métricas extraídas correndo o `myProg` em batch mode + opção 8
do menu (para contar arestas do grafo de interferências).

> **Edges** = número de arestas do grafo *antes* de qualquer split/spill
> (i.e. as interferências detectadas no grafo inicial).
> **Splits**/**Spills** = número de webs partidas / despejadas pelo
> algoritmo até conseguir colorir.

## Datasets standard (todos com algoritmo `basic`)

| Caso              | Webs | Edges | Registos | Splits | Spills | Estado |
|-------------------|:----:|:-----:|:--------:|:------:|:------:|:------:|
| `ranges1` + K=2   |  3   |  1    |  2       |  0     |  0     | OK     |
| `ranges2` + K=2   |  2   |  1    |  2       |  0     |  0     | OK     |
| `ranges3` + K=2   |  2   |  1    |  2       |  0     |  0     | OK     |
| `ranges4` + K=1   |  4   |  0    |  1       |  0     |  0     | OK     |
| `ranges5` + K=1   |  3   |  0    |  1       |  0     |  0     | OK     |
| `ranges6` + K=3   |  5   |  5    |  3       |  0     |  0     | OK     |

**Notas:**
- `ranges4` e `ranges5` têm **0 arestas** porque todas as variáveis se
  sucedem com *clean handoffs* (a- seguido de b+). Por isso 1 registo
  chega.
- `ranges6` é um **ciclo de 5 webs** (cromático = 3) → precisa de 3
  registos.

## Cenários forçados: 5-ciclo `ranges6` com K=2

| Algoritmo            | Webs | Edges | Registos | Splits | Spills | Estado     |
|----------------------|:----:|:-----:|:--------:|:------:|:------:|:----------:|
| `basic`              |  5   |  5    |  0       |  0     |  0     | INFEASIBLE |
| `spilling, 1`        |  5   |  5    |  2       |  0     |  1     | OK         |
| `free`               |  5   |  5    |  2       |  0     |  1     | OK         |

Aqui `free` faz exactamente o mesmo que `spilling`, porque nenhuma web
de `ranges6` tem múltiplas sub-webs — não há nada para partir.

## Cenário criado para forçar splitting: `ranges_split_clean` (K=2)

Este input é uma variante do `ranges6` em que a variável `e` aparece
em duas linhas que se fundem (`e: 8+,9-` + `e: 9+,10,11-`). Antes da
fusão são 6 sub-webs; depois da fusão sobram 5 webs num 5-ciclo.

| Algoritmo            | Webs | Edges | Registos | Splits | Spills | Estado     |
|----------------------|:----:|:-----:|:--------:|:------:|:------:|:----------:|
| `basic`              |  5   |  5    |  0       |  0     |  0     | INFEASIBLE |
| `spilling, 1`        |  5   |  5    |  2       |  0     |  1     | OK         |
| `splitting, 1`       |  6   |  5    |  2       |  1     |  0     | OK         |
| `free`               |  6   |  5    |  2       |  1     |  0     | OK         |

**Observações chave para a apresentação:**

1. `basic` falha (cromático 3, só temos 2 registos).
2. `spilling` resolve **a custo de memória** (despeja 1 web → 1 load/store
   em cada uso dessa variável em runtime).
3. `splitting` resolve **sem tocar em memória**: parte `e` nas suas 2
   sub-webs, o que reduz o 5-ciclo a uma cadeia bipartida (cromático 2).
4. `free` escolhe sozinho a alternativa mais barata — *splitting* sem
   nenhum spill. Compara com `ranges6` onde o mesmo `free` cai no spill
   porque não há nada para partir.

## Como reproduzir

```bash
# A tabela acima é gerada por um one-liner shell que invoca o myProg
# em batch mode para webs/registers/splits/spills e o menu opcao 8
# para a contagem de arestas (linha "total edges: N" no output ASCII).

./myProg -b ranges/ranges6.txt registers/free_K2.txt out.txt
#   Allocation: FEASIBLE (free: 0 split(s), 1 spill(s))
#   webs: 5    registers: 2

./myProg -b ranges/ranges_split_clean.txt registers/free_K2.txt out.txt
#   Allocation: FEASIBLE (free: 1 split(s), 0 spill(s))
#   webs: 6    registers: 2
```
