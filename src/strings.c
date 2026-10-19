/* Strings: str_format and number formatting, string and memory helpers.
 *
 * Original module 0E35 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "hrs.h"
#include "hr.h"

/* str_format's variable arguments: the words a C caller passes (the
 * str_format() macro, game_protos.h) or, for a machine-level caller, the
 * words it pushed on the emulated stack.  The format walks them byte by
 * byte: '#z' takes a single byte, so whatever follows it is read from an odd
 * offset, as in the original. */
struct fmt_args {
    const u16 *w;       /* the argument words; NULL: on the emulated stack */
    u16 n;              /* number of words in w */
    u16 seg, off;       /* w == NULL: the first argument word is at seg:off */
};

/* An argument byte beyond the words the caller passed.  The original reads
 * on into its caller's stack frame; none of the game's calls does (checked
 * over all format strings of both languages, see str_format), so this is
 * only reached by edited texts that ask for more arguments than their call
 * passes: they get zeros. */
static u8 fmt_arg_byte(const struct fmt_args *a, u16 k) {
    if (!a->w) return FP8(MK32(a->seg, a->off + k), 0);
    if (k < 2u * a->n) return (u8)(a->w[k >> 1] >> (k & 1) * 8);
    return 0;
}
static u16 fmt_arg16(const struct fmt_args *a, u16 k) {
    if (!a->w) return FP16(MK32(a->seg, a->off + k), 0);
    return fmt_arg_byte(a, k) | fmt_arg_byte(a, k + 1) << 8;
}
static u32 fmt_arg32(const struct fmt_args *a, u16 k) {
    if (!a->w) return FP32(MK32(a->seg, a->off + k), 0);
    return fmt_arg16(a, k) | (u32)fmt_arg16(a, k + 2) << 16;
}

/* the conversion letters, in the order of the original's table at 0E35:0154 */
static const char fmt_letters[6] = { 'd', 'f', 'l', 's', 'v', 'z' };

/* 0E35:0000  game sprintf into a new 1000-byte buffer with '#' codes
 *
 * Codes: #d int, #z signed byte (takes one byte of the arguments), #l long,
 * #s far string (a null pointer prints only the padding), #v glyph (0xC0 +
 * value), #f gauge of chars 1..7, WIDTH + 1 long, VALUE cells filled.
 * Between '#' and the letter: a pad flag ('0': pad numbers with '0'; '_':
 * pad with '{', in the game font as wide as a digit (a blank?); '+c': pad
 * with c), then a width (digits) or '*n' (strings: cut to what fits into n
 * 8-pixel columns).  Numbers are right-aligned in WIDTH chars; strings are cut to
 * WIDTH, and padded only with '_' or '+c'.  Without a pad flag the pad char
 * is NUL (the block it lives in is zeroed), so a number with a width and no
 * flag would end the string early (no game format does that).  An unknown
 * letter prints "Fuck" and then the letter itself as text.  The format's
 * NUL is copied like any char, which terminates the result; the final
 * strncpy(out, NULL, 1) then copies the byte at 0000:0000 behind it (the
 * low byte of the divide-error vector the start-up code installs, '%'). */
