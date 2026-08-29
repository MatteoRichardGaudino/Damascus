#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BOOK_BACKEND_NONE = 0,
    BOOK_BACKEND_KINGSROW_ODB = 1, // kr_italian.odb (1.76M positions)
    BOOK_BACKEND_CHECKERBOARD_BIN = 2 // book.bin (1.2M entries)
} BookBackendType;

typedef enum {
    BOOK_MODE_OFF = 0,
    BOOK_MODE_BEST = 1,      // Play only best-scored move
    BOOK_MODE_GOOD = 2,      // Sample among good moves (<= 10 cp loss)
    BOOK_MODE_ALL = 3,       // Sample across all book moves
    BOOK_MODE_PUCT_GUIDED = 4 // Blend book into PUCT priors without skipping search
} BookPlayMode;

typedef struct {
    Move     move;
    int16_t  score;
    uint8_t  depth;
    uint8_t  flags;
    float    prior_weight;
} BookMoveEntry;

typedef struct {
    int           count;
    BookMoveEntry entries[32];
} BookMoveList;

typedef struct {
    bool            loaded;
    BookBackendType active_backend;
    uint32_t        total_positions;
    uint32_t        total_moves;
    char            file_path[256];
    char            version_str[16];
} OpeningBookStatus;

// Lifecycle
bool opening_book_init(BookBackendType backend, const char *custom_path);
void opening_book_cleanup(void);
OpeningBookStatus opening_book_get_status(void);
bool opening_book_is_available(BookBackendType backend, const char *custom_path);

// Backend & Mode Helpers
const char *opening_book_backend_get_name(BookBackendType backend);
const char *opening_book_backend_get_cli_name(BookBackendType backend);
BookBackendType opening_book_backend_parse(const char *name);
const char *opening_book_mode_get_name(BookPlayMode mode);
const char *opening_book_mode_get_cli_name(BookPlayMode mode);
BookPlayMode opening_book_mode_parse(const char *name);
void opening_book_set_custom_path(const char *path);
const char *opening_book_get_custom_path(void);

// Query API
bool opening_book_probe(const GameState *game, BookMoveList *out_moves);
bool opening_book_probe_compact(const CompactState *state, BookMoveList *out_moves);
Move opening_book_select_move(const GameState *game, BookPlayMode mode, float temperature, uint32_t *rng_state);

// Helpers / Converters
uint64_t opening_book_encode_compact_board(const Board *board, Player player);

#endif // OPENING_BOOK_H
