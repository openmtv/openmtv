/* The data of --filter hr, generated at build time by tools/hrdata.py into
 * build/hr_data.c: the rules from runtime/hr_rules.txt and the 4x font from
 * tools/hrfont/font*.txt and try/NAME/font*.txt (docs/hochaufloesend.md, 3b, 3c and 8). */
#ifndef MTV_HR_DATA_H
#define MTV_HR_DATA_H
enum { HR_NEAREST, HR_MMPX, HR_WRAP, HR_FONT, HR_THIN };
/* bank res, sprites first..last: rule (a bank's ranges before its default 0..255) */
typedef struct { unsigned short res; unsigned char first, last, rule; } HrRule;
/* a glyph of the 4x font: 4h rows of 32 sub-pixels (palette index, 0 clear) */
typedef struct { unsigned short res; unsigned char sprite, h; const unsigned char *px; } HrGlyph;
extern const HrRule hr_rules[];
extern const int hr_rule_count;
/* a 4x font: the draft (name "") or a trial font of tools/hrfont/try/NAME/ (HR_FONT=NAME) */
typedef struct { const char *name; const HrGlyph *glyphs; int count; } HrFontSet;
extern const HrFontSet hr_font_sets[];
extern const int hr_font_set_count;
/* AI pictures (docs/ki-bilder.md): where the game draws an animation part, from
 * runtime/ki_parts.txt -- kind 0: none (it moves), 1: on room a's picture at x, y,
 * 2: on sprite b of bank a at x, y, 3: at x, y in the picture of the group whose
 * first sprite is b of bank a (pictures shown together, piece beside piece); sorted
 * by res, k */
typedef struct { unsigned short res, k, kind, a, b; short x, y; } KiPart;
extern const KiPart ki_parts[];
extern const int ki_part_count;
/* ... and the sharp sprites res:k that always lie on picture r2:k2 of the model's at
 * x, y (the TV's button frames); sorted by r2, k2 */
typedef struct { unsigned short r2, k2, res, k; short x, y; } KiCover;
extern const KiCover ki_covers[];
extern const int ki_cover_count;
#endif