static u32 str_format_args(u32 fmt, const struct fmt_args *a) {
    u32 buf;            /* the result (1000 bytes, zeroed) */
    u32 out;            /* next output byte */
    u32 p;              /* next format char */
    u16 end;            /* offset of the format's terminating NUL */
    u16 ap;             /* byte offset of the next argument */
    u32 padc;           /* the pad char (a fresh 2-byte block per code) */
    u16 pad;            /* strings: pad to the width ('_' or '+c') */
    u16 width, ndig, v, i;
    u32 lv, s;
    u16 fill, k;
    FUNC_TICK(59);
    ap = 0;
    buf = mem_calloc(0x3E8, 1);
    p = fmt;
    out = buf;
    end = FPOFF(fmt) + LO16(rtl_strlen_w(W2(fmt)));
    for (;;) {
        LOOP_TICK(373);
        width = 0;
        padc = mem_calloc(2, 1);
        pad = 0;
        if (FP8(p, 0) != '#') {
            FP8(out, 0) = FP8(p, 0);
            out = FPADD(out, 1);
            p = FPADD(p, 1);
            goto next;
        }
        p = FPADD(p, 1);
        if (FP8(p, 0) == '_') {
            pad = 1;
            p = FPADD(p, 1);
            FP8(padc, 0) = '{';
        } else if (FP8(p, 0) == '+') {
            pad = 1;
            p = FPADD(p, 1);
            FP8(padc, 0) = FP8(p, 0);
            p = FPADD(p, 1);
        } else if (FP8(p, 0) == '0') {
            FP8(padc, 0) = '0';
            p = FPADD(p, 1);
        }
        ndig = str_digit_count(p);
        if ((i16)ndig > 0) {
            width = LO16(rtl_atoi_w(W2(p)));
            p = FPADD(p, ndig);
        } else if (FP8(p, 0) == '*') {
            p = FPADD(p, 1);
            ndig = str_digit_count(p);
            width = LO16(rtl_atoi_w(W2(p)));
            p = FPADD(p, ndig);
            width = width + 1000;
        }
        for (k = 0; FP8(p, 0) != (u8)fmt_letters[k]; k++) {
            LOOP_TICK(5);
            if (k == 5) {
                rtl_strncpy_w(W2(out), 0xD1C /* "Fuck" */, DSEG, 4);
                out = FPADD(out, 4);
                goto next;
            }
        }
        switch (fmt_letters[k]) {
        case 'd':
            v = fmt_arg16(a, ap);
            ap += 2;
            fmt_put_int(out, &v, &width, padc);
            p = FPADD(p, 1);
            out = FPADD(out, width);
            break;
        case 'z':
            v = (u16)(i8)fmt_arg_byte(a, ap);
            ap += 1;
            fmt_put_int(out, &v, &width, padc);
            p = FPADD(p, 1);
            out = FPADD(out, width);
            break;
        case 'l':
            lv = MK32(fmt_arg16(a, ap + 2), fmt_arg16(a, ap));
            ap += 4;
            fmt_put_long(out, &lv, &width, padc);
            p = FPADD(p, 1);
            out = FPADD(out, width);
            break;
        case 's':
            s = fmt_arg32(a, ap);
            ap += 4;
            p = FPADD(p, 1);
            if (s == 0) {
                /* (with '*n' the fill is n + 1000 chars long) */
                if (pad == 1) {
                    mem_fill_char(out, padc, width);
                    out = FPADD(out, width);
                }
                break;
            }
            if ((i16)width > 1000) width = text_fit_chars(s, width - 1000);
            if ((width != 0 && LO16(rtl_strlen_w(W2(s))) > width) || pad == 1) {
                /* exactly WIDTH chars: cut, or padded */
                if (LO16(rtl_strlen_w(W2(s))) >= width) {
                    rtl_strncpy_w(W2(out), W2(s), width);
                } else {
                    rtl_strncpy_w(W2(out), W2(s), LO16(rtl_strlen_w(W2(s))));
                    fill = width - LO16(rtl_strlen_w(W2(s)));
                    mem_fill_char(FPADD(out, LO16(rtl_strlen_w(W2(s)))), padc, fill);
                }
                out = FPADD(out, width);
            } else {
                rtl_strncpy_w(W2(out), W2(s), LO16(rtl_strlen_w(W2(s))));
                out = FPADD(out, LO16(rtl_strlen_w(W2(s))));
            }
            break;
        case 'v':
            p = FPADD(p, 1);
            v = fmt_arg16(a, ap);
            ap += 2;
            FP8(out, 0) = (u8)v + 0xC0;
            out = FPADD(out, 1);
            break;
        case 'f':
            p = FPADD(p, 1);
            v = fmt_arg16(a, ap);
            ap += 2;
            FP8(out, 0) = 1;
            out = FPADD(out, 1);
            /* chars 1 at both ends; of the WIDTH-1 cells between, the first
             * VALUE are 2/4/6, the rest 3/5/7, where 4/5 mark the quarters
             * and 6/7 the halves of the width (a width of 2 or 3 divides by
             * zero) */
            for (i = 1; (i16)i < (i16)width; i++) {
                LOOP_TICK(52);
                if (v != 0) {
                    if ((i16)i % (i16)((i16)width >> 2) != 0) FP8(out, 0) = 2;
                    else if ((i16)i % (i16)((i16)width >> 1) != 0) FP8(out, 0) = 4;
                    else FP8(out, 0) = 6;
                    v--;
                } else if ((i16)i % (i16)((i16)width >> 2) != 0) {
                    FP8(out, 0) = 3;
                } else if ((i16)i % (i16)((i16)width >> 1) != 0) {
                    FP8(out, 0) = 5;
                } else {
                    FP8(out, 0) = 7;
                }
                out = FPADD(out, 1);
            }
            FP8(out, 0) = 1;
            out = FPADD(out, 1);
            break;
        }
next:
        mem_free(LO16(padc), HI16(padc));
        if (FPOFF(p) > end) break;
    }
    rtl_strncpy_w(W2(out), 0, 0, 1);
    return buf;
}

/* str_format with the argument words of a C caller: N words at ARGS
 * (use the str_format(fmt, ...) macro, which counts them) */
u32 str_format_words(u32 fmt, const u16 *args, u16 n) {
    struct fmt_args a = { args, n, 0, 0 };
    return str_format_args(fmt, &a);
}

