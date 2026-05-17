# Estrutura das classes — Projeto II (DA, Spring 2026)

O projecto usa **structs simples + funções livres** (o estilo idiomático
dos TPs). A única classe verdadeira é o `Graph<T>` fornecido nas aulas
— tudo o resto são tipos de dados imutáveis manipulados pelos módulos
`parser`, `web` e `allocator`.

---

## Módulos principais

| Conceito          | No código                                                            | Ficheiro                              |
|-------------------|----------------------------------------------------------------------|---------------------------------------|
| **Graph**         | `Graph<T>` (template do TP, usado como `Graph<int>` — info = id da web) | `include/data_structures/Graph.h`     |
| **LiveRange**     | `LiveRange` (struct) — uma linha do ficheiro `ranges`                | `include/types.h`                     |
| **Web**           | `Web` (struct) + `SubWeb` (atómica)                                  | `include/web.h`                       |
| **Allocator**     | funções livres: `allocateBasic`, `allocateSpilling`, `allocateSplitting`, `allocateFree` | `include/allocator.h` |
| **Parser**        | funções livres: `parseRanges`, `parseRegisters`                      | `include/parser.h`                    |

---

## Definições

### Input do parser

```cpp
struct ProgramPoint {
    int  line;
    bool isDef;       // marcador '+'
    bool isLastUse;   // marcador '-'
};

struct LiveRange {
    std::vector<ProgramPoint> points;
};

struct Variable {
    std::string name;
    std::vector<LiveRange> ranges;
};
```

### SubWeb e Web (depois da fusão)

```cpp
struct SubWeb {                       // unidade atomica (1 LiveRange)
    int id;
    std::string varName;
    int varIndex, rangeIndex;
    std::vector<WebPoint> points;
};

struct Web {                          // grupo fundido de SubWebs
    int id;
    std::vector<int> subWebIds;       // indices das SubWebs que compoem esta Web
    std::vector<WebPoint> points;     // uniao dos pontos, ordenada
};
```

### Resultado da alocação

```cpp
struct WebAssignment {
    int webId;
    int registerId;   // -1 = spilled (M)
};

struct AllocationResult {
    bool feasible;
    std::vector<Web> webs;
    int registersUsed;
    std::vector<WebAssignment> assignments;
    std::string message;              // ex: "free: 1 split(s), 0 spill(s)"
};
```

### Grafo de interferências

Usado tal como vem do TP, sem alterações:

```cpp
Graph<int> g = buildInterferenceGraph(webs);
//        ^^^ info = id da web (0..N-1)
```

---

## Decisões de design (a destacar na slide)

- **Structs em vez de classes OO**: Os tipos são puros *plain-data*; toda a
  lógica fica em funções livres por módulo (`parser.h`, `web.h`,
  `allocator.h`). Isto torna cada etapa do pipeline testável de forma
  independente.

- **`SubWeb` como camada atómica**: Cada linha do ficheiro de input vira
  uma `SubWeb`. A fusão (union-find sobre sub-webs da mesma variável que
  partilham um ponto do programa) produz `Web`s. Manter as `SubWeb`s
  acessíveis é essencial para o **splitting** (T2.3 e T2.4), que reverte
  a fusão.

- **Spilling fica fora de `Web`**: A informação "esta web foi spillada"
  vive em `WebAssignment::registerId == -1`. Isto permite que a mesma
  `Web` seja reutilizada em algoritmos diferentes (basic / spilling /
  splitting / free), sem mutações.

- **`Graph<int>`**: A informação do vértice é apenas o id da web. O resto
  (pontos, sub-webs, etc.) fica nas estruturas paralelas — o grafo do TP
  serve só como *estrutura de adjacência*.

---
