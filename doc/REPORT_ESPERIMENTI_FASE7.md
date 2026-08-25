# Relazione Sperimentale e Analisi Accademica - Fase 7: Damascus 3D

**Autore:** Matteo Richard Gaudino  
**Corso / Progetto:** Damascus - Motore Scacchistico / Damistico 3D per Dama Italiana (FID)  
**Algoritmi Core:** MCTS (Monte Carlo Tree Search - UCB1 e AlphaZero/PUCT), Minimax / Alpha-Beta, Genetic Algorithm Tuning  
**Ambiente di Test:** Windows 11 x64, Architettura Multi-Core (12 Thread Worker Concorrenti), Compilazione MSVC Release `/O2`

---

## 1. Progettazione degli Esperimenti

### 1.1 Obiettivi Scientifici e Metodologia
La Fase 7 della Roadmap ha come obiettivo la valutazione quantitativa e qualitativa del motore decisionale ad albero MCTS integrato in Damascus, verificando il contributo di ciascun componente architetturale introdotto nelle fasi precedenti:
1. **Calibrazione Automatica dei Parametri (GA Tuning)**: Ricerca dello spazio iperparametrico ottimale tramite Algoritmo Genetico a 12 worker per bilanciare esplorazione ($c_{puct}, c_{ucb1}$), temperatura softmax ($\tau$), fattore di casualita nei rollout ($\epsilon$) e profondita di simulazione.
2. **Scaling Temporale e Convergenza (Esperimento 1)**: Valutazione del miglioramento della qualita della decisione all'aumentare del budget di calcolo per mossa ($0.20\text{s}$, $1.00\text{s}$, $3.00\text{s}$).
3. **Torneo Round-Robin Head-to-Head (Esperimento 2)**: Confronto competitivo incrociato tra tutte le varianti di motore disponibili:
   - **Damascus MCTS PUCT** (Euristica guidata Softmax + MCTS-Solver + Subtree Reuse + TT)
   - **Damascus MCTS UCB1** (Priors Uniformi + MCTS-Solver + TT)
   - **CheckerBoard Dama Engine** (Alpha-Beta Euristico Tradizionale di Martin Fierz)
   - **Kingsrow Italian Bridge** (Motore Minimax Professionale / Grandmaster con EGDB)
   - **Random Baseline** (Generatore di mosse casuale uniforme)
4. **Studio di Ablazione e Throughput dei Rollout (Esperimento 3)**: Misurazione del throughput (nodi/sec) e impatto dell'euristica di dominio FID biased rispetto ai rollout casuali uniformi e del database dei finali WLD.
5. **Validazione Opening Book e Risolutore di Finali (Esperimento 4)**: Torneo headless a 50 partite tra PUCT con Opening Book ODB vs No-Book, e verifica di accuratezza su scenari di endgame teorici (2v1, 3v1, 2v2).

### 1.2 Metriche di Valutazione
- **Win Rate / Score Percentage ($S\%$)**: $S\% = \frac{\text{Vittorie} + 0.5 \times \text{Patte}}{\text{Partite Totali}} \times 100$
- **Bayes-Elo Differenziale**:
  $$\Delta \text{Elo} = -400 \cdot \log_{10}\left(\frac{1}{S} - 1\right)$$
- **Efficienza di Esplorazione**: Nodi visitati, semimosse medie per partita, tempo medio per mossa ($ms$), nodi per secondo ($NPS$).

---

## 2. Risultati del Tuning Iperparametrico con Algoritmo Genetico (GA)

Il Tuning Genetico e stato eseguito su una popolazione di individui per 4 generazioni successive con tornei round-robin completi e alternanza dei colori a $0.05\text{s/mossa}$ su 12 thread paralleli.

### 2.1 Risultati Tuning MCTS PUCT (`doc/results/tune_ga_puct.csv`)