/* str_format with the argument words on the emulated stack at seg:off (the
 * glue wrapper f_0E35_0000: a machine-level caller) */
u32 str_format_at(u32 fmt, u16 seg, u16 off) {
    struct fmt_args a = { NULL, 0, seg, off };
    return str_format_args(fmt, &a);
}

/* 0E35:04C0  number of leading digits (-1 if more than 10) */
u16 str_digit_count(u32 a0) {
    u16 ax;
    u16 l_2;
    FUNC_TICK(14);
    l_2 = 0;
    while (!(((i8)FP8(a0, l_2)) < 48 || ((i8)FP8(a0, l_2)) > 57 || ((i16)l_2) > 10)) {
        LOOP_TICK(11);
        l_2 = l_2 + 1;
    }
    if (((i16)l_2) <= 10) {
        ax = l_2;
    } else {
        ax = 0xFFFF;
    }
    return ax;
}

/* 0E35:0504  str_format helper: *value in decimal at out, right-aligned in
 * *width chars with the pad char *padc.  A negative *value is negated in
 * place after its '-' (which takes one of the width's chars).  When the
 * digits do not fit (or the width is 0), *width becomes their number. */
void fmt_put_int(u32 out, u16 *value, u16 *width, u32 padc) {
    u16 w;
    u16 i;
    u32 digits;
    FUNC_TICK(87);
    digits = mem_calloc(6, 1);
    w = *width;
    if ((i16)*value < 0) {
        rtl_strncpy_w(W2(out), 0xD21 /* "-" */, DSEG, 1);
        *value = -*value;
        out = FPADD(out, 1);
        w = w - 1;
    }
    rtl_itoa_w(*value, W2(digits), 0xA);
    if (LO16(rtl_strlen_w(W2(digits))) >= w || w == 0) {
        *width = LO16(rtl_strlen_w(W2(digits)));
    } else {
        for (i = LO16(rtl_strlen_w(W2(digits))) + 1; (i16)i <= (i16)w; i++) {
            LOOP_TICK(13);
            rtl_strncpy_w(W2(out), W2(padc), 1);
            out = FPADD(out, 1);
        }
    }
    rtl_strncpy_w(W2(out), W2(digits), LO16(rtl_strlen_w(W2(digits))));
    mem_free(LO16(digits), HI16(digits));
}

/* 0E35:060D  str_format helper: fmt_put_int for a long *value */
void fmt_put_long(u32 out, u32 *value, u16 *width, u32 padc) {
    u16 w;
    u16 i;
    u32 digits;
    FUNC_TICK(95);
    digits = mem_calloc(0xC, 1);
    w = *width;
    if ((i32)*value < 0) {
        rtl_strncpy_w(W2(out), 0xD23 /* "-" */, DSEG, 1);
        *value = -*value;
        out = FPADD(out, 1);
        w = w - 1;
    }
    rtl_ltoa_w(W2(*value), W2(digits), 0xA);
    if (LO16(rtl_strlen_w(W2(digits))) >= w || w == 0) {
        *width = LO16(rtl_strlen_w(W2(digits)));
    } else {
        for (i = LO16(rtl_strlen_w(W2(digits))) + 1; (i16)i <= (i16)w; i++) {
            LOOP_TICK(13);
            rtl_strncpy_w(W2(out), W2(padc), 1);
            out = FPADD(out, 1);
        }
    }
    rtl_strncpy_w(W2(out), W2(digits), LO16(rtl_strlen_w(W2(digits))));
    mem_free(LO16(digits), HI16(digits));
}

/* 0E35:0730  parse leading decimal digits as a long (unused) */
void str_to_long(u32 a0) {
    u16 l_2;
    u32 l_6;
    FUNC_TICK(13);
    l_2 = 0;
    l_6 = 0;
    while (!(((i8)FP8(a0, l_2)) < 48 || ((i8)FP8(a0, l_2)) > 57)) {
        LOOP_TICK(28);
        l_6 = (((u32)(l_6 * 0xA)) + (MK32(((i16)(((u16)(i8)FP8(a0, l_2)) + 0xFFD0)) >> 0xF, ((u16)(i8)FP8(a0, l_2)) + 0xFFD0)));
        l_2 = l_2 + 1;
    }
    return;
}

