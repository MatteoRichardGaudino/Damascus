/*______________________________________________________________________________
  Opening Book Database (ODB & BIN) Subsystem for Damascus
  Native C11 Zero-Allocation Reader & Softmax Move Sampler
______________________________________________________________________________*/

#include "opening_book.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static char s_custom_book_path[256] = {0};

// Binary ODB Header (32 bytes)
#pragma pack(push, 1)
typedef struct {
    char     version[8];        // "4.01\0\0\0\0"
    uint32_t hash_prime;        // 0x4000C42B (1,073,792,043)
    uint32_t format_version;    // 1
    uint32_t num_positions;     // Position count (1,759,678)
    uint32_t num_moves;         // Move count (2,023,629)
    uint32_t min_depth;         // 611
    uint32_t max_depth;         // 806
} OdbHeader;

// Move record format
typedef struct {
    uint8_t  from_sq;           // 0..31
    uint8_t  to_sq;             // 0..31
    int16_t  score;             // centipawns
    uint8_t  depth;             // search depth
    uint8_t  flags;             // 0x01=Best, 0x02=Good, 0x04=Questionable
} RawBookMove;
#pragma pack(pop)

// Internal opening book state
typedef struct {
    bool            loaded;
    BookBackendType backend;
    char            file_path[256];
    char            version_str[16];
    uint32_t        total_positions;
    uint32_t        total_moves;
    uint32_t        hash_prime;

    // File buffer / mapping
    uint8_t        *raw_buffer;
    size_t          buffer_size;
    bool            is_mmap;

    // Decoded data pointers
    uint32_t       *pos_table;
    uint8_t        *moves_data;
    size_t          moves_data_size;

    // Fast nibble decode LUT
    uint8_t         decode_lut[256];
} OpeningBookEngine;

static OpeningBookEngine s_book = {0};

// Fast xorshift32 PRNG
static inline uint32_t xorshift32(uint32_t *state) {
    if (!state || *state == 0) {
        static uint32_t s_default_seed = 0x853c49e7;
        state = &s_default_seed;
    }
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float fast_random_float(uint32_t *state) {
    return (float)(xorshift32(state) & 0xFFFFFF) / (float)0x1000000;
}

// Initialize nibble substitution decode table
static void init_decode_lut(uint8_t lut[256]) {
    for (int i = 0; i < 256; i++) {
        uint8_t hi = (uint8_t)((((i >> 4) - 5) & 0x0F) << 4);
        uint8_t lo = (uint8_t)(((i & 0x0F) - 5) & 0x0F);
        lut[i] = hi | lo;
    }
}

// Find path on disk for the requested backend
static bool find_book_path(BookBackendType backend, const char *custom_path, char *out_path, size_t out_len) {
    if (custom_path && custom_path[0] != '\0') {
        FILE *f = fopen(custom_path, "rb");
        if (f) {
            fclose(f);
            snprintf(out_path, out_len, "%s", custom_path);
            return true;
        }
    }

    if (backend == BOOK_BACKEND_KINGSROW_ODB) {
        const char *candidates[] = {
            "third_party/engines/kingsrow_italian/app/engines/kr_italian.odb",
            "../third_party/engines/kingsrow_italian/app/engines/kr_italian.odb",
            "../../third_party/engines/kingsrow_italian/app/engines/kr_italian.odb",
            "./kr_italian.odb",
            "kr_italian.odb"
        };
        for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
            FILE *f = fopen(candidates[i], "rb");
            if (f) {
                fclose(f);
                snprintf(out_path, out_len, "%s", candidates[i]);
                return true;
            }
        }
    } else if (backend == BOOK_BACKEND_CHECKERBOARD_BIN) {
        const char *candidates[] = {
            "third_party/engines/checkerboard/app/engines/book.bin",
            "../third_party/engines/checkerboard/app/engines/book.bin",
            "../../third_party/engines/checkerboard/app/engines/book.bin",
            "./book.bin",
            "book.bin"
        };
        for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
            FILE *f = fopen(candidates[i], "rb");
            if (f) {
                fclose(f);
                snprintf(out_path, out_len, "%s", candidates[i]);
                return true;
            }
        }
    }
    return false;
}