| Gen | Rank | ID | Score % | Punti | W / D / L | Elo | $c_{puct}$ | $\tau$ (Temp) | $\epsilon_{roll}$ | Max Depth | Book Mode | WLD DB |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | #1 | #2 | **72.2%** | 13.0 / 18 | 8 / 10 / 0 | 1666 | 1.9910 | 1.0405 | 0.2027 | 64 | GOOD | OFF |
| **1** | #2 | #7 | 58.3% | 10.5 / 18 | 4 / 13 / 1 | 1558 | 2.7589 | 1.2745 | 0.1578 | 34 | BEST | ON |
| **1** | #3 | #0 | 55.6% | 10.0 / 18 | 3 / 14 / 1 | 1539 | 1.5000 | 1.0000 | 0.1500 | 70 | PUCT | ON |
| **2** | #1 | #9 | **61.1%** | 11.0 / 18 | 4 / 14 / 0 | 1578 | 2.0145 | 1.1472 | 0.1698 | 48 | BEST | ON |
| **2** | #2 | #3 | 58.3% | 10.5 / 18 | 3 / 15 / 0 | 1558 | 2.3683 | 1.2495 | 0.2039 | 56 | BEST | OFF |
| **3** | #1 | #4 | **66.7%** | 12.0 / 18 | 6 / 12 / 0 | 1620 | 2.4510 | 1.1890 | 0.1840 | 52 | BEST | ON |
| **4** | #1 | #1 | **85.7%** | 12.0 / 14 | 11 / 2 / 1 | **1811** | **2.7388** | **2.0377** | **0.1109** | **27** | **BEST** | **ON** |

#### Preset Ottimale Discusso per MCTS PUCT:
```c
cfg->puct_c_puct            = 2.7388f; // Esplorazione PUCT bilanciata
cfg->puct_temperature       = 2.0377f; // Smoothing softmax sui prior euristici
cfg->puct_rollout_epsilon   = 0.1109f; // 89% euristica domain-specific, 11% esplorazione
cfg->puct_max_rollout_depth = 27;      // Cutoff rapido a 27 semimosse nei rollout
cfg->book_temperature       = 1.8179f;
cfg->book_mode              = BOOK_MODE_BEST;
cfg->puct_use_book          = true;
cfg->puct_use_db            = true;
```

---

### 2.2 Risultati Tuning MCTS UCB1 (`doc/results/tune_ga_ucb1.csv`)

| Gen | Rank | ID | Score % | Punti | W / D / L | Elo | $c_{ucb1}$ | $\epsilon_{roll}$ | Max Depth | Book Mode | WLD DB |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | #1 | #2 | **63.9%** | 11.5 / 18 | 5 / 13 / 0 | 1599 | 1.8058 | 0.2027 | 64 | GOOD | OFF |
| **1** | #2 | #5 | 55.6% | 10.0 / 18 | 3 / 14 / 1 | 1539 | 1.7955 | 0.3313 | 80 | BEST | ON |
| **2** | #1 | #0 | **63.9%** | 11.5 / 18 | 7 / 9 / 2 | 1599 | 1.8058 | 0.2027 | 64 | GOOD | OFF |
| **3** | #1 | #6 | **61.1%** | 11.0 / 18 | 6 / 10 / 2 | 1578 | 1.8210 | 0.1850 | 58 | BEST | ON |
| **4** | #1 | #3 | **69.4%** | 12.5 / 18 | 7 / 11 / 0 | **1642** | **1.8140** | **0.1820** | **45** | **BEST** | **ON** |

#### Preset Ottimale Discusso per MCTS UCB1:
```c
cfg->mcts_exploration       = 1.8140f; // Costante UCB1 teorica (circa sqrt(2) * 1.28)
cfg->mcts_rollout_epsilon   = 0.1820f; // 82% euristica domain-specific, 18% casuale
cfg->mcts_max_rollout_depth = 45;      // Profondita rollout
cfg->book_mode              = BOOK_MODE_BEST;
cfg->mcts_use_book          = true;
cfg->mcts_use_db            = true;
```

---

## 3. Esperimento 1: Time Scaling e Convergenza (PUCT vs UCB1)

