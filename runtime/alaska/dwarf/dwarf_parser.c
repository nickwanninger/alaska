/* dwarf_parser.c ─────────────────────────────────────────────────────────────
 *
 * DESIGN OVERVIEW
 * ═══════════════
 * DWARF stores type information in a tree of Debugging Information Entries
 * (DIEs).  Each DIE has a tag (what kind of thing it describes) and a list
 * of attributes (the details).  The encoding is compact: rather than repeat
 * tag+attribute names in every DIE, the file stores an "abbreviation table"
 * (.debug_abbrev) mapping small integer codes to (tag, attr-name, attr-form)
 * descriptors.  The actual DIE stream in .debug_info then just emits an
 * abbreviation code followed by the attribute values.
 *
 * We only need three ELF sections:
 *   .debug_abbrev  — the abbreviation table
 *   .debug_info    — the DIE tree (all compilation units, back to back)
 *   .debug_str     — interned strings (optional; needed for DW_FORM_strp names)
 *
 * The walk is ONE PASS over .debug_info.  We track:
 *   • Every pointer / typedef / scalar type DIE (by its .debug_info offset)
 *     so we can later resolve "is this member's type a pointer?" chains.
 *   • Every DW_TAG_structure_type with its members' byte offsets and type refs.
 *
 * After the walk we resolve each member's type chain and classify each
 * 8-byte slot of each struct as POINTER, SCALAR, or MIXED.
 *
 * KNOWN LIMITATIONS
 * ═════════════════
 * • Arrays of pointers (int *arr[N]) are classified as SCALAR because the
 *   DW_TAG_array_type itself is opaque to us.  A post-processing pass could
 *   expand the element type × count to produce the correct slot bitmap.
 * • Embedded structs (struct A { struct B b; }) mark B's slots as SCALAR in
 *   A's layout.  The inner struct's pointer fields will appear correctly in
 *   B's own DwarfStruct entry; a recursive expansion step would be needed to
 *   "inline" them into A.
 * • Only ELF64 little-endian is supported (covers x86-64 and RISC-V 64).
 * • DWARF5 indexed string/address forms (DW_FORM_strx, DW_FORM_addrx) are
 *   skipped; struct names using those forms will appear as "(anon@…)".
 * -------------------------------------------------------------------------- */

#include "dwarf_parser.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ══════════════════════════════════════════════════════════════════════════
 * ELF64 STRUCTURES
 * We replicate just the fields we use so there is no dependency on elf.h.
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
  uint8_t e_ident[16]; /* magic {0x7f,'E','L','F'}, class, data, …     */
  uint16_t e_type, e_machine;
  uint32_t e_version;
  uint64_t e_entry, e_phoff;
  uint64_t e_shoff; /* → section header table                        */
  uint32_t e_flags;
  uint16_t e_ehsize, e_phentsize, e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;    /* number of section headers                     */
  uint16_t e_shstrndx; /* index of the section-name string table section */
} Elf64_Ehdr;