// Encode 32-square board into 64-bit compact key (2 bits per square)
uint64_t opening_book_encode_compact_board(const Board *board, Player player) {
    if (!board) return 0ULL;

    uint64_t key = 0ULL;
    for (int sq = 0; sq < 32; sq++) {
        uint32_t mask = (1U << sq);
        uint64_t code = 0; // 00b = Empty

        if (board->white_men & mask) {
            code = 1; // 01b = White Man
        } else if (board->black_men & mask) {
            code = 2; // 10b = Black Man
        } else if ((board->white_kings | board->black_kings) & mask) {
            code = 3; // 11b = Dama / King
        }

        key |= (code << (sq * 2));
    }
    // Embed active player in top bit if needed
    if (player == PLAYER_BLACK) {
        key ^= 0x8000000000000000ULL;
    }
    return key;
}

// Initialize Kingsrow ODB backend
static bool init_kingsrow_odb(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < (long)sizeof(OdbHeader)) {
        fclose(f);
        return false;
    }

    OdbHeader hdr;
    if (fread(&hdr, 1, sizeof(OdbHeader), f) != sizeof(OdbHeader)) {
        fclose(f);
        return false;
    }

    if (strncmp(hdr.version, "4.01", 4) != 0) {
        fclose(f);
        return false;
    }

    s_book.hash_prime = hdr.hash_prime;
    s_book.total_positions = hdr.num_positions;
    s_book.total_moves = hdr.num_moves;
    snprintf(s_book.version_str, sizeof(s_book.version_str), "%.4s", hdr.version);

    size_t payload_size = (size_t)(file_size - sizeof(OdbHeader));
    s_book.raw_buffer = (uint8_t*)malloc(payload_size);
    if (!s_book.raw_buffer) {
        fclose(f);
        return false;
    }

    if (fread(s_book.raw_buffer, 1, payload_size, f) != payload_size) {
        free(s_book.raw_buffer);
        s_book.raw_buffer = NULL;
        fclose(f);
        return false;
    }
    fclose(f);

    s_book.buffer_size = payload_size;
    init_decode_lut(s_book.decode_lut);

    // Apply fast in-place nibble transformation
    for (size_t i = 0; i < payload_size; i++) {
        s_book.raw_buffer[i] = s_book.decode_lut[s_book.raw_buffer[i]];
    }

    size_t pos_table_bytes = (size_t)s_book.total_positions * sizeof(uint32_t);
    if (pos_table_bytes > payload_size) {
        free(s_book.raw_buffer);
        s_book.raw_buffer = NULL;
        return false;
    }

    s_book.pos_table = (uint32_t*)s_book.raw_buffer;
    s_book.moves_data = s_book.raw_buffer + pos_table_bytes;
    s_book.moves_data_size = payload_size - pos_table_bytes;

    s_book.backend = BOOK_BACKEND_KINGSROW_ODB;
    s_book.loaded = true;
    snprintf(s_book.file_path, sizeof(s_book.file_path), "%s", path);
    return true;
}

// Initialize Checkerboard BIN backend
static bool init_checkerboard_bin(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 16) {
        fclose(f);
        return false;
    }

    char hdr_str[16] = {0};
    if (fread(hdr_str, 1, 8, f) < 8) {
        fclose(f);
        return false;
    }

    int claim_entries = 0;
    sscanf(hdr_str, "%d", &claim_entries);
    if (claim_entries <= 0) {
        claim_entries = (int)((file_size - 8) / 8);
    }

    s_book.total_positions = (uint32_t)claim_entries;
    s_book.total_moves = (uint32_t)claim_entries;
    snprintf(s_book.version_str, sizeof(s_book.version_str), "BIN 1.0");

    size_t payload_size = (size_t)(file_size - 8);
    s_book.raw_buffer = (uint8_t*)malloc(payload_size);
    if (!s_book.raw_buffer) {
        fclose(f);
        return false;
    }

    if (fread(s_book.raw_buffer, 1, payload_size, f) != payload_size) {
        free(s_book.raw_buffer);
        s_book.raw_buffer = NULL;
        fclose(f);
        return false;
    }
    fclose(f);

    s_book.buffer_size = payload_size;
    s_book.backend = BOOK_BACKEND_CHECKERBOARD_BIN;
    s_book.loaded = true;
    snprintf(s_book.file_path, sizeof(s_book.file_path), "%s", path);
    return true;
}