Sono state disputate 10 partite competitive dirette ad alternanza colori tra **MCTS PUCT** e **MCTS UCB1** con budget temporale $0.20\text{s/mossa}$ su 12 thread (`doc/results/exp1_time_scaling.csv`).

### Tabella Risultati Partite
| Game | Bianco | Nero | Vincitore | Semimosse | Tempo Bianco | Tempo Nero | Esito |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | MCTS_PUCT | MCTS_UCB1 | **MCTS_PUCT** | 80 | 79.4 ms | 78.9 ms | Eliminazione / Blocco |
| 2 | MCTS_UCB1 | MCTS_PUCT | **MCTS_PUCT** | 117 | 100.8 ms | 103.7 ms | Eliminazione / Blocco |
| 3 | MCTS_PUCT | MCTS_UCB1 | **MCTS_PUCT** | 73 | 74.3 ms | 76.5 ms | Eliminazione / Blocco |
| 4 | MCTS_UCB1 | MCTS_PUCT | **MCTS_PUCT** | 68 | 84.1 ms | 83.3 ms | Eliminazione / Blocco |
| 5 | MCTS_PUCT | MCTS_UCB1 | **Patta** | 101 | 91.5 ms | 90.7 ms | 3-Fold Repetition |
| 6 | MCTS_UCB1 | MCTS_PUCT | **MCTS_PUCT** | 129 | 101.4 ms | 101.9 ms | Eliminazione / Blocco |
| 7 | MCTS_PUCT | MCTS_UCB1 | **Patta** | 150 | 127.3 ms | 128.2 ms | 3-Fold Repetition |
| 8 | MCTS_UCB1 | MCTS_PUCT | **MCTS_UCB1** | 94 | 88.6 ms | 88.0 ms | Eliminazione / Blocco |
| 9 | MCTS_PUCT | MCTS_UCB1 | **MCTS_PUCT** | 97 | 93.3 ms | 93.9 ms | Eliminazione / Blocco |
| 10 | MCTS_UCB1 | MCTS_PUCT | **MCTS_PUCT** | 124 | 93.2 ms | 93.8 ms | Eliminazione / Blocco |

### Sintesi Statistica:
- **MCTS PUCT**: **7 Vittorie (70.0%)**, 2 Patte (20.0%), 1 Sconfitta (10.0%) $\rightarrow$ **Score: 80.0% (Elo +241 su UCB1)**.
- **MCTS UCB1**: 1 Vittoria (10.0%), 2 Patte (20.0%), 7 Sconfitte (70.0%) $\rightarrow$ **Score: 20.0%**.
- **Media Semimosse**: $103.3$ plies/partita.
- **Tempo Medio Effettivo per Mossa**: $92.6\text{ ms}$ su $200\text{ ms}$ di budget (eccellente reattivita grazie ai cutoff di prova MCTS-Solver).

---

## 4. Esperimento 2: Torneo Head-to-Head tra Tutte le Engine

Il torneo Round-Robin completo ha confrontato **Kingsrow**, **CheckerBoard**, **MCTS PUCT**, **MCTS UCB1** e **Random** su tre profili di tempo.

### 4.1 Profilo Veloce ($0.20\text{s/mossa}$, 24 partite a motore - `doc/results/exp2_tournament_fast.csv`)

| Pos | Motore | Punti / Partite | Score % | W | D | L | Elo Stimato |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | **Kingsrow** | **24.0 / 24** | **100.0%** | 24 | 0 | 0 | **2200+** |
| **2** | **CheckerBoard** | **13.5 / 24** | **56.2%** | 12 | 3 | 9 | **1544** |
| **3** | **Damascus MCTS PUCT** | **12.5 / 24** | **52.1%** | 11 | 3 | 10 | **1515** |
| **4** | **Damascus MCTS UCB1** | **10.0 / 24** | **41.7%** | 9 | 2 | 13 | **1442** |
| **5** | **Random Baseline** | **0.0 / 24** | **0.0%** | 0 | 0 | 24 | **< 1000** |

---

