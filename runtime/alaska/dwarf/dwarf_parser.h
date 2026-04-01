/* dwarf_parser.h ─────────────────────────────────────────────────────────────
 *
 * Minimal DWARF4/5 struct-layout extractor.
 *
 * Opens /proc/self/exe, maps it into memory, and walks the .debug_info DIE
 * tree to recover every struct type's size and per-slot pointer/scalar layout.
 * The binary must have been compiled with -g; no compiler changes are needed.
 *
 * Intended for embedded RISC-V environments: no external dependencies, no
 * heap allocation beyond the initial mmap, C99-compatible.
 *
 * Usage:
 *     DwarfTypeDB db;
 *     if (dwarf_parse_self(&db) == 0)
 *         dwarf_dump(&db);
 * -------------------------------------------------------------------------- */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Tuneable limits ─────────────────────────────────────────────────────── */
#define DWARF_MAX_STRUCTS 1024 /* max number of struct types recorded     */
#define DWARF_MAX_SLOTS 64    /* max 8-byte slots per struct (= 512 B)   */
#define DWARF_MAX_NAME 128    /* max struct/member name length            */

/* ── Public types ────────────────────────────────────────────────────────── */

/*
 * Classification of a single 8-byte slot within a struct.
 *
 * SLOT_SCALAR  — no pointer-typed field lives in this slot.
 * SLOT_POINTER — a pointer-typed field (including function pointers) occupies
 *                this slot.  The null ambiguity discussed in fingerprinting
 *                applies here: at runtime, this slot *may* be NULL.
 * SLOT_MIXED   — both a pointer field and a scalar field share this slot
 *                (typically caused by packed structs, bitfields, or anonymous
 *                unions).  Treat conservatively as "maybe pointer".
 */
typedef enum {
  SLOT_SCALAR = 0,
  SLOT_POINTER = 1,
  SLOT_MIXED = 2,
} SlotKind;

/* Recovered layout of one struct type. */
typedef struct {
  char name[DWARF_MAX_NAME]; /* struct tag, or "(anon@<die_offset>)"  */
  uint64_t die_offset;       /* byte offset of the DIE in .debug_info */
  uint32_t byte_size;        /* sizeof(struct)                        */
  int slot_count;            /* ceil(byte_size / 8)                   */
  SlotKind slots[DWARF_MAX_SLOTS];
} DwarfStruct;

/* Output container.  Zero-initialise before passing to dwarf_parse_self(). */
typedef struct {
  DwarfStruct structs[DWARF_MAX_STRUCTS];
  int count;
} DwarfTypeDB;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Parse DWARF from /proc/self/exe and populate db.  Returns 0 on success. */
int dwarf_parse_self(DwarfTypeDB *db);

/* Print every recovered struct layout to stdout.  Useful for debugging. */
void dwarf_dump(const DwarfTypeDB *db);

#ifdef __cplusplus
} /* extern "C" */
#endif