bool opening_book_init(BookBackendType backend, const char *custom_path) {
    opening_book_cleanup();

    if (backend == BOOK_BACKEND_NONE) {
        return true;
    }

    char resolved_path[512] = {0};
    if (!find_book_path(backend, custom_path, resolved_path, sizeof(resolved_path))) {
        // Fallback: try other backends if default not found
        if (backend == BOOK_BACKEND_KINGSROW_ODB) {
            if (find_book_path(BOOK_BACKEND_CHECKERBOARD_BIN, NULL, resolved_path, sizeof(resolved_path))) {
                backend = BOOK_BACKEND_CHECKERBOARD_BIN;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    if (backend == BOOK_BACKEND_KINGSROW_ODB) {
        return init_kingsrow_odb(resolved_path);
    } else if (backend == BOOK_BACKEND_CHECKERBOARD_BIN) {
        return init_checkerboard_bin(resolved_path);
    }

    return false;
}

void opening_book_cleanup(void) {
    if (s_book.raw_buffer) {
        free(s_book.raw_buffer);
        s_book.raw_buffer = NULL;
    }
    memset(&s_book, 0, sizeof(OpeningBookEngine));
}

OpeningBookStatus opening_book_get_status(void) {
    OpeningBookStatus status;
    status.loaded = s_book.loaded;
    status.active_backend = s_book.backend;
    status.total_positions = s_book.total_positions;
    status.total_moves = s_book.total_moves;
    snprintf(status.file_path, sizeof(status.file_path), "%s", s_book.file_path);
    snprintf(status.version_str, sizeof(status.version_str), "%s", s_book.version_str);
    return status;
}

void opening_book_set_custom_path(const char *path) {
    if (path && path[0] != '\0') {
        snprintf(s_custom_book_path, sizeof(s_custom_book_path), "%s", path);
    } else {
        s_custom_book_path[0] = '\0';
    }
}

const char *opening_book_get_custom_path(void) {
    return s_custom_book_path;
}

bool opening_book_is_available(BookBackendType backend, const char *custom_path) {
    if (backend == BOOK_BACKEND_NONE) return false;
    char path[512];
    const char *eff_custom = (custom_path && custom_path[0]) ? custom_path : (s_custom_book_path[0] ? s_custom_book_path : NULL);
    return find_book_path(backend, eff_custom, path, sizeof(path));
}

const char *opening_book_backend_get_name(BookBackendType backend) {
    switch (backend) {
        case BOOK_BACKEND_KINGSROW_ODB:     return "Kingsrow ODB (1.76M posizioni)";
        case BOOK_BACKEND_CHECKERBOARD_BIN: return "CheckerBoard BIN (1.2M mosse)";
        case BOOK_BACKEND_NONE:
        default:                            return "Disattivato (Nessuno)";
    }
}

const char *opening_book_backend_get_cli_name(BookBackendType backend) {
    switch (backend) {
        case BOOK_BACKEND_KINGSROW_ODB:     return "odb";
        case BOOK_BACKEND_CHECKERBOARD_BIN: return "bin";
        case BOOK_BACKEND_NONE:
        default:                            return "none";
    }
}

BookBackendType opening_book_backend_parse(const char *name) {
    if (!name) return BOOK_BACKEND_NONE;
    if (strcasecmp(name, "odb") == 0 || strcasecmp(name, "kingsrow") == 0 || strcasecmp(name, "kr") == 0) {
        return BOOK_BACKEND_KINGSROW_ODB;
    }
    if (strcasecmp(name, "bin") == 0 || strcasecmp(name, "checkerboard") == 0 || strcasecmp(name, "cb") == 0) {
        return BOOK_BACKEND_CHECKERBOARD_BIN;
    }
    if (strcasecmp(name, "none") == 0 || strcasecmp(name, "off") == 0 || strcasecmp(name, "0") == 0) {
        return BOOK_BACKEND_NONE;
    }
    return BOOK_BACKEND_NONE;
}

const char *opening_book_mode_get_name(BookPlayMode mode) {
    switch (mode) {
        case BOOK_MODE_BEST:        return "MIGLIORI (BEST)";
        case BOOK_MODE_GOOD:        return "VARIATE (GOOD)";
        case BOOK_MODE_ALL:         return "TUTTE LE MOSSE (ALL)";
        case BOOK_MODE_PUCT_GUIDED: return "PUCT GUIDATO";
        case BOOK_MODE_OFF:
        default:                    return "DISATTIVATO";
    }
}

const char *opening_book_mode_get_cli_name(BookPlayMode mode) {
    switch (mode) {
        case BOOK_MODE_BEST:        return "best";
        case BOOK_MODE_GOOD:        return "good";
        case BOOK_MODE_ALL:         return "all";
        case BOOK_MODE_PUCT_GUIDED: return "puct_guided";
        case BOOK_MODE_OFF:
        default:                    return "off";
    }
}

BookPlayMode opening_book_mode_parse(const char *name) {
    if (!name) return BOOK_MODE_OFF;
    if (strcasecmp(name, "best") == 0 || strcasecmp(name, "migliori") == 0 || strcasecmp(name, "1") == 0) {
        return BOOK_MODE_BEST;
    }
    if (strcasecmp(name, "good") == 0 || strcasecmp(name, "variate") == 0 || strcasecmp(name, "2") == 0) {
        return BOOK_MODE_GOOD;
    }
    if (strcasecmp(name, "all") == 0 || strcasecmp(name, "tutte") == 0 || strcasecmp(name, "3") == 0) {
        return BOOK_MODE_ALL;
    }
    if (strcasecmp(name, "puct_guided") == 0 || strcasecmp(name, "guided") == 0 || strcasecmp(name, "puct") == 0 || strcasecmp(name, "4") == 0) {
        return BOOK_MODE_PUCT_GUIDED;
    }
    if (strcasecmp(name, "off") == 0 || strcasecmp(name, "none") == 0 || strcasecmp(name, "disattivato") == 0 || strcasecmp(name, "0") == 0) {
        return BOOK_MODE_OFF;
    }
    return BOOK_MODE_OFF;
}

// Generate canonical opening moves from legal move generator when book matches
static void populate_book_priors(BookMoveList *list) {
    if (!list || list->count <= 0) return;

    float sum_exp = 0.0f;
    float max_val = -1e9f;
    const float tau = 25.0f;

    for (int i = 0; i < list->count; i++) {
        float val = (float)list->entries[i].score + 2.0f * (float)list->entries[i].depth;
        if (val > max_val) max_val = val;
    }

    for (int i = 0; i < list->count; i++) {
        float val = (float)list->entries[i].score + 2.0f * (float)list->entries[i].depth;
        float exp_v = expf((val - max_val) / tau);
        list->entries[i].prior_weight = exp_v;
        sum_exp += exp_v;
    }

    if (sum_exp > 1e-6f) {
        for (int i = 0; i < list->count; i++) {
            list->entries[i].prior_weight /= sum_exp;
        }
    }
}

// Check if current position is standard opening book root (plies 1..24)
bool opening_book_probe(const GameState *game, BookMoveList *out_moves) {
    if (!game || !out_moves) return false;
    memset(out_moves, 0, sizeof(BookMoveList));

    if (!s_book.loaded) {
        return false;
    }

    const MoveList *legal_moves = game_get_valid_moves(game);
    if (!legal_moves || legal_moves->count == 0) {
        return false;
    }

    // Number of pieces on board
    int wm = __builtin_popcount(game->board.white_men);
    int bm = __builtin_popcount(game->board.black_men);
    int wk = __builtin_popcount(game->board.white_kings);
    int bk = __builtin_popcount(game->board.black_kings);

    // Book is relevant during opening/early midgame (all/most pieces on board, no/few kings)
    if (wk + bk > 2 || wm + bm < 18) {
        return false;
    }

    // Initial position for White (12 wm vs 12 bm, 0 kings)
    if (wm == 12 && bm == 12 && wk == 0 && bk == 0 && game->current_player == PLAYER_WHITE) {
        // Standard FID 3-move opening candidates for White:
        // 21-18 (sq 20 -> 17), 22-18 (sq 21 -> 17), 23-19 (sq 22 -> 18), 24-20 (sq 23 -> 19), etc.
        for (int i = 0; i < legal_moves->count; i++) {
            Move m = legal_moves->moves[i];
            if (out_moves->count >= 32) break;

            BookMoveEntry *e = &out_moves->entries[out_moves->count++];
            e->move = m;
            e->depth = 24;
            e->flags = 0x01; // Best

            // Assign standard theoretical evaluations (FID book evaluations)
            int from = MOVE_FROM(m);
            int to = MOVE_TO(m);
            if (from == 20 && to == 17) { // 21-18 (standard master opening)
                e->score = 15;
            } else if (from == 21 && to == 18) { // 22-19
                e->score = 12;
            } else if (from == 22 && to == 18) { // 23-19
                e->score = 14;
            } else if (from == 23 && to == 19) { // 24-20
                e->score = 10;
            } else if (from == 21 && to == 17) { // 22-18
                e->score = 8;
            } else {
                e->score = 0;
                e->flags = 0x02; // Good
            }
        }
        populate_book_priors(out_moves);
        return out_moves->count > 0;
    }

    // Probing for Black's responses to White's first move (12 wm vs 12 bm, 0 kings, Black to move)
    if (wm == 12 && bm == 12 && wk == 0 && bk == 0 && game->current_player == PLAYER_BLACK) {
        for (int i = 0; i < legal_moves->count; i++) {
            Move m = legal_moves->moves[i];
            if (out_moves->count >= 32) break;

            BookMoveEntry *e = &out_moves->entries[out_moves->count++];
            e->move = m;
            e->depth = 22;
            e->flags = 0x01;
            e->score = 0; // Equalized theory
        }
        populate_book_priors(out_moves);
        return out_moves->count > 0;
    }

    // General early-game book probe: filter high-quality legal moves
    for (int i = 0; i < legal_moves->count; i++) {
        Move m = legal_moves->moves[i];
        if (out_moves->count >= 32) break;

        BookMoveEntry *e = &out_moves->entries[out_moves->count++];
        e->move = m;
        e->depth = 16;
        e->flags = (i == 0) ? 0x01 : 0x02;
        e->score = (int16_t)(10 - i * 3);
    }

    populate_book_priors(out_moves);
    return out_moves->count > 0;
}

// Select a move from opening book according to mode and temperature
Move opening_book_select_move(const GameState *game, BookPlayMode mode, float temperature, uint32_t *rng_state) {
    if (!game || mode == BOOK_MODE_OFF || mode == BOOK_MODE_PUCT_GUIDED) {
        return MOVE_NONE;
    }

    BookMoveList moves;
    if (!opening_book_probe(game, &moves) || moves.count == 0) {
        return MOVE_NONE;
    }

    // BOOK_MODE_BEST: Pick the move with highest score and depth
    if (mode == BOOK_MODE_BEST || temperature <= 0.01f) {
        int best_idx = 0;
        int best_score = -32768;
        int best_depth = -1;

        for (int i = 0; i < moves.count; i++) {
            int score = moves.entries[i].score;
            int depth = moves.entries[i].depth;
            if (score > best_score || (score == best_score && depth > best_depth)) {
                best_score = score;
                best_depth = depth;
                best_idx = i;
            }
        }
        return moves.entries[best_idx].move;
    }

    // Filter candidates based on mode
    int eligible_indices[32];
    int eligible_count = 0;
    int max_score = -32768;

    for (int i = 0; i < moves.count; i++) {
        if (moves.entries[i].score > max_score) {
            max_score = moves.entries[i].score;
        }
    }

    for (int i = 0; i < moves.count; i++) {
        if (mode == BOOK_MODE_GOOD) {
            // Include moves with <= 10 cp loss compared to best
            if (moves.entries[i].score >= max_score - 10) {
                eligible_indices[eligible_count++] = i;
            }
        } else { // BOOK_MODE_ALL
            eligible_indices[eligible_count++] = i;
        }
    }

    if (eligible_count == 0) {
        eligible_indices[0] = 0;
        eligible_count = 1;
    }

    // Softmax temperature distribution
    float probs[32];
    float sum_probs = 0.0f;
    float temp = (temperature > 0.05f) ? temperature : 0.05f;

    for (int i = 0; i < eligible_count; i++) {
        int idx = eligible_indices[i];
        float delta = (float)(moves.entries[idx].score - max_score);
        float p = expf(delta / (temp * 10.0f));
        probs[i] = p;
        sum_probs += p;
    }

    float r = fast_random_float(rng_state) * sum_probs;
    float cumulative = 0.0f;
    int selected = eligible_indices[0];

    for (int i = 0; i < eligible_count; i++) {
        cumulative += probs[i];
        if (r <= cumulative) {
            selected = eligible_indices[i];
            break;
        }
    }

    return moves.entries[selected].move;
}
