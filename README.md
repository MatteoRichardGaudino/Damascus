# Damascus ♟️

**Damascus** è un'applicazione 3D del gioco della **Dama Italiana**, scritta completamente in **C puro (C11)** utilizzando **OpenGL**, **GLFW**, **cglm** e **Nuklear**. 

Progettato per essere performante, moderno e totalmente multipiattaforma (macOS & Windows), Damascus include un'architettura ad alte prestazioni per engine AI ed un'interfaccia 3D interattiva.

---

## 🚀 Caratteristiche Principali

- **100% C Puro**: Nessuna dipendenza C++ o altri linguaggi.
- **Grafica 3D OpenGL**:
  - Scacchiera 3D e pezzi tridimensionali (pedine e dame).
  - Supporto preparato per Coordinate UV e Texture mappabili nei shader.
  - I pezzi mangiati vengono posizionati ordinatamente lungo i bordi esterni della scacchiera.
- **Controlli Interattivi**:
  - **Click Sinistro**: Selezione e spostamento intuitivo dei pezzi mediante Raycasting 3D.
  - **Click Destro + Trascinamento**: Rotazione fluida della scena (Telecamera Arcball 3D).
  - **Ridimensionamento Finestra**: Mantiene automaticamente l'aspect ratio e le giuste proporzioni grafiche.
- **Modalità di Gioco**:
  - **2 Players**: Giocatore vs Giocatore in locale.
  - **1 Player**: Giocatore vs CPU.
  - **CPU vs CPU**: Sfida automatica tra due engine di gioco.
- **Engine AI Efficienti & Stateful**:
  - Gli engine di gioco mantengono lo stato interno senza ricreare l'algoritmo o lo stato da zero ad ogni mossa.
  - Incluso l'engine **Random** di base (altri engine avanzati integrabili tramite l'interfaccia `Engine`).

---

## 🇮🇹 Regole della Dama Italiana

Il gioco segue rigorosamente le regole ufficiali della **Dama Italiana**:

### 1. La Scacchiera e i Pezzi
- Si gioca su una scacchiera 8x8 orientata in modo che la casella nell'angolo in basso a destra sia **scura**.
- Il gioco si svolge esclusivamente sulle **32 caselle scure**.
- Ciascun giocatore inizia con **12 pedine** (Bianche per chi inizia, Nere per l'avversario).

### 2. Movimento delle Pedine
- La **Pedina** si muove di 1 casella in avanti in diagonale verso una casella libera.
- La pedina **non può** muoversi all'indietro.
- Quando una pedina raggiunge la base avversaria (l'ultima riga in fondo), viene promossa a **Dama** (rappresentata da due pedine sovrapposte).

### 3. Movimento della Dama
- La **Dama** si muove di 1 casella in diagonale, sia in **avanti** che all'**indietro**.

### 4. Regole di Presa (Cattura)
- La presa è **OBBLIGATORIA**. Se un giocatore può mangiare, deve farlo.
- **La Pedina**:
  - Può mangiare solo in **avanti** saltando la pedina avversaria su una casella libera subito dietro.
  - **NON può mangiare la Dama**. Può mangiare solo altre pedine.
- **La Dama**:
  - Può mangiare sia in **avanti** che all'**indietro**.
  - Può mangiare sia pedine che altre dame.

### 5. Criteri di Priorità nelle Prese Multiple (Legge del Massimo)
Quando si hanno più possibilità di presa, occorre rispettare tassativamente il seguente ordine di priorità:
1. **Pezzi totali**: Bisogna mangiare dove si cattura il **maggior numero di pezzi**.
2. **Priorità di pezzo**: A parità di pezzi catturabili, se si può scegliere tra mangiare con una Dama o con una Pedina, si **DEVE mangiare con la Dama**.
3. **Qualità delle prese della Dama**: Se una Dama ha più opzioni con pari numero di pezzi catturabili, deve scegliere il percorso che cattura il **maggior numero di Dame**.
4. **Priorità del primo pezzo incontrato**: A parità di tutte le condizioni precedenti, la Dama deve scegliere il percorso dove incontra per prima una Dama rispetto a una pedina.

---

## 🛠️ Requisiti e Configurazione del Progetto

### Prerequisiti
- **Compiler C**: Clang / GCC / MSVC (con supporto C11).
- **CMake**: versione 3.16 o superiore.
- **Librerie di sistema**: Drivers OpenGL installati (su macOS inclusi via Framework OpenGL/Cocoa).

---

## 📦 Compilazione ed Esecuzione

### macOS / Linux
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Damascus
```

### Windows (PowerShell / Command Prompt)
```cmd
cmake -B build
cmake --build build --config Release
.\build\Release\Damascus.exe
```

---

## 📐 Struttura del Codice

```
Damascus/
├── CMakeLists.txt         # Configurazione di build CMake
├── README.md              # Documentazione del progetto e regole
├── shaders/
│   ├── basic.vert         # Vertex shader 3D (Posizione, Normali, UV)
│   └── basic.frag         # Fragment shader 3D (Illuminazione Phong, Texture)
└── src/
    ├── main.c             # Loop principale dell'applicazione
    ├── window.c/.h        # Gestione finestra GLFW e ridimensionamento
    ├── graphics.c/.h      # Rendering 3D OpenGL (Scacchiera, Pezzi, Bordi)
    ├── camera.c/.h        # Telecamera Arcball 3D (Rotazione click destro)
    ├── interaction.c/.h   # Raycasting 3D (Selezione click sinistro)
    ├── game.c/.h          # Logica della Dama Italiana e stato della partita
    ├── engine.h           # Interfaccia C per Engine AI stateful
    ├── engine_random.c/.h # Implementazione Engine Random
    └── ui.c/.h            # Menu di gioco con Nuklear GUI
```