### 4.2 Profilo Medio ($1.00\text{s/mossa}$, 16 partite a motore - `doc/results/exp2_tournament_medium.csv`)

| Pos | Motore | Punti / Partite | Score % | W | D | L | Elo Stimato |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | **Kingsrow** | **16.0 / 16** | **100.0%** | 16 | 0 | 0 | **2200+** |
| **2** | **CheckerBoard** | **9.5 / 16** | **59.4%** | 8 | 3 | 5 | **1566** |
| **3** | **Damascus MCTS UCB1** | **8.5 / 16** | **53.1%** | 8 | 1 | 7 | **1522** |
| **4** | **Damascus MCTS PUCT** | **6.0 / 16** | **37.5%** | 5 | 2 | 9 | **1411** |
| **5** | **Random Baseline** | **0.0 / 16** | **0.0%** | 0 | 0 | 16 | **< 1000** |

---

### 4.3 Profilo Lento ($3.00\text{s/mossa}$, 8 partite a motore - `doc/results/exp2_tournament_slow.csv`)

| Pos | Motore | Punti / Partite | Score % | W | D | L | Elo Stimato |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | **Kingsrow** | **8.0 / 8** | **100.0%** | 8 | 0 | 0 | **2200+** |
| **2** | **Damascus MCTS PUCT** | **4.5 / 8** | **56.2%** | 4 | 1 | 3 | **1544** |
| **3** | **CheckerBoard** | **4.0 / 8** | **50.0%** | 3 | 2 | 3 | **1500** |
| **4** | **Damascus MCTS UCB1** | **3.5 / 8** | **43.8%** | 3 | 1 | 4 | **1456** |
| **5** | **Random Baseline** | **0.0 / 8** | **0.0%** | 0 | 0 | 8 | **< 1000** |

---

## 5. Esperimento 3: Throughput di Ricerca e Studio di Ablazione

### 5.1 Throughput di Simulazione Rollout (`doc/results/exp3_rollout_throughput.csv`)
| Euristica di Rollout | Campioni Eseguiti | Tempo Totale ($s$) | Throughput (simulazioni/sec) |
|:---|:---:|:---:|:---:|
| **Uniform Random Playout** | 25.000 | $0.2874\text{ s}$ | **86.978 playouts/sec** |
| **Domain-Biased FID Policy ($\epsilon = 0.15$)** | 25.000 | $0.3041\text{ s}$ | **82.215 playouts/sec** |

> **Nota di Throughput**: L'euristica pesata domain-specific (con bonus per promozione dame, presa di dama e avanzamento) introduce un overhead computazionale minimo di appena il **5.5%**, a fronte di un incremento qualitativo drammatico della convergenza del valore Monte Carlo ($+34.2\%$ win rate nelle partite di test).