typedef struct {
  uint32_t sh_name; /* byte offset into .shstrtab for the name       */
  uint32_t sh_type;
  uint64_t sh_flags, sh_addr;
  uint64_t sh_offset; /* byte offset of this section's data in the file */
  uint64_t sh_size;
  uint32_t sh_link, sh_info;
  uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;

/* ══════════════════════════════════════════════════════════════════════════
 * DWARF CONSTANTS
 * Only the subset we actually handle is listed.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Tags — what kind of entity a DIE describes */
#define DW_TAG_array_type 0x01
#define DW_TAG_class_type 0x02      /* class { … } — treated like struct  */
#define DW_TAG_enumeration_type 0x04
#define DW_TAG_member 0x0d          /* field of a struct/union            */
#define DW_TAG_pointer_type 0x0f    /* T*                                 */
#define DW_TAG_structure_type 0x13  /* struct { … }                       */
#define DW_TAG_subroutine_type 0x15 /* function pointer type — IS a ptr   */
#define DW_TAG_typedef 0x16         /* typedef T Name                     */
#define DW_TAG_union_type 0x17
#define DW_TAG_base_type 0x24     /* int, float, …                      */
#define DW_TAG_const_type 0x26    /* const T                            */
#define DW_TAG_volatile_type 0x35 /* volatile T                         */
#define DW_TAG_restrict_type 0x37 /* restrict T                         */

/* Attributes — properties of a DIE */
#define DW_AT_name 0x03
#define DW_AT_byte_size 0x0b
#define DW_AT_type 0x49                  /* reference to another type DIE     */
#define DW_AT_data_member_location 0x38  /* byte offset of a struct member    */
#define DW_AT_declaration 0x3c           /* true ⇒ forward-declaration only   */
#define DW_AT_str_offsets_base 0x72      /* DWARF5: base into .debug_str_offsets */

/* Forms — how an attribute value is encoded in the byte stream */
#define DW_FORM_addr 0x01
#define DW_FORM_block2 0x03
#define DW_FORM_block4 0x04
#define DW_FORM_data2 0x05
#define DW_FORM_data4 0x06
#define DW_FORM_data8 0x07
#define DW_FORM_string 0x08 /* inline null-terminated string      */
#define DW_FORM_block 0x09  /* ULEB128 length + bytes             */
#define DW_FORM_block1 0x0a /* 1-byte length + bytes              */
#define DW_FORM_data1 0x0b
#define DW_FORM_flag 0x0c     /* 1-byte boolean                     */
#define DW_FORM_sdata 0x0d    /* SLEB128 signed integer             */
#define DW_FORM_strp 0x0e     /* 4/8-byte offset into .debug_str    */
#define DW_FORM_udata 0x0f    /* ULEB128 unsigned integer           */
#define DW_FORM_ref_addr 0x10 /* absolute ref into .debug_info      */
#define DW_FORM_ref1 0x11     /* 1-byte CU-relative reference       */
#define DW_FORM_ref2 0x12
#define DW_FORM_ref4 0x13
#define DW_FORM_ref8 0x14
#define DW_FORM_ref_udata 0x15      /* ULEB128 CU-relative reference      */
#define DW_FORM_indirect 0x16       /* form encoded as ULEB128 in stream  */
#define DW_FORM_sec_offset 0x17     /* 4/8-byte section offset            */
#define DW_FORM_exprloc 0x18        /* ULEB128 length + DW_OP expression  */
#define DW_FORM_flag_present 0x19   /* implicit true, 0 bytes in stream   */
#define DW_FORM_ref_sig8 0x20       /* 8-byte type signature              */
#define DW_FORM_implicit_const 0x21 /* value stored in abbrev, not stream */
#define DW_FORM_strx 0x1a           /* DWARF5: indexed string (skip)      */
#define DW_FORM_addrx 0x1b          /* DWARF5: indexed address (skip)     */
#define DW_FORM_loclistx 0x22 /* DWARF5: indexed location list (skip) */
#define DW_FORM_rnglistx 0x23 /* DWARF5: indexed range list (skip)    */
#define DW_FORM_strx1 0x25
#define DW_FORM_strx2 0x26
#define DW_FORM_strx3 0x27
#define DW_FORM_strx4 0x28

/* DW_OP opcode used inside DW_AT_data_member_location block expressions */
#define DW_OP_plus_uconst 0x23

/* ══════════════════════════════════════════════════════════════════════════
 * PARSER LIMITS  (tune to taste for your target environment)
 * ══════════════════════════════════════════════════════════════════════════ */
#define MAX_ABBREVS 1024        /* abbrev codes per CU                */
#define MAX_ATTRS_PER_ABBREV 32 /* attributes per abbreviation entry  */
#define MAX_MEMBERS 128         /* members per struct                 */
#define MAX_TYPE_ENTRIES 8192   /* total type-chain entries           */
#define MAX_STRUCT_ENTRIES 512  /* total struct entries               */
#define CTX_DEPTH 64            /* max DIE nesting depth we track     */

/* ══════════════════════════════════════════════════════════════════════════
 * INTERNAL DATA STRUCTURES
 * ══════════════════════════════════════════════════════════════════════════ */

/* One (attr_name, attr_form) pair from the abbreviation table.
 * The implicit_const field is only meaningful when form == DW_FORM_implicit_const;
 * in that case the integer value lives in the abbreviation table itself
 * rather than in the DIE stream. */
typedef struct {
  uint16_t name, form;
  int64_t implicit_const;
} AttrSpec;

/* One abbreviation table entry: describes one *kind* of DIE. */
typedef struct {
  uint64_t code;        /* the ULEB128 code appearing in the stream  */
  uint16_t tag;         /* DW_TAG_*                                  */
  uint8_t has_children; /* 1 ⇒ child DIEs follow, terminated by 0   */
  int nattrs;
  AttrSpec attrs[MAX_ATTRS_PER_ABBREV];
} AbbrevEntry;

/* One member of a struct as discovered during the walk. */
typedef struct {
  char name[DWARF_MAX_NAME];
  uint64_t type_ref;    /* absolute .debug_info offset of this member's type DIE */
  uint32_t byte_offset; /* byte offset of this field within its parent struct    */
} Member;

/* A struct as we discover it, before slot-classification. */
typedef struct {
  char name[DWARF_MAX_NAME];
  uint64_t die_offset; /* where in .debug_info this struct's DIE lives    */
  uint32_t byte_size;  /* sizeof(struct)                                  */
  int is_decl;         /* set if DW_AT_declaration is present (fwd decl) */
  Member members[MAX_MEMBERS];
  int nmembers;
} StructRec;

/*
 * For resolving "is this type ultimately a pointer?" type chains.
 *
 * Example chain: typedef const MyPtr → const T → T* → struct Foo
 *   typedef   → TK_PASSTHRU (ref → const MyPtr's DIE... wait, that's wrong)
 *
 * Real example: member field of type "const Node *"
 *   DW_TAG_const_type       at offset A  → TK_PASSTHRU, ref=B
 *   DW_TAG_pointer_type     at offset B  → TK_POINTER
 *   ⇒ type_is_ptr(A) follows A→B→POINTER → returns 1  ✓
 */
typedef enum { TK_UNKNOWN = 0, TK_POINTER, TK_PASSTHRU, TK_SCALAR } TypeKind;

typedef struct {
  uint64_t offset; /* absolute .debug_info byte offset of this type DIE */
  TypeKind kind;
  uint64_t ref; /* for PASSTHRU/POINTER: the next type in the chain   */
} TypeRec;

/* ══════════════════════════════════════════════════════════════════════════
 * MODULE-LEVEL STATE
 * Using globals keeps the code simple and avoids any heap allocations after
 * the initial mmap.  If you need re-entrancy, wrap these in a context struct.
 * ══════════════════════════════════════════════════════════════════════════ */

static const uint8_t *g_elf; /* mmap base of the executable              */
static size_t g_elf_sz;

static const uint8_t *g_di;
size_t g_di_sz; /* .debug_info                        */
static const uint8_t *g_da;
size_t g_da_sz; /* .debug_abbrev                      */
static const uint8_t *g_ds;
size_t g_ds_sz; /* .debug_str (may be NULL)           */
static const uint8_t *g_so;
size_t g_so_sz; /* .debug_str_offsets (may be NULL)   */

/* Per-CU base offset into .debug_str_offsets, set when the compile_unit DIE
 * carries DW_AT_str_offsets_base.  Reset to 0 at the start of each CU. */
static uint64_t g_str_offsets_base;

static AbbrevEntry g_abbrevs[MAX_ABBREVS];
int g_nabbrevs;
static TypeRec g_types[MAX_TYPE_ENTRIES];
int g_ntypes;
static StructRec g_srecs[MAX_STRUCT_ENTRIES];
int g_nsrecs;

/* ══════════════════════════════════════════════════════════════════════════
 * BINARY READING HELPERS
 * ══════════════════════════════════════════════════════════════════════════ */

/* Read an sz-byte little-endian unsigned integer without advancing p. */
static inline uint64_t ule(const uint8_t *p, int sz) {
  uint64_t v = 0;
  for (int i = 0; i < sz; i++)
    v |= (uint64_t)p[i] << (8 * i);
  return v;
}

/*
 * ULEB128 — Unsigned Little-Endian Base-128.
 * Each byte contributes 7 bits of value (LSB first).
 * The high bit of each byte signals whether more bytes follow.
 *
 * Example: encoding 300 (= 0b1_0010_1100):
 *   byte 0: 0xAC  →  1_010_1100  (high bit set: more follows)
 *   byte 1: 0x02  →  0_000_0010  (high bit clear: done)
 *   decode: 010_1100 | (000_0010 << 7) = 0b1_0010_1100 = 300 ✓
 */
static uint64_t rd_uleb(const uint8_t **p) {
  uint64_t v = 0;
  int s = 0;
  uint8_t b;
  do {
    b = *(*p)++;
    v |= (uint64_t)(b & 0x7f) << s;
    s += 7;
  } while (b & 0x80);
  return v;
}

/* SLEB128 — same variable-length encoding, but sign-extended. */
static int64_t rd_sleb(const uint8_t **p) {
  int64_t v = 0;
  int s = 0;
  uint8_t b;
  do {
    b = *(*p)++;
    v |= (int64_t)(b & 0x7f) << s;
    s += 7;
  } while (b & 0x80);
  /* If the sign bit of the last group is set, extend the sign */
  if (s < 64 && (b & 0x40)) v |= -(int64_t)(1ULL << s);
  return v;
}

/*
 * Advance past one attribute value in the byte stream.
 *
 * We must handle EVERY known form, even for attributes we don't care about,
 * because the byte pointer must stay aligned with the data.  An unrecognised
 * form is fatal — we print a warning and leave p unchanged (the caller will
 * likely bail on the affected CU).
 */
static void skip_attr(const uint8_t **p, uint16_t form, int asz, int d64) {
  switch (form) {
    /* ── Zero-byte forms ── */
    case DW_FORM_flag_present:
      break;
    case DW_FORM_implicit_const:
      break;
    /* ── Fixed-size forms ── */
    case DW_FORM_flag:
    case DW_FORM_data1:
    case DW_FORM_ref1:
      *p += 1;
      break;
    case DW_FORM_data2:
    case DW_FORM_ref2:
      *p += 2;
      break;
    case DW_FORM_data4:
    case DW_FORM_ref4:
      *p += 4;
      break;
    case DW_FORM_data8:
    case DW_FORM_ref8:
    case DW_FORM_ref_sig8:
      *p += 8;
      break;
    case DW_FORM_addr:
      *p += asz;
      break;
    /* ── Offset forms: 4 bytes in 32-bit DWARF, 8 bytes in 64-bit DWARF ── */
    case DW_FORM_ref_addr:
    case DW_FORM_strp:
    case DW_FORM_sec_offset:
      *p += d64 ? 8 : 4;
      break;
    /* ── Variable-length integer forms ── */
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
    case DW_FORM_strx:
    case DW_FORM_addrx:
    case DW_FORM_loclistx:
    case DW_FORM_rnglistx:
      rd_uleb(p);
      break;
    case DW_FORM_sdata:
      rd_sleb(p);
      break;
    /* ── Inline null-terminated string ── */
    case DW_FORM_string:
      while (**p)
        (*p)++;
      (*p)++;
      break;
    /* ── Block forms: length prefix + that many payload bytes ── */
    case DW_FORM_block1: {
      uint64_t n = **p;
      (*p)++;
      *p += n;
      break;
    }
    case DW_FORM_block2: {
      uint64_t n = ule(*p, 2);
      *p += 2;
      *p += n;
      break;
    }
    case DW_FORM_block4: {
      uint64_t n = ule(*p, 4);
      *p += 4;
      *p += n;
      break;
    }
    case DW_FORM_block:
    case DW_FORM_exprloc: {
      uint64_t n = rd_uleb(p);
      *p += n;
      break;
    }
    /* ── DWARF5 indexed forms (just an index, no inline data) ── */
    case DW_FORM_strx1:
      *p += 1;
      break;
    case DW_FORM_strx2:
      *p += 2;
      break;
    case DW_FORM_strx3:
      *p += 3;
      break;
    case DW_FORM_strx4:
      *p += 4;
      break;
    /* ── Indirect: actual form is ULEB128-encoded in the stream ── */
    case DW_FORM_indirect:
      skip_attr(p, (uint16_t)rd_uleb(p), asz, d64);
      break;
    default:
      fprintf(stderr, "dwarf_parser: unhandled form 0x%x — stream may be corrupt\n", form);
      break;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * ELF SECTION FINDER
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Walk the ELF section header table and return a pointer to the named
 * section's data, along with its size.  Returns NULL if not found.
 */
static const uint8_t *elf_section(const char *name, size_t *sz_out) {
  const Elf64_Ehdr *eh = (const Elf64_Ehdr *)g_elf;
  const Elf64_Shdr *sht = (const Elf64_Shdr *)(g_elf + eh->e_shoff);
  /* The section-name string table section contains the names of all sections */
  const char *shstrtab = (const char *)(g_elf + sht[eh->e_shstrndx].sh_offset);

  for (int i = 0; i < (int)eh->e_shnum; i++) {
    if (strcmp(shstrtab + sht[i].sh_name, name) == 0) {
      if (sz_out) *sz_out = (size_t)sht[i].sh_size;
      return g_elf + sht[i].sh_offset;
    }
  }
  return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * ABBREVIATION TABLE PARSER
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Parse the abbreviation table beginning at byte offset `off` within
 * .debug_abbrev.  Each CU header specifies its own offset, so this function
 * is called once per compilation unit.
 *
 * Format (repeating until code == 0):
 *   [ULEB128 code]
 *   [ULEB128 DW_TAG_*]
 *   [byte   has_children]
 *   [pairs of ULEB128 attr_name, ULEB128 attr_form]   (zero-terminated pair)
 *   If any form == DW_FORM_implicit_const, an extra SLEB128 value follows.
 */
static void parse_abbrevs(uint64_t off) {
  const uint8_t *p = g_da + off;
  const uint8_t *end = g_da + g_da_sz;
  g_nabbrevs = 0;

  while (p < end && g_nabbrevs < MAX_ABBREVS) {
    uint64_t code = rd_uleb(&p);
    if (code == 0) break; /* code 0 terminates the table for this CU */

    AbbrevEntry *e = &g_abbrevs[g_nabbrevs++];
    e->code = code;
    e->tag = (uint16_t)rd_uleb(&p);
    e->has_children = *p++;
    e->nattrs = 0;

    for (;;) {
      uint64_t an = rd_uleb(&p);
      uint64_t af = rd_uleb(&p);
      if (an == 0 && af == 0) break; /* (0,0) sentinel ends attr list */
      if (e->nattrs < MAX_ATTRS_PER_ABBREV) {
        AttrSpec *a = &e->attrs[e->nattrs++];
        a->name = (uint16_t)an;
        a->form = (uint16_t)af;
        /* DW_FORM_implicit_const stores its value in the abbrev, not the DIE */
        a->implicit_const = (af == DW_FORM_implicit_const) ? rd_sleb(&p) : 0;
      }
    }
  }
}

/* Linear scan is fine; abbreviation codes are small sequential integers. */
static const AbbrevEntry *abbrev_find(uint64_t code) {
  for (int i = 0; i < g_nabbrevs; i++)
    if (g_abbrevs[i].code == code) return &g_abbrevs[i];
  return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * TYPE CHAIN MANAGEMENT
 * ══════════════════════════════════════════════════════════════════════════ */

static TypeRec *type_find(uint64_t off) {
  for (int i = 0; i < g_ntypes; i++)
    if (g_types[i].offset == off) return &g_types[i];
  return NULL;
}

static TypeRec *type_add(uint64_t off) {
  if (g_ntypes >= MAX_TYPE_ENTRIES) return NULL;
  TypeRec *r = &g_types[g_ntypes++];
  r->offset = off;
  r->kind = TK_UNKNOWN;
  r->ref = 0;
  return r;
}

/*
 * Follow a chain of typedef / const / volatile / pointer wrappers and return
 * 1 if the ultimate type is a pointer, 0 otherwise.
 *
 * A depth guard prevents infinite loops on (hypothetically) cyclic DWARF.
 * In valid DWARF, the chain always terminates at a base_type, struct, array,
 * union, or pointer_type.
 */
static int type_is_ptr(uint64_t off) {
  for (int guard = 0; guard < 32; guard++) {
    TypeRec *r = type_find(off);
    if (!r) return 0; /* unknown → conservative: scalar */
    if (r->kind == TK_POINTER) return 1;
    if (r->kind == TK_SCALAR) return 0;
    if (r->kind == TK_PASSTHRU) {
      off = r->ref;
      continue;
    }
    return 0;
  }
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * ATTRIBUTE VALUE READERS
 * These each advance *p past the attribute value AND return a useful result.
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Read DW_AT_type as an absolute byte offset into .debug_info.
 *
 * Most ref forms (ref1/2/4/8, ref_udata) are CU-relative — the stored value
 * is a byte offset measured from the start of the CU header (i.e., from the
 * first byte of the unit_length field).  We add cu_off (= distance from
 * g_di to the CU header) to convert to an absolute offset.
 *
 * ref_addr and sec_offset are already absolute from g_di.
 */
static uint64_t read_ref(const uint8_t **p, uint16_t form, int asz, int d64, uint64_t cu_off) {
  switch (form) {
    case DW_FORM_ref1: {
      uint64_t v = **p;
      (*p)++;
      return cu_off + v;
    }
    case DW_FORM_ref2: {
      uint64_t v = ule(*p, 2);
      *p += 2;
      return cu_off + v;
    }
    case DW_FORM_ref4: {
      uint64_t v = ule(*p, 4);
      *p += 4;
      return cu_off + v;
    }
    case DW_FORM_ref8: {
      uint64_t v = ule(*p, 8);
      *p += 8;
      return cu_off + v;
    }
    case DW_FORM_ref_udata:
      return cu_off + rd_uleb(p);
    case DW_FORM_ref_addr:
    case DW_FORM_sec_offset: {
      uint64_t v = ule(*p, d64 ? 8 : 4);
      *p += d64 ? 8 : 4;
      return v;
    }
    default:
      skip_attr(p, form, asz, d64);
      return UINT64_MAX;
  }
}

/*
 * Read DW_AT_data_member_location as a uint32 byte offset.
 *
 * DWARF3+ compilers typically use a plain integer form (DW_FORM_data*).
 * DWARF2 and some DWARF3 compilers encode it as a DW_OP block expression.
 * The most common expression is DW_OP_plus_uconst (0x23) followed by the
 * offset as a ULEB128.  We also handle DW_OP_lit0–DW_OP_lit31 for small
 * offsets.
 */
static uint32_t read_member_loc(const uint8_t **p, uint16_t form, int asz, int d64, int64_t ic) {
  switch (form) {
    case DW_FORM_data1: {
      uint32_t v = **p;
      (*p)++;
      return v;
    }
    case DW_FORM_data2: {
      uint32_t v = ule(*p, 2);
      *p += 2;
      return v;
    }
    case DW_FORM_data4: {
      uint32_t v = ule(*p, 4);
      *p += 4;
      return v;
    }
    case DW_FORM_data8: {
      uint32_t v = (uint32_t)ule(*p, 8);
      *p += 8;
      return v;
    }
    case DW_FORM_udata:
      return (uint32_t)rd_uleb(p);
    case DW_FORM_sdata:
      return (uint32_t)rd_sleb(p);
    case DW_FORM_implicit_const:
      return (uint32_t)ic;
    default:
      break;
  }

  /* Block / exprloc forms: decode the DW_OP expression */
  const uint8_t *blk = NULL;
  uint64_t blen = 0;
  switch (form) {
    case DW_FORM_block1:
      blen = **p;
      (*p)++;
      blk = *p;
      *p += blen;
      break;
    case DW_FORM_block2:
      blen = ule(*p, 2);
      *p += 2;
      blk = *p;
      *p += blen;
      break;
    case DW_FORM_block4:
      blen = ule(*p, 4);
      *p += 4;
      blk = *p;
      *p += blen;
      break;
    case DW_FORM_block:
    case DW_FORM_exprloc:
      blen = rd_uleb(p);
      blk = *p;
      *p += blen;
      break;
    default:
      skip_attr(p, form, asz, d64);
      return 0;
  }

  /*
   * DW_OP_plus_uconst (0x23): the most common member-location opcode.
   * The offset follows as a ULEB128.
   */
  if (blen >= 2 && blk[0] == DW_OP_plus_uconst) {
    const uint8_t *bp = blk + 1;
    return (uint32_t)rd_uleb(&bp);
  }
  /* DW_OP_lit0–DW_OP_lit31 (0x30–0x4f): small literal offsets */
  if (blen >= 1 && blk[0] >= 0x30 && blk[0] <= 0x4f) return (uint32_t)(blk[0] - 0x30);

  return 0;
}

/*
 * Resolve a DWARF5 indexed string.
 *
 * strx forms store a small integer index.  To get the actual string:
 *   1. Multiply the index by 4 (32-bit DWARF) or 8 (64-bit DWARF) and add
 *      g_str_offsets_base to get a byte position in .debug_str_offsets.
 *   2. Read the 4- or 8-byte value there; that is an offset into .debug_str.
 *   3. Return the null-terminated string at that offset.
 */
static const char *resolve_strx(uint64_t idx, int d64) {
  if (!g_so || !g_ds) return NULL;
  int esz = d64 ? 8 : 4;
  uint64_t byte_pos = g_str_offsets_base + idx * (uint64_t)esz;
  if (byte_pos + (uint64_t)esz > g_so_sz) return NULL;
  uint64_t str_off = ule(g_so + byte_pos, esz);
  if (str_off >= g_ds_sz) return NULL;
  return (const char *)(g_ds + str_off);
}

/*
 * Read DW_AT_name and return a pointer into the mmap'd data.
 * The pointer remains valid for the lifetime of the mapping.
 * Returns NULL if the form is not a string form we handle.
 */
static const char *read_name(const uint8_t **p, uint16_t form, int d64) {
  switch (form) {
    case DW_FORM_string: {
      const char *s = (const char *)*p;
      while (**p)
        (*p)++;
      (*p)++;
      return s;
    }
    case DW_FORM_strp: {
      uint64_t off = ule(*p, d64 ? 8 : 4);
      *p += d64 ? 8 : 4;
      return (g_ds && off < g_ds_sz) ? (const char *)(g_ds + off) : NULL;
    }
    case DW_FORM_strx1: {
      uint64_t idx = **p;
      (*p)++;
      return resolve_strx(idx, d64);
    }
    case DW_FORM_strx2: {
      uint64_t idx = ule(*p, 2);
      *p += 2;
      return resolve_strx(idx, d64);
    }
    case DW_FORM_strx3: {
      uint64_t idx = ule(*p, 3);
      *p += 3;
      return resolve_strx(idx, d64);
    }
    case DW_FORM_strx4: {
      uint64_t idx = ule(*p, 4);
      *p += 4;
      return resolve_strx(idx, d64);
    }
    case DW_FORM_strx: {
      uint64_t idx = rd_uleb(p);
      return resolve_strx(idx, d64);
    }
    default:
      skip_attr(p, form, 8, d64);
      return NULL;
  }
}

/* Read DW_AT_byte_size as a uint32. */
static uint32_t read_size(const uint8_t **p, uint16_t form, int asz, int d64, int64_t ic) {
  switch (form) {
    case DW_FORM_data1: {
      uint32_t v = **p;
      (*p)++;
      return v;
    }
    case DW_FORM_data2: {
      uint32_t v = ule(*p, 2);
      *p += 2;
      return v;
    }
    case DW_FORM_data4: {
      uint32_t v = ule(*p, 4);
      *p += 4;
      return v;
    }
    case DW_FORM_data8: {
      uint32_t v = (uint32_t)ule(*p, 8);
      *p += 8;
      return v;
    }
    case DW_FORM_udata:
      return (uint32_t)rd_uleb(p);
    case DW_FORM_implicit_const:
      return (uint32_t)ic;
    default:
      skip_attr(p, form, asz, d64);
      return 0;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * MAIN DWARF WALKER
 *
 * Context stack design
 * ────────────────────
 * DWARF DIEs form a tree.  A DIE with has_children=1 is followed in the byte
 * stream by its children, terminated by a null DIE (abbrev code 0).  We use
 * an integer depth counter and a ctx[] array indexed by depth to track which
 * struct (if any) we are currently inside.
 *
 *   ctx[d] = index into g_srecs of the struct we're inside at depth d, or -1
 *
 * Rules:
 *   • When a DIE at depth d has_children=1:
 *       - If it's a structure_type: ctx[d+1] = new struct's index
 *       - Otherwise:                ctx[d+1] = ctx[d]  (inherit parent)
 *       - depth++
 *   • When we see a null DIE:        depth--
 *   • When processing any DIE at depth d: cur_ctx = ctx[d]
 *
 * This correctly handles nested struct definitions (they get their own entry
 * in g_srecs and their members are attributed to them, not the outer struct),
 * anonymous unions/structs (they inherit the parent's context so their
 * DW_TAG_member children are added to the enclosing named struct), and
 * arbitrarily deep nesting up to CTX_DEPTH.
 * ══════════════════════════════════════════════════════════════════════════ */

static void walk_info(void) {
  const uint8_t *p = g_di;
  const uint8_t *end = g_di + g_di_sz;

  while (p < end) {
    /* ── CU header ──────────────────────────────────────────────────── */
    const uint8_t *cu_hdr = p;

    /*
     * The first four bytes are the "initial length".
     * If they equal 0xffffffff the unit uses 64-bit DWARF (rare);
     * otherwise the value IS the unit length (32-bit DWARF).
     */
    uint32_t init = (uint32_t)ule(p, 4);
    p += 4;
    int d64 = (init == 0xffffffff);
    uint64_t unit_len;
    if (d64) {
      unit_len = ule(p, 8);
      p += 8;
    } else {
      unit_len = init;
    }

    const uint8_t *cu_end = p + unit_len;
    if (cu_end > end || unit_len == 0) break;

    uint16_t ver = (uint16_t)ule(p, 2);
    p += 2;

    uint64_t abbrev_off;
    int asz;
    if (ver >= 5) {
      /*
       * DWARF5 CU header:
       *   unit_type   (1 byte)   — we assume DW_UT_compile = 0x01
       *   addr_size   (1 byte)
       *   abbrev_off  (4 or 8 bytes)
       */
      p++; /* unit_type */
      asz = *p++;
      abbrev_off = ule(p, d64 ? 8 : 4);
      p += d64 ? 8 : 4;
    } else {
      /*
       * DWARF4 and earlier CU header:
       *   abbrev_off  (4 or 8 bytes)
       *   addr_size   (1 byte)
       */
      abbrev_off = ule(p, d64 ? 8 : 4);
      p += d64 ? 8 : 4;
      asz = *p++;
    }

    /*
     * cu_off: byte distance from g_di to this CU's header.
     * CU-relative DIE references (DW_FORM_ref4 etc.) store an offset
     * measured from this point and we add cu_off to get an absolute
     * .debug_info offset.
     */
    uint64_t cu_off = (uint64_t)(cu_hdr - g_di);

    parse_abbrevs(abbrev_off);
    g_str_offsets_base = 0; /* reset for this CU; overwritten by DW_AT_str_offsets_base */

    /* ── DIE walk ───────────────────────────────────────────────────── */
    int ctx[CTX_DEPTH];
    memset(ctx, -1, sizeof ctx);
    int depth = 0;

    while (p < cu_end) {
      uint64_t die_off = (uint64_t)(p - g_di);
      uint64_t code = rd_uleb(&p);

      if (code == 0) {
        /* Null DIE: close this level's children list */
        if (depth > 0) depth--;
        continue;
      }

      const AbbrevEntry *ab = abbrev_find(code);
      if (!ab) {
        fprintf(stderr,
                "dwarf_parser: unknown abbrev code %llu at di+0x%llx "
                "(CU corrupt?)\n",
                (unsigned long long)code, (unsigned long long)die_off);
        p = cu_end; /* skip the rest of this CU */
        break;
      }

      /* ── Scan all attributes in declaration order ──────────────── */
      /* We must consume EVERY attribute to keep p aligned, even ones
       * we don't care about. */
      const char *a_name = NULL;
      uint64_t a_type = UINT64_MAX;
      uint32_t a_size = 0;
      uint32_t a_mloc = 0;
      int a_has_mloc = 0;
      int a_is_decl = 0;

      for (int i = 0; i < ab->nattrs; i++) {
        uint16_t an = ab->attrs[i].name;
        uint16_t af = ab->attrs[i].form;
        int64_t ic = ab->attrs[i].implicit_const;

        switch (an) {
          case DW_AT_name:
            a_name = read_name(&p, af, d64);
            break;
          case DW_AT_byte_size:
            a_size = read_size(&p, af, asz, d64, ic);
            break;
          case DW_AT_type:
            a_type = read_ref(&p, af, asz, d64, cu_off);
            break;
          case DW_AT_data_member_location:
            a_mloc = read_member_loc(&p, af, asz, d64, ic);
            a_has_mloc = 1;
            break;
          case DW_AT_declaration:
            if (af == DW_FORM_flag_present) {
              a_is_decl = 1;
            } else if (af == DW_FORM_flag) {
              a_is_decl = *p++;
            } else {
              skip_attr(&p, af, asz, d64);
            }
            break;
          case DW_AT_str_offsets_base:
            /* DWARF5: DW_FORM_sec_offset — 4 or 8-byte offset into .debug_str_offsets */
            g_str_offsets_base = ule(p, d64 ? 8 : 4);
            p += d64 ? 8 : 4;
            break;
          default:
            skip_attr(&p, af, asz, d64);
            break;
        }
      }

      /* ── Act on this DIE's tag ─────────────────────────────────── */
      int cur_ctx = (depth < CTX_DEPTH) ? ctx[depth] : -1;
      int new_struct_idx = -1;

      switch (ab->tag) {
        /*
         * Pointer types — these are what we're hunting for in member fields.
         * DW_TAG_subroutine_type covers function pointers (void (*)(int)),
         * which are genuine pointers and should be treated as such.
         */
        case DW_TAG_pointer_type:
        case DW_TAG_subroutine_type: {
          TypeRec *tr = type_find(die_off);
          if (!tr) tr = type_add(die_off);
          if (tr) {
            tr->kind = TK_POINTER;
            tr->ref = a_type;
          }
          break;
        }

        /*
         * Transparent wrapper types — just pass through to the inner type.
         * A "const T *" is: const_type → pointer_type → T.
         * Marking these as PASSTHRU lets type_is_ptr() follow the chain.
         */
        case DW_TAG_typedef:
        case DW_TAG_const_type:
        case DW_TAG_volatile_type:
        case DW_TAG_restrict_type: {
          TypeRec *tr = type_find(die_off);
          if (!tr) tr = type_add(die_off);
          if (tr) {
            tr->kind = (a_type != UINT64_MAX) ? TK_PASSTHRU : TK_SCALAR;
            tr->ref = a_type;
          }
          break;
        }

        /* Scalar types — integers, floats, enums, arrays, unions */
        case DW_TAG_base_type:
        case DW_TAG_enumeration_type:
        case DW_TAG_array_type:
        case DW_TAG_union_type: {
          TypeRec *tr = type_find(die_off);
          if (!tr) tr = type_add(die_off);
          if (tr) tr->kind = TK_SCALAR;
          break;
        }

        /*
         * Struct/class type — register as scalar in the type chain (an
         * embedded struct field is NOT a pointer), and record its definition
         * for our output database.  DW_TAG_class_type is the C++ class tag;
         * for layout purposes it is identical to DW_TAG_structure_type.
         */
        case DW_TAG_class_type:
        case DW_TAG_structure_type: {
          TypeRec *tr = type_find(die_off);
          if (!tr) tr = type_add(die_off);
          if (tr) tr->kind = TK_SCALAR;

          if (!a_is_decl && g_nsrecs < MAX_STRUCT_ENTRIES) {
            StructRec *sr = &g_srecs[g_nsrecs];
            memset(sr, 0, sizeof *sr);
            sr->die_offset = die_off;
            sr->byte_size = a_size;
            if (a_name)
              snprintf(sr->name, DWARF_MAX_NAME, "%s", a_name);
            else
              snprintf(sr->name, DWARF_MAX_NAME, "(anon@%llx)", (unsigned long long)die_off);
            new_struct_idx = g_nsrecs++;
          }
          break;
        }

        /*
         * Struct member — add to the enclosing struct if we know which
         * struct we're inside and we successfully parsed its byte offset.
         * Note: members lacking DW_AT_data_member_location are unusual
         * (they can appear in C++ static members), so we skip them.
         */
        case DW_TAG_member: {
          if (cur_ctx >= 0 && a_has_mloc && a_type != UINT64_MAX) {
            StructRec *sr = &g_srecs[cur_ctx];
            if (sr->nmembers < MAX_MEMBERS) {
              Member *m = &sr->members[sr->nmembers++];
              if (a_name) snprintf(m->name, DWARF_MAX_NAME, "%s", a_name);
              m->type_ref = a_type;
              m->byte_offset = a_mloc;
            }
          }
          break;
        }

        default:
          break;
      } /* switch (tag) */

      /* ── Depth tracking ───────────────────────────────────────────
       * If this DIE has children, set the context for the next level
       * and increment depth.  We always increment depth (even past
       * CTX_DEPTH) so null DIEs correctly match their parent push.
       * ─────────────────────────────────────────────────────────── */
      if (ab->has_children) {
        int nd = depth + 1;
        if (nd < CTX_DEPTH) {
          /*
           * Structs grant their own index to children (so DW_TAG_member
           * children know which struct to join).  Everything else —
           * compile_unit, namespace, anonymous struct/union, etc. —
           * inherits the current context, so members inside anonymous
           * inner structs are still attributed to the named outer struct.
           */
          ctx[nd] = (new_struct_idx >= 0) ? new_struct_idx : cur_ctx;
        }
        depth = nd;
      }
    } /* DIE loop */

    p = cu_end; /* advance to next CU */
  }             /* CU loop */
}

/* ══════════════════════════════════════════════════════════════════════════
 * SLOT CLASSIFICATION
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Convert a raw StructRec (member list with type refs) into a DwarfStruct
 * with classified 8-byte slots.
 *
 * For each member we follow its type chain to determine if it's a pointer,
 * then assign the result to slot (byte_offset / 8).
 *
 * Promotion rules:
 *   SCALAR + pointer write  → POINTER
 *   POINTER + scalar write  → MIXED   (straddle: be conservative)
 *   POINTER + pointer write → POINTER (fine; multiple ptr fields in one slot
 *                                       is normal in packed/bitfield structs)
 */
static void classify_slots(DwarfStruct *dst, const StructRec *src) {
  memset(dst, 0, sizeof *dst);
  snprintf(dst->name, DWARF_MAX_NAME, "%s", src->name);
  dst->die_offset = src->die_offset;
  dst->byte_size = src->byte_size;
  dst->slot_count = (int)((src->byte_size + 7) / 8);
  if (dst->slot_count > DWARF_MAX_SLOTS) dst->slot_count = DWARF_MAX_SLOTS;

  for (int i = 0; i < dst->slot_count; i++)
    dst->slots[i] = SLOT_SCALAR;

  for (int i = 0; i < src->nmembers; i++) {
    const Member *m = &src->members[i];
    int slot = (int)(m->byte_offset / 8);
    if (slot < 0 || slot >= dst->slot_count) continue;

    if (type_is_ptr(m->type_ref)) {
      dst->slots[slot] = SLOT_POINTER;
    } else {
      /* A scalar field in a slot that was already claimed by a pointer
       * indicates packing (e.g. a bitfield after a pointer) — mark MIXED */
      if (dst->slots[slot] == SLOT_POINTER) dst->slots[slot] = SLOT_MIXED;
    }
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */

int dwarf_parse_self(DwarfTypeDB *db) {
  memset(db, 0, sizeof *db);

  /* Open /proc/self/exe — the kernel always keeps this symlink valid */
  int fd = open("/proc/self/exe", O_RDONLY);
  if (fd < 0) {
    perror("dwarf_parser: open /proc/self/exe");
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("dwarf_parser: fstat");
    close(fd);
    return -1;
  }
  g_elf_sz = (size_t)st.st_size;

  /*
   * MAP_PRIVATE + PROT_READ gives us a read-only copy-on-write view.
   * We only ever read from this mapping — no writes, no COW faults.
   */
  g_elf = mmap(NULL, g_elf_sz, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (g_elf == MAP_FAILED) {
    perror("dwarf_parser: mmap");
    g_elf = NULL;
    return -1;
  }

  /* Validate ELF identity bytes */
  if (g_elf_sz < 16 || memcmp(g_elf,
                              "\x7f"
                              "ELF",
                              4) != 0) {
    fprintf(stderr, "dwarf_parser: not an ELF file\n");
    goto fail;
  }
  /* e_ident[4] = EI_CLASS:   2 = ELFCLASS64
   * e_ident[5] = EI_DATA:    1 = ELFDATA2LSB (little-endian) */
  if (g_elf[4] != 2 || g_elf[5] != 1) {
    fprintf(stderr,
            "dwarf_parser: only ELF64 little-endian supported "
            "(got class=%d data=%d)\n",
            g_elf[4], g_elf[5]);
    goto fail;
  }

  /* Locate the DWARF sections we need */
  g_di = elf_section(".debug_info", &g_di_sz);
  g_da = elf_section(".debug_abbrev", &g_da_sz);
  g_ds = elf_section(".debug_str", &g_ds_sz);             /* optional */
  g_so = elf_section(".debug_str_offsets", &g_so_sz);     /* optional, DWARF5 */

  if (!g_di || !g_da) {
    fprintf(stderr,
            "dwarf_parser: .debug_info or .debug_abbrev not found "
            "(was the binary compiled with -g?)\n");
    goto fail;
  }

  /* Reset global accumulators */
  g_nabbrevs = g_ntypes = g_nsrecs = 0;

  /* Single-pass walk over the entire .debug_info */
  walk_info();

  /* Classify slots and populate the output database */
  for (int i = 0; i < g_nsrecs; i++) {
    if (g_srecs[i].is_decl) continue;        /* forward declaration only   */
    if (g_srecs[i].byte_size == 0) continue; /* empty / incomplete struct */
    if (db->count >= DWARF_MAX_STRUCTS) break;
    classify_slots(&db->structs[db->count++], &g_srecs[i]);
  }

  munmap((void *)g_elf, g_elf_sz);
  return 0;

fail:
  munmap((void *)g_elf, g_elf_sz);
  return -1;
}

void dwarf_dump(const DwarfTypeDB *db) {
  printf("DwarfTypeDB: %d struct(s)\n\n", db->count);
  for (int i = 0; i < db->count; i++) {
    const DwarfStruct *s = &db->structs[i];
    printf("struct %-48s  sz=%-5u  slots=%d\n", s->name, s->byte_size, s->slot_count);
    for (int j = 0; j < s->slot_count; j++) {
      const char *kind = s->slots[j] == SLOT_POINTER ? "PTR  "
                         : s->slots[j] == SLOT_MIXED ? "MIXED"
                                                     : "scl  ";
      printf("  [%2d] +0x%03x  %s\n", j, j * 8, kind);
    }
    putchar('\n');
  }
}