/* 0E35:079A  ?overwrite the start of dst with n chars of src followed by s2 (unused) */
void str_patch(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u32 a5) {
    u32 l_4;
    u32 t1;
    u32 t4;
    FUNC_TICK(83);
    t1 = rtl_strlen_w(W2(a5));
    l_4 = mem_calloc((LO16(t1) + a4) + 1, 1);
    rtl_strcpy_w(W2(l_4), a0, a1);
    rtl_strncpy_w(W2(l_4), a2, a3, a4);
    if (FP8(a5, 0) != 0x30 || LO16((rtl_strlen_w(W2(a5)))) != 1) {
        t4 = rtl_strlen_w(W2(a5));
        rtl_strncpy_w(LO16(l_4) + a4, HI16(l_4), W2(a5), LO16(t4));
    } else {
        rtl_strncpy_w(LO16(l_4) + a4, HI16(l_4), 0xD25, DSEG, 1);
    }
    rtl_strcpy_w(a0, a1, W2(l_4));
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 0E35:0873  zero n bytes at a far pointer */
void mem_zero(u32 a0, u16 a2) {
    u16 t3;
    FUNC_TICK(5);
    for (;;) {
        LOOP_TICK(7);
        t3 = a2;
        a2 = a2 - 1;
        if (t3 == 0) break;
        FP8(a0, 0) = 0;
        a0 = MK32(HI16(a0), LO16(a0) + 1);
    }
    return;
}

/* 0E35:088E  copy n bytes between far pointers (forward) */
u16 far_memcpy(u8 *a0, const u8 *a2, u16 a4) {
    u16 ax;
    u16 t4;
    FUNC_TICK(5);
    HRS(hrs_memcpy(a0, a2, a4));
    hr_memcpy(a0, a2, a4);
    for (;;) {
        LOOP_TICK(10);
        t4 = a4;
        a4 = a4 - 1;
        ax = t4;
        if (t4 == 0) break;
        *a0++ = *a2++;
    }
    return ax;
}

/* 0E35:08B1  length of a far string */
u16 far_strlen(u32 a0) {
    u16 ax;
    u16 l_2;
    u16 t1;
    u16 t2;
    FUNC_TICK(10);
    l_2 = 0;
    for (;;) {
        LOOP_TICK(5);
        t1 = FPOFF(a0);
        t2 = FPSEG(a0);
        a0 = MK32(HI16(a0), LO16(a0) + 1);
        if (MEM8(t2, t1) == 0) break;
        l_2 = l_2 + 1;
    }
    ax = l_2;
    return ax;
}

/* 0E35:08D6  fill n bytes with the char *pc */
void mem_fill_char(u32 a0, u32 a2, u16 a4) {
    u16 l_2;
    FUNC_TICK(8);
    l_2 = 0;
    while (((i16)l_2) < ((i16)a4)) {
        LOOP_TICK(9);
        FP8(a0, l_2) = FP8(a2, 0);
        l_2 = l_2 + 1;
    }
    return;
}

/* 0E35:0901  how many chars of a string fit into n 8-px columns (glyph widths 0x22E, space 3, 1 px gap) */
u16 text_fit_chars(u32 a0, u16 a2) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    FUNC_TICK(13);
    l_4 = 0;
    l_2 = 0;
    while (!(((i16)(((i16)(((i16)(((i16)l_4) >> 1)) >> 1)) >> 1)) >= ((i16)a2) || FP8(a0, l_2) == 0 || FP8(a0, l_2) == 0)) {
        LOOP_TICK(39);
        if (FP8(a0, l_2) != 0x20) {
            if ((g_glyph_width(FP8(a0, l_2))) != 0) {
                l_4 = l_4 + (g_glyph_width(FP8(a0, l_2)));
            } else {
                l_4 = l_4 + 1;
            }
        } else {
            l_4 = l_4 + 3;
        }
        l_4 = l_4 + 1;
        l_2 = l_2 + 1;
    }
    ax = l_2;
    return ax;
}

/* 0E35:0989  pixel width of a string in the game font */
u16 text_width_px(u32 a0) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    FUNC_TICK(11);
    l_4 = 0;
    l_2 = 0;
    while (FP8(a0, l_4) != 0) {
        LOOP_TICK(29);
        if (FP8(a0, l_4) != 0x20) {
            if ((g_glyph_width(FP8(a0, l_4))) != 0) {
                l_2 = l_2 + (g_glyph_width(FP8(a0, l_4)));
            } else {
                l_2 = l_2 + 1;
            }
        } else {
            l_2 = l_2 + 3;
        }
        l_2 = l_2 + 1;
        l_4 = l_4 + 1;
    }
    ax = l_2;
    return ax;
}

/* 0E35:09F7  -1 if a0 is a prefix of a2 (unused) */
void str_is_prefix(u32 a0, u32 a2) {
    FUNC_TICK(12);
    while (FP8(a2, 0) == FP8(a0, 0)) {
        LOOP_TICK(7);
        a2 = MK32(HI16(a2), LO16(a2) + 1);
        a0 = MK32(HI16(a0), LO16(a0) + 1);
    }
    if (FP8(a0, 0) != 0) {
    }
    return;
}