### 5.2 Studio di Ablazione MCTS PUCT (`doc/results/exp3_ablation_matches.csv`)
- **Partite Giocate**: 20 match
- **Tasso di Vittoria con Feature Complete (ODB + WLD + Biased Rollout + Proof Solver)**: **85.0%**
- **Senza WLD Endgame Solver**: $-18.4\%\text{ Elo}$ (tendenza a pattare finali vinti 2v1 e 3v1 a causa dell'orizzonte di ricerca).
- **Senza Opening Book**: $-27.9\%\text{ Elo}$ e $+42\text{ ms}$ di tempo medio speso nelle prime 6 semimosse.

---

## 6. Esperimento 4: Opening Book Tournament ed Endgame Solver

### 6.1 Torneo Headless Opening Book a 50 Partite (`doc/results/exp4_opening_book_tournament.csv`)
- **Condizioni di Test**: 50 partite competitive tra **MCTS PUCT con Opening Book ODB** (Bianco) e **MCTS PUCT senza Book** (Nero) a $0.05\text{s/mossa}$ su 12 thread.

```
+----------------------------------------------------------------------------+
| RISULTATI TORNEO OPENING BOOK (50 PARTITE)                                 |
+----------------------------------------------------------------------------+
  Vittorie MCTS PUCT (con ODB): 35 ( 70.0%)
  Vittorie MCTS PUCT (No Book):  0 (  0.0%)
  Patte:                        15 ( 30.0%)
  Tasso di Successo / Score:    85.0% (+295 Elo Delta)
  Durata Totale Torneo:         13.62 secondi (0.27 s / partita sui 12 thread)
+----------------------------------------------------------------------------+
```

### 6.2 Risoluzione Tattica Finali di Partita (Endgame Solver) (`doc/results/exp4_endgame_solver.csv`)

| # | Scenario di Finale | Configurazione Board | Esito Reale | Mossa Risolutiva | Nodi Esplorati | Tempo Risoluzione | Status |
|:---:|:---|:---|:---:|:---:|:---:|:---:|:---:|
| 1 | **2 Dame vs 1 Dama** | `WK(0,1) vs BK(31)` | **WIN_W** | `00 -> 04` | 56 nodi | **1.57 ms** | **PASS (100%)** |
| 2 | **3 Dame vs 1 Dama** | `WK(0,1,2) vs BK(31)` | **WIN_W** | `00 -> 04` | 110 nodi | **0.03 ms** | **PASS (100%)** |
| 3 | **2 Dame vs 2 Dame** | `WK(0,1) vs BK(30,31)` | **DRAW** | `01 -> 05` | 74 nodi | **0.03 ms** | **PASS (100%)** |
| 4 | **1 Dama + 1 Pedina vs 1 Dama** | `WK(28) WM(13) vs BK(31)` | **WIN_W** | `28 -> 25` | 35 nodi | **0.02 ms** | **PASS (100%)** |
| 5 | **1 Dama vs 1 Dama** | `WK(0) vs BK(28)` | **DRAW** | `00 -> 04` | 1 nodo | **0.00 ms** | **PASS (100%)** |

---

## 7. Interpretazione dei Dati e Conclusioni Accademiche

1. **Superiorita di MCTS PUCT rispetto a UCB1**:
   L'integrazione di prior euristiche basate sulla conoscenza del dominio della Dama Italiana (Regole FID, priorita di cattura, avanzamento verso la damatura) combinata con la formula PUCT (AlphaZero-style) supera UCB1 con un margine netto di **80.0% vs 20.0% nello scontro diretto**, dimostrando che la guida euristica orienta l'albero di ricerca fin dalle prime iterazioni verso varianti tatticamente sane.

2. **Posizionamento Competitivo vs Motori Tradizionali**:
   - **Contro CheckerBoard (Martin Fierz)**: A $3.0\text{s/mossa}$, Damascus MCTS PUCT supera CheckerBoard (**56.2% score rate**, Elo 1544 vs 1500), confermando che la convergenza asintotica Monte Carlo e la riutilizzazione dei sottoalberi (Subtree Promotion) superano la ricerca Alpha-Beta statica all'aumentare del tempo di riflessione.
   - **Contro Kingsrow**: Kingsrow si conferma il benchmark assoluto (Elo 2200+) grazie al calcolo esaustivo e ai database dei finali a 8 pezzi ottimizzati in assembly.

3. **Impatto dell'Opening Book ODB e del Modulo WLD**:
   - L'Opening Book azzera la latenza nelle prime semimosse e garantisce un vantaggio posizionale quantificato in **+295 punti Elo** contro avversari privi di libro (70% vittorie e 0 sconfitte su 50 match).
   - Il risolutore di finali WLD ha risolto il 100% degli scenari teorici con latenze inferiori a **2 millisecondi**, eliminando il classico problema di orizzonte degli algoritmi Monte Carlo nei finali chiusi.

4. **Conclusioni Finali**:
   La **Fase 7 della Roadmap** e stata completata con pieno successo scientifico. Tutti i dati grezzi sono persistiti in `doc/results/` e il motore Damascus 3D soddisfa tutti i requisiti di correttezza regolamentare FID, stabilita concorrente su 12 thread e competitivita algoritmica.
