/* User interface: cursor and mouse, frames, popups, message boxes, TV family panel, endings.
 *
 * Original module 105E of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* 105E:000E  ++g_cursor_shown; when it becomes 1 save the VGA background (24x11) under the mouse and draw the arrow g_cursor_shape directly on VGA; at 0 with a drag image: draw g_drag_image at mouse-hotspot (clamped to 320x120) instead */
u16 cursor_show(u16 ax) {
    u16 di;
    u16 si;
    u32 rv_;
    u16 t9;
    u16 t10;
    u16 t16;
    u32 t26;
    u8 fr_[16] = {0};                  /* bytes bp-16 .. bp-1 of the original frame */
    FUNC_TICK(148);
    g_cursor_shown = g_cursor_shown + 1;
    if (g_cursor_shown == 1) {
        rtl_vga_get_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1, g_mouse_y);
        rtl_vga_get_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1, g_mouse_y);
        t9 = FPOFF(g_cursor_sprites(g_cursor_shape));
        FR16(fr_, 16, 0xC) = FPSEG(g_cursor_sprites(g_cursor_shape));
        FR16(fr_, 16, 0xE) = t9;
        t10 = FPOFF(g_cursor_comp);
        FR16(fr_, 16, 0x6) = FPSEG(g_cursor_comp);
        FR16(fr_, 16, 0x8) = t10;
        FR16(fr_, 16, 0x4) = 0;
        FR16(fr_, 16, 0x2) = 0;
        FR16(fr_, 16, 0xA) = g_mouse_x & 7;
        arrow_cover(g_cursor_comp, 0);  /* (--mouse system, fixes.c) */
        rtl_sprite_put((fr_ + 16 - 0xE));
        arrow_uncover();
        rtl_vga_put_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1, g_mouse_y);
        g_cursor_drawn_x = g_mouse_x;
        t16 = g_mouse_y;
        g_cursor_drawn_y = g_mouse_y;
        ax = t16;
    } else if (g_cursor_shown == 0) {
        ax = g_drag_image_lo | g_drag_image_hi;
        if ((g_drag_image_lo | g_drag_image_hi) != 0) {
            si = g_mouse_x - g_drag_hot_x;
            di = g_mouse_y - g_drag_hot_y;
            if (((i16)(g_mouse_x - g_drag_hot_x)) >= 0) {
                if (((i16)(si + g_drag_w)) > 319) {
                    si = 0x13F - g_drag_w;
                }
            } else {
                si = 0;
            }
            if (((i16)di) >= 0) {
                if (((i16)(di + g_drag_h)) > 120) {
                    di = 0x78 - g_drag_h;
                }
            } else {
                di = 0;
            }
            rtl_vga_get_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)si) >> 1)) >> 1)) >> 1, di);
            rtl_vga_get_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)si) >> 1)) >> 1)) >> 1, di);
            drag_cover(g_cursor_comp);   /* (a network game: the window draws it at the pointer, fixes.c) */
            blit_transparent(g_drag_image, g_cursor_comp, si & 7, 0, 0, 0, g_drag_w, g_drag_h, 0 /* unused ax */);
            drag_uncover();
            t26 = rtl_vga_put_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)si) >> 1)) >> 1)) >> 1, di);
            g_cursor_drawn_x = si;
            g_cursor_drawn_y = di;
            ax = LO16(t26);
        }
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 105E:01AE  --g_cursor_shown; when the cursor (or drag image) disappears restore the saved VGA background at its last position */
u16 cursor_hide(void) {
    u16 t4;
    FUNC_TICK(28);
    g_cursor_shown = g_cursor_shown - 1;
    if (!(g_cursor_drawn_x == 0xFFFF || ((g_cursor_shown != 0 || (g_drag_image_lo | g_drag_image_hi) != 0) && (g_cursor_shown != 0xFFFF || (g_drag_image_lo | g_drag_image_hi) == 0)))) {
        rtl_vga_put_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1, g_cursor_drawn_y);
    }
    t4 = g_cursor_shown;
    G32(0x7AC2) = MK32(((i16)g_cursor_shown) >> 0xF, g_cursor_shown);
    return t4;
}

/* 105E:0203  move = *pending/5 (at least +-1); *pos += move; *pending -= move (smooth cursor movement) */
void mouse_ease_step(u32 a0, u32 a2) {
    u16 si;
    FUNC_TICK(41);
    if (((i16)FP16(a0, 0)) <= 0) {
        if (((i16)FP16(a0, 0)) >= 0) {
            si = 0;
        } else if (((i16)FP16(a0, 0)) <= -5) {
            si = (u16)((u16)((i32)(i16)FP16(a0, 0) / 5));
        } else {
            si = 0xFFFF;
        }
    } else if (((i16)FP16(a0, 0)) >= 5) {
        si = (u16)((u16)((i32)(i16)FP16(a0, 0) / 5));
    } else {
        si = 1;
    }
    FP16(a2, 0) = FP16(a2, 0) + si;
    FP16(a0, 0) = FP16(a0, 0) - si;
    return;
}

/* 105E:0266  draw a 0-terminated string at pixel x,y into a bitmap with the 8px glyph font g_font (char->glyph map 0x12E, advance widths 0x22E, space = 3px, unmapped chars -> '.'); flag 1 = digits right-aligned in fixed cells; uses the small font of room slot 0x108 when g_font_alt */
void text_draw_2(u16 a0, u16 a1, u32 a2, u32 a4, u16 a6) {
    u16 bx;
    int cc;
    u16 di;
    u16 es;
    u16 si;
    u16 t5;
    u8 fr_[20] = {0};                  /* bytes bp-20 .. bp-1 of the original frame */
    FUNC_TICK(28);
    FR16(fr_, 20, 0x6) = FPSEG(a4);
    FR16(fr_, 20, 0x8) = FPOFF(a4);
    FR16(fr_, 20, 0x12) = ((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1;
    t5 = (u16)((i32)(i16)a0 % 8);
    FR16(fr_, 20, 0x10) = 0;
    di = t5;
    while ((FP8(a2, FR16(fr_, 20, 0x10))) != 0) {
        LOOP_TICK(82);
        si = FP8(a2, FR16(fr_, 20, 0x10));
        if ((FP8(a2, FR16(fr_, 20, 0x10))) != 0x20) {
            if (g_font_map(si) == 0) {
                si = 0x2E;
            }
            if (a6 == 1) {
                if (((i16)si) >= 48) {
                    if (((i16)si) <= 57) {
                        if (g_glyph_width(si) < 5) {
                            cc = ((i16)(di + (5 - g_glyph_width(si)))) <= 8;
                            di = di + (5 - g_glyph_width(si));
                            if (!(cc)) {
                                FR16(fr_, 20, 0x12) = FR16(fr_, 20, 0x12) + 1;
                                di = di - 8;
                            }
                        }
                    }
                }
            }
            if (g_font != 0) {
                bx = (FPOFF(g_room_gfx2) + (g_font_map(si) << 3)) + 0xFFFA;
                es = FPSEG(g_room_gfx2);
            } else {
                bx = (FPOFF(g_font_2) + (g_font_map(si) << 3)) + 0xFFFA;
                es = FPSEG(g_font_2);
            }
            FR16(fr_, 20, 0xC) = es;
            FR16(fr_, 20, 0xE) = bx;
            FR16(fr_, 20, 0x4) = FR16(fr_, 20, 0x12);
            FR16(fr_, 20, 0x2) = a1;
            FR16(fr_, 20, 0xA) = di;
            rtl_glyph_put((fr_ + 20 - 0xE));
            if (a6 == 0) {
L_0350:
                di = di + (g_glyph_width(si) + 1);
            } else if (((i16)si) < 48) {
                goto L_0350;
            } else if (((i16)si) > 57) {
                goto L_0350;
            } else if (g_glyph_width(si) >= 5) {
                di = di + 5;
            } else {
                goto L_0350;
            }
        } else {
            di = di + 3;
        }
        if (((i16)di) > 8) {
            FR16(fr_, 20, 0x12) = FR16(fr_, 20, 0x12) + 1;
            di = di - 8;
        }
        FR16(fr_, 20, 0x10) = FR16(fr_, 20, 0x10) + 1;
    }
    return;
}

/* 105E:0386  draw window frame + filled interior on a bitmap, rect in 8px cells (x0,y0,x1,y1,bmp,style) */
u16 ui_frame(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
    FUNC_TICK(39);
    a2 = a2 << 3;
    a3 = a3 << 3;
    return ui_frame_px(a0 << 3, a1 << 3, a2, a3, MK32(a5, a4), a6);
}

/* 105E:03DA  window frame in pixels: interior tile 0, edges top 0x30 bottom 0x28 left 0x40 right 0x38, corners 8/0x10/0x18/0x20 from tileset 0x7B0A (style 1) or 0x7B0E */
u16 ui_frame_px(u16 a0, u16 a1, u16 a2, u16 a3, u32 a4, u16 a6) {
    u16 bx;
    u16 di;
    u16 es;
    u16 si;
    FUNC_TICK(51);
    g_frame_target = a4;
    if (a6 != 1) {
        bx = FPOFF(g_frame_tiles2);
        es = FPSEG(g_frame_tiles2);
    } else {
        bx = FPOFF(g_frame_tiles1);
        es = FPSEG(g_frame_tiles1);
    }
    g_frame_tileset = MK32(es, bx);
    si = a0 + 8;
    while (((i16)si) < ((i16)a2)) {
        LOOP_TICK(15);
        di = a1 + 8;
        while (((i16)di) < ((i16)a3)) {
            LOOP_TICK(9);
            ui_frame_tile(0, si, di);
            di = di + 8;
        }
        si = si + 8;
    }
    si = a0 + 8;
    while (((i16)si) < ((i16)a2)) {
        LOOP_TICK(15);
        ui_frame_tile(0x30, si, a1);
        ui_frame_tile(0x28, si, a3);
        si = si + 8;
    }
    si = a1 + 8;
    while (((i16)si) < ((i16)a3)) {
        LOOP_TICK(15);
        ui_frame_tile(0x40, a0, si);
        ui_frame_tile(0x38, a2, si);
        si = si + 8;
    }
    ui_frame_tile(8, a0, a1);
    ui_frame_tile(0x10, a2, a1);
    ui_frame_tile(0x18, a2, a3);
    return ui_frame_tile(0x20, a0, a3);
}

/* 105E:04DB  draw one 8px frame tile (atlas offset) at pixel x,y into g_frame_target via the glyph blitter 377A */
u16 ui_frame_tile(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(28);
    g_glyph_blit = MK32(FPSEG(g_frame_tileset), FPOFF(g_frame_tileset) + a0);
    G16(0x8F3A) = (u16)((u16)((i32)(i16)a1 % 8));
    G32(0x8F3C) = g_frame_target;
    G16(0x8F40) = (u16)((u16)((i32)(i16)a1 / 8));
    G16(0x8F42) = a2;
    return LO16(rtl_glyph_put(HPTR(MK32(DSEG, 0x8F36))));
}

/* 105E:0526  style 1: draw framed box (cells x0..x1,y0..y1) into the screen buffer and print word-wrapped text inside (0DDD:0239) */
void ui_text_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
    u16 si;
    FUNC_TICK(43);
    si = a0;
    if (a6 == 1) {
        ui_frame(si, a1, a2, a3, g_screen_buf_lo, g_screen_buf_hi, 1);
        text_draw_ex_free((si << 3) + 4, (a1 << 3) + 0xFFFC, a4, a5, a2 - si, 1, g_screen_buf_lo, g_screen_buf_hi);
    }
    return;
}

/* 105E:058C  get popup slot (mode 1/3: first free slot >= 1, mode 2: slot 0), allocate and save the screen-buffer background under the cell rect; mode 1 also draws ui_text_box and presents; returns slot */
u16 popup_open(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
    u16 ax;
    u16 di;
    u16 si;
    u32 t9;
    FUNC_TICK(104);
    si = 1;
    di = a6;
    if (a6 == 1) {
        for (;;) {
L_05A4:
            LOOP_TICK(7);
            if ((g_popup_bmp_lo(si) | g_popup_bmp_hi(si)) == 0) break;
            si = si + 1;
        }
        G32(0x7ACE) = MK32(((i16)si) >> 0xF, si);
    } else if (di != 3) {
        if (di == 2) {
            si = 0;
        }
    } else {
        goto L_05A4;
    }
    t9 = bitmap_alloc(((a2 - a0) + 1) << 3, ((a3 - a1) + 1) << 3);
    g_popup_bmp(si) = t9;
    if (di == 1) {
L_0606:
        g_popup_col(si) = a0;
        g_popup_row(si) = a1;
        g_popup_w(si) = (a2 - a0) + 1;
        g_popup_h(si) = (a3 - a1) + 1;
        g_popup_used(si) = 1;
        grab_screen(g_popup_bmp(si), a0 << 3, a1 << 3);
        if (di == 1) {
            ui_text_box(a0, a1, a2, a3, a4, a5, di);
            present_frame();
        }
    } else {
        if (di == 3) goto L_0606;
    }
    ax = si;
    return ax;
}

/* 105E:069A  window_create + print text + window_show (drawn directly on VGA) */
void ui_message_window(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(32);
    window_create(a0, a1, a2, a3);
    text_draw_ex_free(4, 0xFFFC, a4, a5, a2 - a0, 1, g_popup_bmp_lo(0), g_popup_bmp_hi(0));
    window_show(a0, a1);
    return;
}

/* 105E:06EA  put the saved background of popup slot back into the screen buffer, present, free the save buffer */
u16 popup_close(u16 a0, u16 ax) {
    int cc;
    u16 si;
    u16 t16;
    FUNC_TICK(53);
    si = a0;
    if (g_popup_used(a0) == 1) {
        cc = (g_popup_bmp_lo(si) | g_popup_bmp_hi(si)) == 0;
        ax = g_popup_bmp_lo(si) | g_popup_bmp_hi(si);
        if (!(cc)) {
            blit_opaque_to(g_popup_bmp(si), g_screen_buf_lo, g_screen_buf_hi, g_popup_col(si) << 3, g_popup_row(si) << 3);
            present_frame();
            t16 = mem_free(g_popup_bmp_lo(si), g_popup_bmp_hi(si));
            g_popup_bmp(si) = 0;
            ax = t16;
        }
    }
    return ax;
}

/* 105E:0778  ? redraw saved background of a popup slot into the screen buffer without freeing (unused) */
void popup_restore(u16 a0) {
    u16 si;
    FUNC_TICK(40);
    si = a0;
    if (g_popup_used(a0) == 1) {
        if ((g_popup_bmp_lo(si) | g_popup_bmp_hi(si)) != 0) {
            blit_opaque_to(g_popup_bmp(si), g_screen_buf_lo, g_screen_buf_hi, g_popup_col(si) << 3, g_popup_row(si) << 3);
        }
    }
    return;
}

/* 105E:07D9  draw the frame around the 320x120 room view from room-frame tiles (res 4) into the screen buffer */
void room_draw_border(void) {
    u16 si;
    FUNC_TICK(78);
    g_clip_on = 0;
    g_tile_atlas = g_roomframe_tiles;
    g_tile_target = g_screen_buf;
    si = 8;
    while (((i16)si) <= 304) {
        LOOP_TICK(17);
        tile_draw_opaque(0x52, si, 0);
        tile_draw_opaque(0xA, si, 0x72);
        si = si + 8;
    }
    si = 8;
    while (((i16)si) < 110) {
        LOOP_TICK(17);
        tile_draw_opaque(0x1A, 0, si);
        tile_draw_opaque(0x32, 0x138, si);
        si = si + 6;
    }
    tile_draw_opaque(0x4A, 0, 0);
    tile_draw_opaque(0x5A, 0x138, 0);
    tile_draw_opaque(2, 0, 0x72);
    tile_draw_opaque(0x12, 0x138, 0x72);
    tile_draw_opaque(0x22, 0, 4);
    tile_draw_opaque(0x2A, 0, 0x6E);
    tile_draw_opaque(0x3A, 0x138, 4);
    tile_draw_opaque(0x42, 0x138, 0x6E);
    return;
}

/* 105E:08F1  clear the 320x120 view and draw its frame from the current room atlas (slot 0x104) */
void room_draw_border_alt(void) {
    u16 si;
    FUNC_TICK(91);
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
    g_tile_atlas = g_room_gfx1;
    g_tile_target = g_screen_buf;
    si = 1;
    while (((i16)si) <= 38) {
        LOOP_TICK(25);
        tile_draw_opaque(0x52, si << 3, 0);
        tile_draw_opaque(0xA, si << 3, 0x75);
        si = si + 1;
    }
    si = 8;
    while (((i16)si) < 110) {
        LOOP_TICK(17);
        tile_draw_opaque(0x1A, 0, si);
        tile_draw_opaque(0x32, 0x138, si);
        si = si + 4;
    }
    tile_draw_opaque(0x4A, 0, 0);
    tile_draw_opaque(0x5A, 0x138, 0);
    tile_draw_opaque(2, 0, 0x75);
    tile_draw_opaque(0x12, 0x138, 0x75);
    tile_draw_opaque(0x22, 0, 4);
    tile_draw_opaque(0x2A, 0, 0x71);
    tile_draw_opaque(0x3A, 0x138, 4);
    tile_draw_opaque(0x42, 0x138, 0x71);
    return;
}

/* 105E:0A31  n * 8 - 6 */
u16 col_to_px(u16 a0) {
    u16 ax;
    FUNC_TICK(10);
    ax = (a0 << 3) + 0xFFFA;
    return ax;
}

/* 105E:0A44  atlas offset of a figure sprite: (figure.look(+0xA) * 10 + frame) * 8 - 6 */
u16 figure_sprite_ofs(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(20);
    ax = ((((G16((a0 * 0x1E) + 0x7B4C)) * 0xA) + a1) << 3) + 0xFFFA;
    return ax;
}

/* 105E:0A72  draw a sprite into g_drag_image at 0,0 (icon of the item carried by the mouse) */
void drag_image_draw(u32 a0) {
    FUNC_TICK(26);
    blit_transparent(a0, g_drag_image, 0, 0, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0 /* unused ax */);
    return;
}

/* 105E:0AAC  draw a sprite into a given buffer at x, y */
void draw_sprite_to(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(24);
    blit_transparent(a0, MK32(a3, a2), a4, a5, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0 /* unused ax */);
    return;
}

/* 105E:0AE4  opaque copy of a whole sprite into bitmap dst at x,y (clipped) */
void blit_opaque_to(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(24);
    blit_opaque_rect(a0, MK32(a3, a2), a4, a5, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0 /* unused ax */);
    return;
}

/* 105E:0B1C  opaque copy of a whole sprite into the screen buffer at x,y */
u16 blit_opaque(u32 a0, u16 a2, u16 a3) {
    FUNC_TICK(24);
    return blit_opaque_rect(a0, g_screen_buf, a2, a3, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0);
}

/* 105E:0B56  copy the screen-buffer area at x,y (size of dst) into bitmap dst (save background under an animation) */
u16 grab_screen(u32 a0, u16 a2, u16 a3) {
    FUNC_TICK(24);
    return blit_opaque_rect(g_screen_buf, a0, 0, 0, a2, a3, FP16(a0, 0) << 3, FP16(a0, 2), 0);
}

/* 105E:0B90  opaque draw of tile (atlas offset) of g_tile_atlas into g_tile_target at x,y */
void tile_draw_opaque(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(31);
    blit_opaque_rect(MK32(g_tile_atlas_hi, g_tile_atlas_lo + a0), g_tile_target, a1, a2, 0, 0, FP16(g_tile_atlas, a0) << 3, FP16(g_tile_atlas, a0 + 2), 0 /* unused ax */);
    return;
}

/* 105E:0BD9  draw tile n of the current tile atlas at x, y */
u16 draw_atlas_tile(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(31);
    return blit_transparent(MK32(g_tile_atlas_hi, g_tile_atlas_lo + a0), g_tile_target, a1, a2, 0, 0, FP16(g_tile_atlas, a0) << 3, FP16(g_tile_atlas, a0 + 2), g_tile_atlas_lo + a0);
}

/* 105E:0C22  transparent draw of a sprite into g_tile_target at x,y */
void sprite_draw_target(u32 a0, u16 a2, u16 a3) {
    FUNC_TICK(24);
    blit_transparent(a0, g_tile_target, a2, a3, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0 /* unused ax */);
    return;
}

/* 105E:0C5C  transparent atlas tile into g_tile_target at x, y = g_tile_fixed_y */
void tile_draw_at_x(u16 a0, u16 a1) {
    FUNC_TICK(31);
    blit_transparent(MK32(g_tile_atlas_hi, g_tile_atlas_lo + a0), g_tile_target, a1, g_tile_fixed_y, 0, 0, FP16(g_tile_atlas, a0) << 3, FP16(g_tile_atlas, a0 + 2), 0 /* unused ax */);
    return;
}

/* 105E:0CA6  transparent atlas tile at x = g_tile_fixed_x, y (unused) */
void tile_draw_at_y(u16 a0, u16 a1) {
    FUNC_TICK(31);
    blit_transparent(MK32(g_tile_atlas_hi, g_tile_atlas_lo + a0), g_tile_target, g_tile_fixed_x, a1, 0, 0, FP16(g_tile_atlas, a0) << 3, FP16(g_tile_atlas, a0 + 2), 0 /* unused ax */);
    return;
}

/* 105E:0CF0  transparent tile g_tile_fixed_n at x,y (unused) */
void tile_draw_fixed(u16 a0, u16 a1) {
    FUNC_TICK(29);
    blit_transparent(MK32(g_tile_atlas_hi, g_tile_atlas_lo + g_tile_fixed_n), g_tile_target, a0, a1, 0, 0, FP16(g_tile_atlas, g_tile_fixed_n) << 3, FP16(g_tile_atlas, g_tile_fixed_n + 2), 0 /* unused ax */);
    return;
}

/* 105E:0D3B  draw a sprite at x, y into the screen buffer */
u16 draw_sprite(u32 a0, u16 a2, u16 a3) {
    FUNC_TICK(24);
    return blit_transparent(a0, g_screen_buf, a2, a3, 0, 0, FP16(a0, 0) << 3, FP16(a0, 2), 0);
}

/* 105E:0D75  opaque copy of a whole bitmap into the screen buffer at x (multiple of 8), y */
u16 bitmap_put_screen(u32 a0, u16 a2, u16 a3) {
    u32 rv_;
    u16 t1;
    u8 fr_[14] = {0};                  /* bytes bp-14 .. bp-1 of the original frame */
    FUNC_TICK(24);
    FR16(fr_, 14, 0xA) = FPSEG(a0);
    FR16(fr_, 14, 0xC) = FPOFF(a0);
    t1 = FPOFF(g_screen_buf);
    FR16(fr_, 14, 0x6) = FPSEG(g_screen_buf);
    FR16(fr_, 14, 0x8) = t1;
    FR16(fr_, 14, 0x4) = ((i16)(((i16)(((i16)a2) >> 1)) >> 1)) >> 1;
    FR16(fr_, 14, 0x2) = a3;
    rv_ = LO16(rtl_bitmap_put((fr_ + 14 - 0xC)));
    return (u16)rv_;
}

/* 105E:0DB1  blit (src,dst,x,y,sx,sy,w,h), colour 0 transparent, w rounded to 8; clips only vertically (g_clip_y1..y2 when g_clip_on) and sy<0; byte-aligned or shifted asm path (3786/37A2) */
u16 blit_transparent(u32 a0, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8, u16 a9, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u32 rv_;
    u16 t2;
    u8 fr_[44] = {0};                  /* bytes bp-44 .. bp-1 of the original frame */
    FUNC_TICK(112);
    si = a5;
    di = a7;
    if (g_clip_on == 1) {
        if (((i16)si) >= ((i16)g_clip_y1)) {
            cc = ((i16)(si + a9)) <= ((i16)g_clip_y2);
            ax = si + a9;
            if (!(cc)) {
                a9 = g_clip_y2 - si;
                ax = (u16)(g_clip_y2 - si);
            }
        } else {
            t2 = di + (g_clip_y1 - si);
            a9 = a9 - (g_clip_y1 - si);
            ax = (u16)(g_clip_y1 - si);
            si = g_clip_y1;
            di = t2;
        }
        if (((i16)di) < 0) {
            a9 = a9 + di;
            si = (u16)(si - di);
            di = 0;
        }
    }
    if (((i16)a9) > 0 && ((i16)a8) > 0) {
        a8 = (a8 + 7) & 0xFFF8;
        if ((a4 & 7) != 0) {
            FR16(fr_, 44, 0x28) = FPSEG(a0);
            FR16(fr_, 44, 0x2A) = FPOFF(a0);
            FR16(fr_, 44, 0x1A) = FPSEG(a2);
            FR16(fr_, 44, 0x1C) = FPOFF(a2);
            FR16(fr_, 44, 0x18) = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x16) = si;
            FR16(fr_, 44, 0x24) = ((i16)(((i16)(((i16)a6) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x22) = di;
            FR16(fr_, 44, 0x20) = ((i16)(((i16)(((i16)a8) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x1E) = a9;
            FR16(fr_, 44, 0x26) = a4 & 7;
            ax = LO16(rtl_blit_transparent_shifted((fr_ + 44 - 0x2A)));
        } else {
            FR16(fr_, 44, 0x12) = FPSEG(a0);
            FR16(fr_, 44, 0x14) = FPOFF(a0);
            FR16(fr_, 44, 0x6) = FPSEG(a2);
            FR16(fr_, 44, 0x8) = FPOFF(a2);
            FR16(fr_, 44, 0x4) = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x2) = si;
            FR16(fr_, 44, 0x10) = ((i16)(((i16)(((i16)a6) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0xE) = di;
            FR16(fr_, 44, 0xC) = ((i16)(((i16)(((i16)a8) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0xA) = a9;
            ax = LO16(rtl_blit_transparent_aligned((fr_ + 44 - 0x14)));
        }
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 105E:0ED3  opaque copy of a whole bitmap into another at x (multiple of 8), y (375B) */
void bitmap_put(u32 a0, u32 a2, u16 a4, u16 a5) {
    u8 fr_[14] = {0};                  /* bytes bp-14 .. bp-1 of the original frame */
    FUNC_TICK(24);
    FR16(fr_, 14, 0xA) = FPSEG(a0);
    FR16(fr_, 14, 0xC) = FPOFF(a0);
    FR16(fr_, 14, 0x6) = FPSEG(a2);
    FR16(fr_, 14, 0x8) = FPOFF(a2);
    FR16(fr_, 14, 0x4) = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
    FR16(fr_, 14, 0x2) = a5;
    rtl_bitmap_put((fr_ + 14 - 0xC));
    return;
}

/* 105E:0F0E  bitmap_put(src, dst, 0, 0) taking split far pointers */
void bitmap_copy(u16 a0, u16 a1, u16 a2, u16 a3) {
    FUNC_TICK(15);
    bitmap_put(MK32(a1, a0), MK32(a3, a2), 0, 0);
    return;
}

/* 105E:0F2B  opaque blit (src,dst,x,y,sx,sy,w,h), vertical clipping only; asm 3762/376D */
u16 blit_opaque_rect(u32 a0, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8, u16 a9, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u32 rv_;
    u16 t2;
    u8 fr_[44] = {0};                  /* bytes bp-44 .. bp-1 of the original frame */
    FUNC_TICK(108);
    si = a5;
    di = a7;
    if (g_clip_on == 1) {
        if (((i16)si) >= ((i16)g_clip_y1)) {
            cc = ((i16)(si + a9)) <= ((i16)g_clip_y2);
            ax = si + a9;
            if (!(cc)) {
                a9 = g_clip_y2 - si;
                ax = (u16)(g_clip_y2 - si);
            }
        } else {
            t2 = di + (g_clip_y1 - si);
            a9 = a9 - (g_clip_y1 - si);
            ax = (u16)(g_clip_y1 - si);
            si = g_clip_y1;
            di = t2;
        }
        if (((i16)di) < 0) {
            a9 = a9 + di;
            si = (u16)(si - di);
            di = 0;
        }
    }
    if (((i16)a9) > 0 && ((i16)a8) > 0) {
        if ((a4 & 7) != 0) {
            FR16(fr_, 44, 0x28) = FPSEG(a0);
            FR16(fr_, 44, 0x2A) = FPOFF(a0);
            FR16(fr_, 44, 0x1A) = FPSEG(a2);
            FR16(fr_, 44, 0x1C) = FPOFF(a2);
            FR16(fr_, 44, 0x18) = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x16) = si;
            FR16(fr_, 44, 0x24) = ((i16)(((i16)(((i16)a6) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x22) = di;
            FR16(fr_, 44, 0x20) = ((i16)(((i16)(((i16)a8) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x1E) = a9;
            FR16(fr_, 44, 0x26) = a4 & 7;
            ax = LO16(rtl_blit_opaque_shifted((fr_ + 44 - 0x2A)));
        } else {
            FR16(fr_, 44, 0x12) = FPSEG(a0);
            FR16(fr_, 44, 0x14) = FPOFF(a0);
            FR16(fr_, 44, 0x6) = FPSEG(a2);
            FR16(fr_, 44, 0x8) = FPOFF(a2);
            FR16(fr_, 44, 0x4) = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0x2) = si;
            FR16(fr_, 44, 0x10) = ((i16)(((i16)(((i16)a6) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0xE) = di;
            FR16(fr_, 44, 0xC) = ((i16)(((i16)(((i16)a8) >> 1)) >> 1)) >> 1;
            FR16(fr_, 44, 0xA) = a9;
            ax = LO16(rtl_blit_opaque_aligned((fr_ + 44 - 0x14)));
        }
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 105E:1041  append figure to the elevator call list (g_elevator_queue) unless already present */
void elevator_queue_add(u16 a0) {
    u16 di;
    u16 si;
    FUNC_TICK(17);
    si = 0;
    di = a0;
    while (g_elevator_queue(si) != di) {
        LOOP_TICK(8);
        if (((i16)si) >= ((i16)g_elevator_calls)) break;
        si = si + 1;
    }
    if (((i16)si) >= ((i16)g_elevator_calls)) {
        g_elevator_queue(g_elevator_calls) = di;
        g_elevator_calls = g_elevator_calls + 1;
    }
    return;
}

/* 105E:1078  remove figure from the elevator call list, shifting the rest */
void elevator_queue_remove(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(15);
    si = 0;
    di = a0;
    while (g_elevator_queue(si) != di) {
        LOOP_TICK(8);
        if (((i16)si) >= ((i16)g_elevator_calls)) break;
        si = si + 1;
    }
    if (((i16)si) < ((i16)g_elevator_calls)) {
        for (;;) {
            LOOP_TICK(15);
            g_elevator_queue(si) = g_elevator_queue(si + 1);
            if (((i16)si) >= ((i16)g_elevator_calls)) {
                ax = 0;
            } else {
                ax = 1;
            }
            if (ax == 0) break;
            si = si + 1;
        }
        g_elevator_calls = g_elevator_calls - 1;
    }
    return;
}

/* 105E:10CA  step *door downward (wrapping 3..0) until it is a real door (type 1..3) on that floor and != exclude (-1 = none) */
void floor_pick_door(u16 a0, u8 *a1, u16 a3) {
    u16 di;
    u16 si;
    FUNC_TICK(11);
    si = a0;
    di = a3;
    for (;;) {
        LOOP_TICK(47);
        if ((floors_door_kind(si, HP16(a1, 0))) < 1) {
L_10D7:
            HP16(a1, 0) = HP16(a1, 0) - 1;
            if (((i16)HP16(a1, 0)) >= 0) continue;
            HP16(a1, 0) = 3;
            continue;
        }
        if ((floors_door_kind(si, HP16(a1, 0))) > 3) goto L_10D7;
        if (di == 0xFFFF) break;
        if (HP16(a1, 0) != di) break;
        goto L_10D7;
    }
    return;
}

/* 105E:114D  number of consecutive doors of type 1..3 on a floor (from door 0) */
u16 floor_door_count(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(13);
    si = 0;
    di = a0;
    while (floors_door_kind(di, si) >= 1) {
        LOOP_TICK(21);
        if (floors_door_kind(di, si) > 3) break;
        si = si + 1;
    }
    ax = si;
    return ax;
}

/* 105E:1196  clip rect 0,0,24,11 (cursor save area) */
void clip_cursor_cell(void) {
    FUNC_TICK(6);
    G32(0x386) = 1;
    G32(0x38A) = 0x180000;
    g_clip_y2 = 0xB;
    return;
}

/* 105E:11B5  clip rectangle = 0,0,320,120 */
void clip_reset(void) {
    FUNC_TICK(6);
    G32(0x386) = 1;
    G32(0x38A) = 0x1400000;
    g_clip_y2 = 0x78;
    return;
}

/* 105E:11D4  clip rect 0,0,320,80 (bottom panel bitmap) */
void clip_panel(void) {
    FUNC_TICK(4);
    clip_reset();
    g_clip_y2 = 0x50;
    return;
}

/* 105E:11DF  mouse x relative to the active window */
u16 cursor_x(void) {
    u16 ax;
    FUNC_TICK(17);
    if (g_cursor_shape != 2 || (g_drag_image_lo | g_drag_image_hi) != 0) {
        if ((g_drag_image_lo | g_drag_image_hi) == 0) {
            ax = g_mouse_x;
        } else {
            ax = g_mouse_x - g_drag_hot_x;
        }
    } else {
        ax = g_mouse_x + 5;
    }
    return ax;
}

/* 105E:1213  mouse y relative to the active window */
u16 cursor_y(void) {
    u16 ax;
    FUNC_TICK(17);
    if (g_cursor_shape != 2 || (g_drag_image_lo | g_drag_image_hi) != 0) {
        if ((g_drag_image_lo | g_drag_image_hi) == 0) {
            ax = g_mouse_y;
        } else {
            ax = g_mouse_y - g_drag_hot_y;
        }
    } else {
        ax = g_mouse_y + 5;
    }
    return ax;
}

/* 105E:1247  if figure +0x16 (queued) is set clear it and remove it from the elevator list (unused) */
void figure_unqueue_elevator(u16 a0) {
    u16 si;
    FUNC_TICK(29);
    si = a0;
    if ((figures(a0).lift_phase) != 0) {
        figures(si).lift_phase = 0;
        elevator_queue_remove(si);
    }
    return;
}

/* 105E:1287  draw the strips left/right of the 208px building (x 0..56, 264..320, y 4..112) in variant g_daylight (1 day tiles 0xA/0xD/0xC, 2 night 0xB/0xF/0xE of res 13) */
void building_draw_sides(u16 a0) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    FUNC_TICK(24);
    clip_reset();
    if (a0 != 1) {
        l_6 = 0xB;
        l_4 = 0xF;
        l_2 = 0xE;
    } else {
        l_6 = 0xA;
        l_4 = 0xD;
        l_2 = 0xC;
    }
    di = 4;
    while (((i16)di) < 113) {
        LOOP_TICK(75);
        t1 = col_to_px(l_4);
        draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t1), 0, di);
        t2 = col_to_px(l_2);
        draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t2), 0x138, di);
        si = 8;
        while (((i16)si) < 56) {
            LOOP_TICK(18);
            t3 = col_to_px(l_6);
            draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t3), si, di);
            si = si + 8;
        }
        si = 0x108;
        while (((i16)si) < 312) {
            LOOP_TICK(18);
            t4 = col_to_px(l_6);
            draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t4), si, di);
            si = si + 8;
        }
        di = di + 1;
    }
    return;
}

/* 105E:1366  draw door (floor, door) open (1) or closed (2) into g_building_buf; sprite pair by door type 1..3 (res 6); x = door.x-56, y = 150-(floor-g_bld_top_floor+4)*30-26; building view only */
void door_draw(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t4;
    u16 t7;
    u16 t10;
    u16 t13;
    u16 t16;
    u8 fr_[8] = {0};                  /* bytes bp-8 .. bp-1 of the original frame */
    FUNC_TICK(135);
    seat_req_door(a0, a1, a2);         /* seats: the other seats draw it too (src/seatctx.c) */
    si = a0;
    if (g_room == ROOM_BUILDING) {
        G32(0x386) = 1;
        G32(0x38A) = 0xD00000;
        g_clip_y2 = 0x96;
        cc = floors_door_kind(si, a1) == 1;
        ax = floors_door_kind(si, a1);
        di = (0x96 - (((si - g_bld_top_floor) + 4) * 0x1E)) + 0xFFE6;
        if (cc) {
            t1 = FPOFF(g_bld_tiles) + 0x42;
            FR16(fr_, 8, 0x2) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x4) = t1;
            t4 = FPOFF(g_bld_tiles) + 0x4A;
            FR16(fr_, 8, 0x6) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x8) = t4;
        } else if (ax == 2) {
            t7 = FPOFF(g_bld_tiles) + 0x52;
            FR16(fr_, 8, 0x2) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x4) = t7;
            t10 = FPOFF(g_bld_tiles) + 0x5A;
            FR16(fr_, 8, 0x6) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x8) = t10;
        } else if (ax == 3) {
            t13 = FPOFF(g_bld_tiles) + 0x62;
            FR16(fr_, 8, 0x2) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x4) = t13;
            t16 = FPOFF(g_bld_tiles) + 0x6A;
            FR16(fr_, 8, 0x6) = FPSEG(g_bld_tiles);
            FR16(fr_, 8, 0x8) = t16;
        }
        ax = a2;
        if (a2 == 1) {
            blit_transparent(FR32(fr_, 8, 0x8), g_building_buf, floors_door_x(si, a1) + 0xFFC8, di, 0, 0, 0x18, 0x18, 0 /* unused ax */);
        } else if (ax == 2) {
            blit_transparent(FR32(fr_, 8, 0x4), g_building_buf, floors_door_x(si, a1) + 0xFFC8, di, 0, 0, 0x18, 0x18, 0 /* unused ax */);
        }
    }
    return;
}

/* 105E:14CA  compose the bottom panel bitmap (320x80): res 9 halves at x 0 and 120, res 15 sprite, res 10 parts at 201/305 */
void panel_build(void) {
    FUNC_TICK(101);
    rtl_bitmap_fill_w(W2(g_panel_bmp), 0, 0, 0x28, 0x50, 0);
    bitmap_put(MK32(g_panel_sprites_hi, g_panel_sprites_lo + 2), g_panel_bmp, 0, 2);
    draw_sprite_to(MK32(g_tvfam_atlas_hi, g_tvfam_atlas_lo + 2), g_panel_bmp_lo, g_panel_bmp_hi, 0xD, 9);
    bitmap_put(MK32(g_panel_sprites_hi, g_panel_sprites_lo + 0xA), g_panel_bmp, 0x78, 2);
    g_tile_target = g_panel_bmp;
    g_tile_atlas = g_panel_atlas;
    draw_atlas_tile(2, 0xC9, 2);
    draw_atlas_tile(0xA, 0xC9, 0x3A);
    draw_atlas_tile(0x12, 0xC9, 0xB);
    draw_atlas_tile(0x1A, 0x131, 0xB);
    return;
}

/* 105E:15B6  draw a figure (person, 16x19) into the screen buffer if its floor is one of the 5 visible floors; y from floor and g_bld_scroll, 1px walking bob */
void figure_draw(u16 a0, u16 a1) {
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    FUNC_TICK(127);
    si = a0;
    if (((i16)(figures(a0).floor)) <= ((i16)g_bld_top_floor)) {
        if (((i16)(figures(si).floor)) >= ((i16)(g_bld_top_floor + 0xFFFC))) {
            cc = (figures(si).motion) != 2;
            di = (((0x96 - ((((figures(si).floor) - g_bld_top_floor) + 4) * 0x1E)) + 0xFFEB) - ((g_bld_top_floor * 0x1E) - (g_bld_scroll + 0x6D))) + 4;
            if (cc) {
L_1670:
                if ((figures(si).motion) == 4) {
                    if (((i16)(figures(si).anim)) <= 2) {
L_16A0:
                        di = di - 1;
                    }
                }
            } else if (((i16)(figures(si).anim)) >= 4) {
                goto L_16A0;
            } else {
                goto L_1670;
            }
            t1 = figures(si).x;
            t2 = g_screen_buf_hi;
            t3 = g_screen_buf_lo;
            t4 = figure_sprite_ofs(si, a1);
            blit_transparent(MK32(g_figure_sprites_hi, g_figure_sprites_lo + t4), MK32(t2, t3), t1, di, 0, 0, 0x10, 0x13, 0 /* unused ax */);
        }
    }
    return;
}

/* 105E:16EE  mouse_poll, then keyboard: Enter = left click (1), Esc = right click (2), arrow keys move the pending cursor motion with acceleration 2..16 (+2 per repeat), other keys -> key handler 0054:085E; returns g_mouse_click */
u16 input_poll(void) {
    u16 ax;
    int cc;
    u16 si;
    u32 t1;
    FUNC_TICK(63);
    mouse_poll();
    t1 = rtl_key_read();
    si = LO16(t1);
    if (!(LO16(t1) == 0xFFFF || g_mouse_click != 0)) {
        if (si != 0x1F) {
            if (si == 0x14) {
                g_mouse_click = 2;
            }
        } else {
            g_mouse_click = 1;
        }
    }
    if (((i16)si) >= 21) {
        if (((i16)si) <= 28) {
            if (((i16)si) < 25) {
                ax = 0x14;
            } else {
                ax = 0x18;
            }
            cc = ((u16)(si - ax)) == g_key_dir;
            si = si - ax;
            if (cc) {
                if (((i16)g_key_speed) < 16) {
                    g_key_speed = g_key_speed + 2;
                }
            } else {
                G32(0xD38) = MK32(2, si);
            }
            ax = g_key_dir - 1;
            if (((u16)(g_key_dir - 1)) <= 3) {
                switch (ax) {
                case 0:
                    g_mouse_pending_dy = g_mouse_pending_dy - g_key_speed;
                    break;
                case 1:
                    g_mouse_pending_dy = g_mouse_pending_dy + g_key_speed;
                    break;
                case 2:
                    g_mouse_pending_dx = g_mouse_pending_dx - g_key_speed;
                    break;
                case 3:
                    g_mouse_pending_dx = g_mouse_pending_dx + g_key_speed;
                    break;
                default:
                    cpu_halt("bad switch index %u in 105E:16EE", ax);
                }
            }
        } else {
L_179C:
            hotkey_handler(si);
        }
    } else {
        goto L_179C;
    }
    ax = g_mouse_click;
    return ax;
}

/* 105E:17AB  read mouse buttons and relative motion (int 33h, 373B), add motion to g_mouse_pending_dx/dy, latch a new button state into g_mouse_click */
void mouse_poll(void) {
    u16 si;
    u32 t1;
    FUNC_TICK(14);
    t1 = rtl_mouse_read();
    g_mouse_pending_dx = g_mouse_pending_dx + g_mouse_rel_x;
    g_mouse_pending_dy = g_mouse_pending_dy + g_mouse_rel_y;
    si = LO16(t1);
    if (LO16(t1) != 0) {
        if (si != g_mouse_click) {
            g_mouse_click = si;
        }
    }
    return;
}

/* 105E:17D1  per-frame ambient animations of the current room (rooms 1,2,4,5,8,9,10,12 and -3): draws animated parts into room buffers (frame counter 0xD3C) and pushes them with room_anim_blit; sets 0x7A38 */
u16 room_anim_update(u16 a0, u16 ax) {
    int cc;
    u16 di;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    u32 rv_;
    u16 t1;
    u16 t5;
    u16 t15;
    u16 t16;
    u16 t17;
    u16 t18;
    u16 t19;
    u16 t21;
    u16 t28;
    u16 t29;
    u16 t30;
    u16 t39;
    u16 t40;
    u16 t41;
    u16 t42;
    u16 t43;
    u16 t55;
    u16 t56;
    u16 t57;
    u16 t58;
    u16 t61;
    u16 t62;
    u16 t66;
    u16 t67;
    u16 t72;
    u16 t77;
    u16 t78;
    u16 t79;
    u16 t80;
    u16 t81;
    u16 t86;
    u16 t87;
    u16 t88;
    u16 t89;
    u16 t95;
    u16 t96;
    u16 t97;
    u16 t98;
    u16 t105;
    u16 t106;
    u16 t107;
    u16 t109;
    u16 t110;
    u16 t111;
    u16 t113;
    u16 t115;
    u16 t116;
    u16 t119;
    u16 t128;
    u16 t129;
    u16 t130;
    u16 t131;
    u16 t132;
    u16 t135;
    u16 t136;
    u16 t137;
    u16 t138;
    u16 t139;
    u16 t140;
    u16 t143;
    u16 t144;
    u16 t148;
    u16 t149;
    u16 t152;
    u16 t153;
    u16 t157;
    u16 t158;
    u16 t160;
    u16 t161;
    u16 t162;
    u16 t163;
    u16 t164;
    u16 t165;
    u16 t174;
    u16 t177;
    u16 t178;
    u16 t179;
    u16 t181;
    u16 t192;
    u16 t193;
    u16 t194;
    u16 t195;
    u16 t204;
    u16 t207;
    u16 t208;
    u16 t209;
    u16 t210;
    u16 t211;
    u32 t214;
    u16 t232;
    u16 t233;
    u16 t234;
    u16 t235;
    u32 t244;
    u16 t262;
    u16 t263;
    u16 t264;
    u16 t265;
    u32 t269;
    u16 t277;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u8 fr_[4] = {0};                  /* bytes bp-4 .. bp-1 of the original frame */
    FUNC_TICK(1990);
    l_6 = 0;
    if ((a0 == 1 || a0 == 3) && g_tip_visible == 0) {
        if (g_room == ROOM_ESTATE_AGENT) {
            g_room_anim_dirty = 1;
            t1 = g_room_anim_frame;
            g_room_anim_frame = g_room_anim_frame + 1;
            if (((i16)t1) > 45) {
                g_room_anim_frame = 1;
            }
            bitmap_copy(g_room_gfx2_lo, g_room_gfx2_hi, g_room_gfx3_lo, g_room_gfx3_hi);
            if (((i16)g_room_anim_frame) > 40) {
                draw_sprite_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xA), g_room_gfx3_lo, g_room_gfx3_hi, (u16)(i8)(G8((g_estate_portrait << 1) + 0xD7C)), (u16)(i8)(G8((g_estate_portrait << 1) + 0xD7D)));
            }
            room_anim_blit(9);
            ax = blit_opaque(g_room_gfx3, 0xF8, 0x28);
        } else if (g_room == ROOM_SUPERMARKET) {
            g_room_anim_dirty = 1;
            if (g_room_mode == 1 || g_room_mode == 2) {
                t5 = g_room_anim_frame;
                g_room_anim_frame = g_room_anim_frame + 1;
                if (((i16)t5) > 30) {
                    g_room_anim_frame = 1;
                }
                bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), g_room_gfx6_lo, g_room_gfx6_hi, 0xC, 0x36);
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x42), g_room_gfx6_lo, g_room_gfx6_hi, 6, 1);
                l_C = (u16)((u16)((i32)(i16)(u16)((u16)((i32)(i16)g_room_anim_frame / 10)) % 3));
                t15 = (u16)(i8)(G8((l_C << 1) + 0xD69));
                t16 = (u16)(i8)(G8((l_C << 1) + 0xD68));
                t17 = g_room_gfx6_hi;
                t18 = g_room_gfx6_lo;
                t19 = col_to_px(l_C + 0xA);
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t19), t18, t17, t16, t15);
                room_anim_blit(8);
                ax = blit_opaque(g_room_gfx6, 0, 0x14);
            } else if (g_room_mode == 0) {
                t21 = g_room_anim_frame;
                g_room_anim_frame = g_room_anim_frame + 1;
                if (((i16)t21) > 40) {
                    g_room_anim_frame = 1;
                }
                bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                l_C = (u16)((u16)((i32)(i16)(u16)((u16)((i32)(i16)g_room_anim_frame / 8)) % 5));
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), g_room_gfx6_lo, g_room_gfx6_hi, 0xC, 0x36);
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), g_room_gfx6_lo, g_room_gfx6_hi, 6, 1);
                t28 = g_room_gfx6_hi;
                t29 = g_room_gfx6_lo;
                t30 = col_to_px(l_C);
                draw_sprite_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t30), t29, t28, 0x11, 0);
                room_anim_blit(8);
                ax = blit_opaque(g_room_gfx6, 0, 0x14);
            } else if (g_room_mode == 6) {
                g_room_anim_frame = g_room_anim_frame + 1;
                bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                l_C = (u16)((u16)((i32)(i16)g_room_anim_frame / 4));
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), g_room_gfx6_lo, g_room_gfx6_hi, 0xC, 0x36);
                t39 = (u16)(i8)(G8((l_C << 1) + 0xD6F));
                t40 = (u16)(i8)(G8((l_C << 1) + 0xD6E));
                t41 = g_room_gfx6_hi;
                t42 = g_room_gfx6_lo;
                t43 = col_to_px(l_C + 3);
                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t43), t42, t41, t40, t39);
                room_anim_blit(8);
                ax = blit_opaque(g_room_gfx6, 0, 0x14);
                if (g_room_anim_frame == 0x1B) {
                    g_room_mode = 1;
                    ax = ui_supermarket_menu(0);
                }
            }
        } else if (g_room == ROOM_OFFICE) {
            ax = g_room_owner;
            if (g_room_owner == g_player && g_room_mode == 0) {
                g_room_anim_frame = g_room_anim_frame + 1;
                ax = g_room_anim_frame;
                if (((i16)g_room_anim_frame) > 319) {
                    g_room_anim_frame = 1;
                }
                if (((i16)g_room_anim_frame) >= 280) {
                    bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                    if ((u16)((u16)((i32)(i16)g_room_anim_frame % 2)) == 0) {
                        draw_sprite_to(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x3A), g_room_gfx6_lo, g_room_gfx6_hi, 0xB, 2);
                    }
                    ax = room_anim_blit(7);
                }
            } else {
L_1BF0:
                if (g_room == ROOM_STUDIO) {
                    g_room_anim_dirty = 1;
                    g_room_anim_frame = g_room_anim_frame + 1;
                    if (((i16)g_room_anim_frame) > 120) {
                        g_room_anim_frame = 1;
                    }
                    bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                    draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), g_room_gfx6_lo, g_room_gfx6_hi, 4, 0);
                    draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), g_room_gfx6_lo, g_room_gfx6_hi, 4, 0x31);
                    if (((i16)g_room_anim_frame) > 97) {
                        draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x2A), g_room_gfx6_lo, g_room_gfx6_hi, 0x18, 8);
                    }
                    if (g_room_mode == 3) {
                        if ((u16)((u16)((i32)(i16)g_room_anim_frame % 6)) != 0) {
                            l_C = g_room_anim_sub;
                        } else {
                            t55 = random_n(5, g_room_anim_frame);
                            g_room_anim_sub = t55 - 1;
                            l_C = t55 - 1;
                        }
                        if (l_C != 0) {
                            t56 = g_room_gfx6_hi;
                            t57 = g_room_gfx6_lo;
                            t58 = col_to_px(l_C + 6);
                            draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t58), t57, t56, 0x18, 0x11);
                        }
                    }
                    t61 = g_room_gfx6_hi;
                    t62 = g_room_gfx6_lo;
                    t66 = (u16)((i32)(i16)(u16)((u16)((i32)(i16)g_room_anim_frame / 12)) % 3);
                    t67 = col_to_px(t66 + 0xF);
                    draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t67), t62, t61, 0x29, 4);
                    room_anim_blit(6);
                    ax = blit_opaque(g_room_gfx6, 0, 0x1E);
                } else if (g_room == 0xFFFD) {
                    if (g_room_stack(g_room_stack_top) == 5) {
                        g_room_anim_frame = g_room_anim_frame + 1;
                        if (((i16)g_room_anim_frame) > 92) {
                            g_room_anim_frame = 0xC;
                        }
                        bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                        if (((i16)g_room_anim_frame) < 12) {
                            t72 = (u16)((i32)(i16)g_room_anim_frame / 3);
                            FR16(fr_, 4, 0x2) = DSEG;
                            FR16(fr_, 4, 0x4) = (t72 * 3) + 0xD5C;
                            l_C = (u16)(i8)(FP8(FR32(fr_, 4, 0x4), 0));
                            l_8 = (u16)(i8)(FP8(FR32(fr_, 4, 0x4), 1));
                            t77 = (u16)(i8)(FP8(FR32(fr_, 4, 0x4), 2));
                            t78 = l_8;
                            t79 = g_room_gfx6_hi;
                            t80 = g_room_gfx6_lo;
                            t81 = col_to_px(l_C);
                            draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t81), t80, t79, t78, t77);
                            if (((i16)l_C) <= 3) {
                                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), g_room_gfx6_lo, g_room_gfx6_hi, 4, 0);
                            }
                        } else {
                            draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x22), g_room_gfx6_lo, g_room_gfx6_hi, 7, 1);
                            if ((u16)((u16)((i32)(i16)g_room_anim_frame % 4)) != 0) {
                                l_C = g_room_anim_sub;
                            } else {
                                t86 = random_n(5, g_room_anim_frame);
                                g_room_anim_sub = t86 - 1;
                                l_C = t86 - 1;
                            }
                            if (l_C != 0) {
                                t87 = g_room_gfx6_hi;
                                t88 = g_room_gfx6_lo;
                                t89 = col_to_px(l_C + 0xA);
                                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t89), t88, t87, 0x1A, 0x10);
                            }
                        }
                        room_anim_blit(6);
                        ax = blit_opaque(g_room_gfx6, 0, 0x1E);
                    } else {
L_1EE4:
                        if (g_room == ROOM_FILM_AGENCY && (g_room_mode == 0 || g_room_mode == 1 || g_room_mode == 5)) {
                            g_room_anim_frame = g_room_anim_frame + 1;
                            blit_opaque_to(g_room_gfx5, g_room_gfx6_lo, g_room_gfx6_hi, 0, 0);
                            blit_opaque_rect(g_screen_buf, g_room_gfx6, 0x38, 0x30, 0x48, 0x42, 0x10, 5, 0 /* unused ax */);
                            if ((u16)((u16)((i32)(i16)g_room_anim_frame % 3)) != 0) {
                                l_C = g_room_anim_sub;
                            } else {
                                t95 = random_n(3, g_room_anim_frame);
                                g_room_anim_sub = t95 - 1;
                                l_C = t95 - 1;
                            }
                            if (l_C != 0) {
                                t96 = g_room_gfx6_hi;
                                t97 = g_room_gfx6_lo;
                                t98 = col_to_px(l_C);
                                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t98), t97, t96, 4, 0x16);
                            }
                            l_C = (u16)((u16)((i32)(i16)(u16)((u16)((i32)(i16)g_room_anim_frame % 56)) / 2)) + 1;
                            if (((i16)l_C) >= 26) {
                                l_8 = 0xD;
                                if (l_C == 0x1A || l_C == 0x1C) {
                                    l_A = 3;
                                } else {
                                    l_A = 4;
                                }
                                t105 = g_room_gfx6_hi;
                                t106 = g_room_gfx6_lo;
                                t107 = col_to_px(l_A);
                                draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t107), t106, t105, 0xE, 0xD);
                            }
                            filmoffers_draw(g_room_gfx6, 0x10, 0x12);   /* --film-offers: tapes 11.. reach into the animation (film_offers.c) */
                            blit_opaque(g_room_gfx6, 0x10, 0x12);
                            ax = room_anim_blit(5);
                            if (((i16)g_room_anim_frame) > 500) {
                                g_room_anim_frame = 1;
                                t109 = timer_read();
                                g_frame_timer = t109;
                                ax = t109;
                                while (((i16)g_room_anim_frame) <= 8) {
                                    LOOP_TICK(101);
                                    t110 = timer_read();
                                    t111 = (u16)((i32)50 / (i16)g_frame_divisor);
                                    ax = (t111 * 3) + g_frame_timer;
                                    if (t110 <= ((u16)((t111 * 3) + g_frame_timer))) continue;
                                    t113 = timer_read();
                                    g_frame_timer = t113;
                                    grab_screen(g_room_gfx6, 0x10, 0x12);
                                    blit_opaque_rect(g_screen_buf, g_room_gfx6, 0x38, 0x30, 0x48, 0x42, 0x10, 5, 0 /* unused ax */);
                                    ax = g_room_anim_frame - 1;
                                    if (((u16)(g_room_anim_frame - 1)) > 5) {
                                        l_8 = 4;
                                        g_room_anim_dirty = 1;
                                        di = 0;
                                    } else {
                                        switch (ax) {
                                        case 0:
                                            l_8 = 0x10;
                                            di = 0x18;
                                            break;
                                        case 1:
                                            l_8 = 0x10;
                                            di = 0x15;
                                            break;
                                        case 2:
                                            l_8 = 0x10;
                                            di = 0x13;
                                            break;
                                        case 3:
                                            l_8 = 0x12;
                                            di = 0x10;
                                            break;
                                        case 4:
                                            l_8 = 0x10;
                                            di = 0xE;
                                            break;
                                        case 5:
                                            l_8 = 4;
                                            di = 0;
                                            break;
                                        default:
                                            cpu_halt("bad switch index %u in 105E:17D1", ax);
                                        }
                                    }
                                    s0 = di;
                                    s1 = l_8;
                                    s2 = g_room_gfx6_hi;
                                    s3 = g_room_gfx6_lo;
                                    if (((i16)g_room_anim_frame) > 6) {
                                        ax = 0xB;
                                    } else {
                                        ax = g_room_anim_frame + 4;
                                    }
                                    t115 = col_to_px(ax);
                                    draw_sprite_to(MK32(g_actor_txt_hi, g_actor_txt_lo + t115), s3, s2, s1, s0);
                                    filmoffers_draw(g_room_gfx6, 0x10, 0x12);   /* --film-offers: as above */
                                    t116 = room_anim_blit(5);
                                    g_room_anim_frame = g_room_anim_frame + 1;
                                    ax = t116;
                                }
                                g_room_anim_frame = 1;
                            }
                        } else if (((u32)MK32(g_room, g_room_mode)) != 0x20000) {
                            if (g_room == 0xFFFD) {
                                if (g_room_stack(g_room_stack_top) == 2) {
L_21B0:
                                    g_room_anim_dirty = 1;
                                    t119 = g_room_anim_frame;
                                    g_room_anim_frame = g_room_anim_frame + 1;
                                    if (((i16)t119) >= 1000) {
                                        g_room_anim_frame = 1;
                                    }
                                    bitmap_copy(g_room_gfx3_lo, g_room_gfx3_hi, g_actor_txt_lo, g_actor_txt_hi);
                                    l_A = (u16)((u16)((i32)(i16)g_room_anim_frame % 35)) + 1;
                                    if (!(((i16)l_A) < 22 || ((i16)l_A) > 25)) {
                                        draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 2), g_actor_txt_lo, g_actor_txt_hi, 6, 0);
                                    }
                                    if (g_room == 0xFFFD) {
                                        if ((u16)((u16)((i32)(i16)g_room_anim_frame % 4)) == 0 || g_news_anim_pick == 0xFFFF) {
                                            for (;;) {
                                                LOOP_TICK(11);
                                                t128 = l_6;
                                                l_6 = l_6 + 1;
                                                t129 = random_n(7, t128);
                                                l_C = t129 - 1;
                                                if (((u16)(t129 - 1)) == g_news_anim_pick) {
                                                    continue;
                                                } else {
                                                    break;
                                                }
                                            }
                                            g_news_anim_pick = l_C;
                                        } else {
                                            l_C = g_news_anim_pick;
                                        }
                                        if (l_C != 0) {
                                            t130 = g_actor_txt_hi;
                                            t131 = g_actor_txt_lo;
                                            t132 = col_to_px(l_C + 1);
                                            draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t132), t131, t130, 6, 6);
                                        }
                                    }
                                    blit_opaque(g_actor_txt, 0xE8, 0x31);
                                    ax = room_anim_blit(4);
                                } else {
L_22BD:
                                    if (g_room == ROOM_LIBRARY && (g_room_mode == 0 || g_room_mode == 3 || g_room_mode == 4)) {
                                        G16(0xD40) = 1;
                                        if (g_room_anim_frame != 0 || (random_n(0x1F4, 1)) != 1) {
                                            if (((i16)g_room_anim_frame) < 1 || ((i16)g_room_anim_frame) > 5) {
                                                if (((i16)g_room_anim_frame) < 6 || ((i16)g_room_anim_frame) > 9) {
                                                    if (((i16)g_room_anim_frame) >= 10 && ((i16)g_room_anim_frame) <= 25) {
                                                        t143 = g_room_gfx5_hi;
                                                        t144 = g_room_gfx5_lo;
                                                        t148 = (u16)((i32)(i16)(g_room_anim_frame + 0xFFF7) % 8);
                                                        t149 = col_to_px(t148 + 1);
                                                        draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t149), t144, t143, 0xA, 0x11);
                                                        t152 = g_room_gfx5_hi;
                                                        t153 = g_room_gfx5_lo;
                                                        t157 = (u16)((i32)(i16)(g_room_anim_frame + 0xFFF7) % 8);
                                                        t158 = col_to_px(t157 + 0xE);
                                                        draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t158), t153, t152, 0x16, 0x25);
                                                        g_room_anim_frame = g_room_anim_frame + 1;
                                                        if (((i16)g_room_anim_frame) > 25) {
                                                            g_room_anim_frame = 0xA;
                                                            g_room_anim_sub = g_room_anim_sub + 1;
                                                            if (((i16)g_room_anim_sub) > 10) {
                                                                G32(0xD3C) = 0x12;
                                                            }
                                                        }
                                                    } else if (((i16)g_room_anim_frame) < 26 || ((i16)g_room_anim_frame) > 29) {
                                                        if (((i16)g_room_anim_frame) < 30 || ((i16)g_room_anim_frame) > 34) {
                                                            if (((i16)g_room_anim_frame) >= 35) {
                                                                g_room_anim_frame = 0;
                                                                g_room_anim_dirty = 1;
                                                            }
                                                        } else {
                                                            t163 = g_room_gfx5_hi;
                                                            t164 = g_room_gfx5_lo;
                                                            t165 = col_to_px(0x2B - g_room_anim_frame);
                                                            draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t165), t164, t163, 2, 0);
                                                            anim_div_tick(5, DSP(0xD3C));
                                                        }
                                                    } else {
                                                        if (g_room_anim_frame == 0x1A) {
                                                            draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x62), g_room_gfx5_lo, g_room_gfx5_hi, 2, 0);
                                                        }
                                                        t160 = g_room_gfx5_hi;
                                                        t161 = g_room_gfx5_lo;
                                                        t162 = col_to_px(0x33 - g_room_anim_frame);
                                                        draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t162), t161, t160, 0xA, 0x11);
                                                        anim_div_tick(3, DSP(0xD3C));
                                                    }
                                                } else {
                                                    t138 = g_room_gfx5_hi;
                                                    t139 = g_room_gfx5_lo;
                                                    t140 = col_to_px(g_room_anim_frame + 0x10);
                                                    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t140), t139, t138, 0xA, 0x11);
                                                    anim_div_tick(3, DSP(0xD3C));
                                                }
                                            } else {
                                                t135 = g_room_gfx5_hi;
                                                t136 = g_room_gfx5_lo;
                                                t137 = col_to_px(g_room_anim_frame + 8);
                                                draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t137), t136, t135, 2, 0);
                                                anim_div_tick(5, DSP(0xD3C));
                                            }
                                        } else {
                                            cursor_hide();
                                            cursor_show(0 /* unused ax */);
                                            g_room_anim_frame = 1;
                                        }
                                        blit_opaque(g_room_gfx5, 0xE8, 0xC);
                                        ax = room_anim_blit(1);
                                    } else if (g_room == ROOM_SCRIPT_AGENCY) {
                                        if (!(((i16)g_room_anim_frame) < 10000 || ((i16)g_room_mode) > 2)) {
                                            g_room_anim_frame = 1;
                                        }
                                        if ((g_room_mode == 3 || g_room_mode == 4) && (((i16)g_room_anim_frame) < 800 || ((i16)g_room_anim_frame) >= 10000)) {
                                            if (((i16)g_room_anim_frame) < 10000) {
                                                g_room_anim_frame = 0x2710;
                                            }
                                            g_room_anim_frame = g_room_anim_frame + 1;
                                            t174 = (u16)((i32)(i16)(u16)((u16)((i32)(i16)(g_room_anim_frame + 0xD8F0) / 3)) % 20);
                                            bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                                            draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xB2), g_room_gfx6_lo, g_room_gfx6_hi, 6, 0);
                                            draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x12), g_room_gfx6_lo, g_room_gfx6_hi, 0x18, 0x1C);
                                            draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x1A), g_room_gfx6_lo, g_room_gfx6_hi, 0x18, 0x12);
                                            l_A = 0;
                                            ax = t174 - 1;
                                            if (((u16)(t174 - 1)) <= 4) {
                                                switch (ax) {
                                                case 0:
                                                case 4:
                                                    l_A = 0x18;
                                                    break;
                                                case 1:
                                                case 2:
                                                case 3:
                                                    l_A = 0x19;
                                                    break;
                                                default:
                                                    cpu_halt("bad switch index %u in 105E:17D1", ax);
                                                }
                                            }
                                            if (l_A != 0) {
                                                t177 = g_room_gfx6_hi;
                                                t178 = g_room_gfx6_lo;
                                                t179 = col_to_px(l_A);
                                                draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t179), t178, t177, 0x11, 0xA);
                                            }
                                            room_anim_blit(3);
                                            ax = bitmap_put_screen(g_room_gfx6, 0xD0, 0x20);
                                            if (g_room_anim_frame == 0x2716) {
                                                porter_say(0);
                                                t181 = present_frame();
                                                g_room_mode = 4;
                                                ax = t181;
                                            }
                                        } else {
                                            g_room_anim_frame = g_room_anim_frame + 1;
                                            if (((i16)g_room_anim_frame) > 860) {
                                                g_room_anim_frame = 1;
                                            }
                                            if (((i16)g_room_anim_frame) < 800 || ((i16)g_room_anim_frame) >= 832) {
                                                if ((u16)((u16)((i32)(i16)g_room_anim_frame % 2)) == 0) {
                                                    bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                                                    l_C = (u16)((u16)((i32)(i16)(u16)((u16)((i32)(i16)g_room_anim_frame / 2)) % 3));
                                                    l_A = 0;
                                                    ax = l_C;
                                                    if (l_C == 0) {
                                                        l_A = 4;
                                                        l_8 = 0x18;
                                                        di = 0x12;
                                                    } else if (ax == 1) {
                                                        l_A = 5;
                                                        l_8 = 0x19;
                                                        di = 0x11;
                                                    } else if (ax == 2) {
                                                        l_A = 6;
                                                        l_8 = 0x19;
                                                        di = 0x13;
                                                    }
                                                    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x12), g_room_gfx6_lo, g_room_gfx6_hi, 0x18, 0x1C);
                                                    t192 = l_8;
                                                    t193 = g_room_gfx6_hi;
                                                    t194 = g_room_gfx6_lo;
                                                    t195 = col_to_px(l_A);
                                                    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t195), t194, t193, t192, di);
                                                    room_anim_blit(3);
                                                    bitmap_put_screen(g_room_gfx6, 0xD0, 0x20);
                                                }
                                            } else if ((u16)((u16)((i32)(i16)g_room_anim_frame % 4)) == 0) {
                                                bitmap_copy(g_room_gfx5_lo, g_room_gfx5_hi, g_room_gfx6_lo, g_room_gfx6_hi);
                                                t204 = (u16)((i32)(i16)(g_room_anim_frame + 0xFCE0) / 4);
                                                draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xA), g_room_gfx6_lo, g_room_gfx6_hi, 0x19, 0xC);
                                                si = t204;
                                                if (((i16)t204) >= 0) {
                                                    if (((i16)si) <= 2) {
                                                        draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x12), g_room_gfx6_lo, g_room_gfx6_hi, 0x18, 0x1C);
                                                    }
                                                }
                                                t207 = (u16)(i8)(G8((si * 3) + 0xD86));
                                                t208 = (u16)(i8)(G8((si * 3) + 0xD85));
                                                t209 = g_room_gfx6_hi;
                                                t210 = g_room_gfx6_lo;
                                                t211 = col_to_px((u16)(i8)(G8((si * 3) + 0xD84)));
                                                draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t211), t210, t209, t208, t207);
                                                room_anim_blit(3);
                                                bitmap_put_screen(g_room_gfx6, 0xD0, 0x20);
                                            }
                                            si = (g_room_anim_frame + 0xFCD4) << 2;
                                            if (((i16)((g_room_anim_frame + 0xFCD4) << 2)) >= 0) {
                                                if (((i16)si) <= 68) {
                                                    cursor_hide();
                                                    if ((g_sa_fly1_bg_lo | g_sa_fly1_bg_hi) != 0) {
                                                        bitmap_put_screen(g_sa_fly1_bg, g_sa_fly1_x & 0xFFF8, g_sa_fly1_y);
                                                        rtl_vga_put_bitmap_w(W2(g_sa_fly1_bg), ((i16)(((i16)(((i16)g_sa_fly1_x) >> 1)) >> 1)) >> 1, g_sa_fly1_y);
                                                    } else {
                                                        t214 = bitmap_alloc(0x18, 0xC);
                                                        g_sa_fly1_bg = t214;
                                                    }
                                                    l_8 = 0xEA - (si << 1);
                                                    if (((i16)si) > 9) {
                                                        if (((i16)si) > 27) {
                                                            if (((i16)si) > 43) {
                                                                if (((i16)si) > 59) {
                                                                    di = ((si + 0xFFC5) << 2) + 0x3F;
                                                                } else {
                                                                    di = ((si + 0xFFD5) << 1) + 0x1F;
                                                                }
                                                            } else {
                                                                di = si + 0xFFF4;
                                                            }
                                                        } else {
                                                            di = 0xF;
                                                        }
                                                    } else {
                                                        di = 0x23 - (si << 1);
                                                    }
                                                    l_A = (u16)((u16)((i32)(i16)(0x5D - (u16)((u16)((i32)(i16)si / 4))) % 8)) + 0xA;
                                                    G32(0xD48) = MK32(di, l_8 & 0xFFF8);
                                                    blit_opaque_rect(g_screen_buf, g_sa_fly1_bg, 0, 0, g_sa_fly1_x & 0xFFF8, g_sa_fly1_y, 0x18, 0xC, 0 /* unused ax */);
                                                    bitmap_copy(g_sa_fly1_bg_lo, g_sa_fly1_bg_hi, g_room_gfx7_lo, g_room_gfx7_hi);
                                                    t232 = (u16)((i32)(i16)l_8 % 8);
                                                    t233 = g_room_gfx7_hi;
                                                    t234 = g_room_gfx7_lo;
                                                    t235 = col_to_px(l_A);
                                                    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t235), t234, t233, t232, 0);
                                                    bitmap_put_screen(g_room_gfx7, l_8 & 0xFFF8, di);
                                                    rtl_vga_put_bitmap_w(W2(g_room_gfx7), ((i16)(((i16)(((i16)l_8) >> 1)) >> 1)) >> 1, di);
                                                    cursor_show(R.ax);
                                                } else {
L_2ACA:
                                                    if (si == 0x48) {
                                                        cursor_hide();
                                                        bitmap_put_screen(g_sa_fly1_bg, g_sa_fly1_x & 0xFFF8, g_sa_fly1_y);
                                                        rtl_vga_put_bitmap_w(W2(g_sa_fly1_bg), ((i16)(((i16)(((i16)g_sa_fly1_x) >> 1)) >> 1)) >> 1, g_sa_fly1_y);
                                                        mem_free_ptr(HPTR(DSP(0xD44)));
                                                        cursor_show(0 /* unused ax */);
                                                    }
                                                }
                                            } else {
                                                goto L_2ACA;
                                            }
                                            ax = (g_room_anim_frame + 0xFCC8) << 2;
                                            si = (g_room_anim_frame + 0xFCC8) << 2;
                                            if (((i16)((g_room_anim_frame + 0xFCC8) << 2)) >= 0) {
                                                if (((i16)si) <= 64) {
                                                    cursor_hide();
                                                    if ((g_sa_fly2_bg_lo | g_sa_fly2_bg_hi) != 0) {
                                                        bitmap_put_screen(g_sa_fly2_bg, g_sa_fly2_x & 0xFFF8, g_sa_fly2_y);
                                                        rtl_vga_put_bitmap_w(W2(g_sa_fly2_bg), ((i16)(((i16)(((i16)g_sa_fly2_x) >> 1)) >> 1)) >> 1, g_sa_fly2_y);
                                                    } else {
                                                        t244 = bitmap_alloc(0x20, 0x14);
                                                        g_sa_fly2_bg = t244;
                                                    }
                                                    l_8 = 0xEA - (si << 1);
                                                    if (((i16)si) > 9) {
                                                        if (((i16)si) > 27) {
                                                            if (((i16)si) > 43) {
                                                                if (((i16)si) > 59) {
                                                                    di = ((si + 0xFFC5) << 2) + 0x3A;
                                                                } else {
                                                                    di = ((si + 0xFFD5) << 1) + 0x1A;
                                                                }
                                                            } else {
                                                                di = si + 0xFFEF;
                                                            }
                                                        } else {
                                                            di = 0xA;
                                                        }
                                                    } else {
                                                        di = 0x1E - (si << 1);
                                                    }
                                                    l_A = (u16)((u16)((i32)(i16)(0x5E - (u16)((u16)((i32)(i16)si / 4))) % 5)) + 0x12;
                                                    G32(0xD50) = MK32(di, l_8 & 0xFFF8);
                                                    blit_opaque_rect(g_screen_buf, g_sa_fly2_bg, 0, 0, g_sa_fly2_x & 0xFFF8, g_sa_fly2_y, 0x20, 0x14, 0 /* unused ax */);
                                                    bitmap_copy(g_sa_fly2_bg_lo, g_sa_fly2_bg_hi, g_room_gfx8_lo, g_room_gfx8_hi);
                                                    t262 = (u16)((i32)(i16)l_8 % 8);
                                                    t263 = g_room_gfx8_hi;
                                                    t264 = g_room_gfx8_lo;
                                                    t265 = col_to_px(l_A);
                                                    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t265), t264, t263, t262, 0);
                                                    bitmap_put_screen(g_room_gfx8, l_8 & 0xFFF8, di);
                                                    t269 = rtl_vga_put_bitmap_w(W2(g_room_gfx8), ((i16)(((i16)(((i16)l_8) >> 1)) >> 1)) >> 1, di);
                                                    ax = cursor_show(LO16(t269));
                                                } else {
L_2CBF:
                                                    if (si == 0x44) {
                                                        cursor_hide();
                                                        bitmap_put_screen(g_sa_fly2_bg, g_sa_fly2_x & 0xFFF8, g_sa_fly2_y);
                                                        rtl_vga_put_bitmap_w(W2(g_sa_fly2_bg), ((i16)(((i16)(((i16)g_sa_fly2_x) >> 1)) >> 1)) >> 1, g_sa_fly2_y);
                                                        mem_free_ptr(HPTR(DSP(0xD4C)));
                                                        si = 0;
                                                        while (((i16)si) <= 4) {
                                                            LOOP_TICK(8);
                                                            if (((i16)(G16(((si + 0x2A) << 1) + 0x79C6))) >= 0) {
                                                                si = si + 1;
                                                                continue;
                                                            } else {
                                                                goto L_2D22;
                                                            }
                                                        }
L_2D45:
                                                        scriptagency_draw_scripts();
                                                        cursor_show(0 /* unused ax */);
                                                        t277 = present_frame();
                                                        g_room_anim_dirty = 1;
                                                        ax = t277;
                                                    }
                                                }
                                            } else {
                                                goto L_2CBF;
                                            }
                                        }
                                    } else {
                                        g_room_anim_dirty = 1;
                                    }
                                }
                            } else {
                                goto L_22BD;
                            }
                        } else {
                            goto L_21B0;
                        }
                    }
                } else {
                    goto L_1EE4;
                }
            }
        } else {
            goto L_1BF0;
        }
    } else if (a0 == 2) {
        G32(0xD3C) = 0;
        g_news_anim_pick = 0xFFFF;
        ax = (G16((si << 3) + 0xD44)) | (G16((si << 3) + 0xD46));
        if (((G16((si << 3) + 0xD44)) | (G16((si << 3) + 0xD46))) != 0) {
            si = 0;
            while (((i16)si) <= 2) {
                LOOP_TICK(22);
                cc = ((G16((si << 3) + 0xD44)) | (G16((si << 3) + 0xD46))) == 0;
                ax = (G16((si << 3) + 0xD44)) | (G16((si << 3) + 0xD46));
                if (!(cc)) {
                    ax = mem_free_ptr(HPTR(MK32(DSEG, (si << 3) + 0xD44)));
                }
                si = si + 1;
            }
        }
    }
    rv_ = ax;
    return (u16)rv_;
L_2D22:
    G16(((si + 0x2A) << 1) + 0x79C6) = (G16(((si + 0x2A) << 1) + 0x79C6)) * 0xFFFF;
    goto L_2D45;
}

/* 105E:2DD2  every n-th call increment *counter (divider 0xD9C) */
void anim_div_tick(u16 a0, u32 a1) {
    FUNC_TICK(11);
    g_anim_div = g_anim_div + 1;
    if (g_anim_div == a0) {
        g_anim_div = 0;
        FP16(a1, 0) = FP16(a1, 0) + 1;
    }
    return;
}

/* 105E:2DEF  copy animated room area n (1,3..9: fixed rects from room buffers 0x10C..0x118) to VGA, hiding the cursor while it overlaps */
u16 room_anim_blit(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u32 rv_;
    u16 t1;
    u16 t3;
    u16 t4;
    u16 t6;
    u16 t7;
    u16 t9;
    u16 t10;
    u16 t12;
    u16 t13;
    u16 t15;
    u16 t16;
    u16 t18;
    u16 t19;
    u16 t21;
    u16 t22;
    u16 t24;
    u16 l_2 = 0;
    u8 fr_[6] = {0};                  /* bytes bp-6 .. bp-1 of the original frame */
    FUNC_TICK(204);
    si = 0;
    if (a0 != 1) {
        if (a0 != 3) {
            if (a0 != 4) {
                if (a0 != 5) {
                    if (a0 != 6) {
                        if (a0 != 7) {
                            if (a0 != 8) {
                                if (a0 == 9) {
                                    t22 = FPOFF(g_room_gfx3);
                                    FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx3);
                                    FR16(fr_, 6, 0x6) = t22;
                                    l_2 = 0x28;
                                    t24 = cursor_over_rect(0xF8, 0x28, 0x118, 0x32);
                                    di = 0x1F;
                                    if (t24 != 0) {
                                        cursor_hide();
                                        si = 1;
                                    }
                                }
                            } else {
                                t19 = FPOFF(g_room_gfx6);
                                FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx6);
                                FR16(fr_, 6, 0x6) = t19;
                                l_2 = 0x14;
                                t21 = cursor_over_rect(0, 0x14, 0x30, 0x72);
                                di = 0;
                                if (t21 != 0) {
                                    cursor_hide();
                                    si = 1;
                                }
                            }
                        } else {
                            t16 = FPOFF(g_room_gfx6);
                            FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx6);
                            FR16(fr_, 6, 0x6) = t16;
                            l_2 = 0x45;
                            t18 = cursor_over_rect(0x78, 0x45, 0xA0, 0x55);
                            di = 0xF;
                            if (t18 != 0) {
                                cursor_hide();
                                si = 1;
                            }
                        }
                    } else {
                        t13 = FPOFF(g_room_gfx6);
                        FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx6);
                        FR16(fr_, 6, 0x6) = t13;
                        l_2 = 0x1E;
                        t15 = cursor_over_rect(0, 0x1E, 0x50, 0x71);
                        di = 0;
                        if (t15 != 0) {
                            cursor_hide();
                            si = 1;
                        }
                    }
                } else {
                    t10 = FPOFF(g_room_gfx6);
                    FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx6);
                    FR16(fr_, 6, 0x6) = t10;
                    l_2 = 0x12;
                    t12 = cursor_over_rect(0x10, 0x12, 0x58, 0x47);
                    di = 2;
                    if (t12 != 0) {
                        cursor_hide();
                        si = 1;
                    }
                }
            } else {
                t7 = FPOFF(g_actor_txt);
                FR16(fr_, 6, 0x4) = FPSEG(g_actor_txt);
                FR16(fr_, 6, 0x6) = t7;
                l_2 = 0x31;
                t9 = cursor_over_rect(0xE8, 0x31, 0x100, 0x3B);
                di = 0x1D;
                if (t9 != 0) {
                    cursor_hide();
                    si = 1;
                }
            }
        } else {
            t4 = FPOFF(g_room_gfx6);
            FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx6);
            FR16(fr_, 6, 0x6) = t4;
            l_2 = 0x20;
            t6 = cursor_over_rect(0xD0, 0x20, 0x100, 0x48);
            di = 0x1A;
            if (t6 != 0) {
                cursor_hide();
                si = 1;
            }
        }
    } else {
        t1 = FPOFF(g_room_gfx5);
        FR16(fr_, 6, 0x4) = FPSEG(g_room_gfx5);
        FR16(fr_, 6, 0x6) = t1;
        l_2 = 0xC;
        t3 = cursor_over_rect(0xE8, 0xC, 0x120, 0x39);
        di = 0x1D;
        if (t3 != 0) {
            cursor_hide();
            si = 1;
        }
    }
    ax = LO16(rtl_vga_put_bitmap_w(W2(FR32(fr_, 6, 0x6)), di, l_2));
    if (si == 1) {
        ax = cursor_show(ax);
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 105E:3002  build window bitmap for cell rect in g_popup_bmp[0] (or preallocated g_window_buf): grab VGA background of the border rows/columns, draw ui_frame over it */
void window_create(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u16 di;
    u16 s0;
    u16 si;
    u32 l_4;
    u32 t16;
    FUNC_TICK(231);
    si = a1;
    di = a0;
    if ((g_window_buf_lo | g_window_buf_hi) == 0) {
        t16 = bitmap_alloc(((a2 - di) + 1) << 3, ((a3 - si) + 1) << 3);
        g_popup_bmp(0) = t16;
    } else {
        /* (not the original's) a window larger than the buffer would overwrite the next heap block */
        if ((u32)64 * ((a2 - di) + 1) * ((a3 - si) + 1) + 16 > (FP32(MKFP(g_window_buf_hi, g_window_buf_lo - 8), 0) & ~1u))
            cpu_halt("window_create: a window of %u x %u cells does not fit the window buffer", (a2 - di) + 1, (a3 - si) + 1);
        FP16(g_window_buf, 0) = (a2 - di) + 1;
        FP16(g_window_buf, 2) = ((a3 - si) + 1) << 3;
        MEM32((u16)(FPSEG(g_window_buf)), (u16)(FPOFF(g_window_buf)) + 4) = MK32(g_window_buf_hi, g_window_buf_lo + 8);
        g_popup_bmp(0) = g_window_buf;
    }
    s0 = 1;
    if (((i16)((a2 - di) + 1)) <= ((i16)((a3 - si) - 1))) {
        ax = ((a3 - si) - 1) << 6;
    } else {
        ax = ((a2 - di) + 1) << 6;
    }
    l_4 = mem_calloc(ax + 8, s0);
    FP16(l_4, 0) = (a2 - di) + 1;
    FP16(l_4, 2) = 8;
    FP32(l_4, 4) = MK32(HI16(l_4), LO16(l_4) + 8);
    rtl_vga_get_bitmap_w(W2(l_4), di, si << 3);
    bitmap_copy(LO16(l_4), HI16(l_4), g_popup_bmp_lo(0), g_popup_bmp_hi(0));
    if (((i16)a3) > ((i16)si)) {
        rtl_vga_get_bitmap_w(W2(l_4), di, a3 << 3);
        bitmap_put(l_4, g_popup_bmp(0), 0, (a3 - si) << 3);
    }
    if (((i16)(a3 - si)) > 1) {
        FP16(l_4, 0) = 1;
        FP16(l_4, 2) = ((a3 - si) - 1) << 3;
        FP32(l_4, 4) = MK32(HI16(l_4), LO16(l_4) + 8);
        rtl_vga_get_bitmap_w(W2(l_4), di, (si + 1) << 3);
        bitmap_put(l_4, g_popup_bmp(0), 0, 8);
        rtl_vga_get_bitmap_w(W2(l_4), a2, (si + 1) << 3);
        bitmap_put(l_4, g_popup_bmp(0), (a2 - di) << 3, 8);
    }
    mem_free(LO16(l_4), HI16(l_4));
    ui_frame(0, 0, a2 - di, a3 - si, g_popup_bmp_lo(0), g_popup_bmp_hi(0), 1);
    return;
}

/* 105E:3247  put the window bitmap on VGA at col, row*8; free it unless preallocated */
void window_show(u16 a0, u16 a1) {
    FUNC_TICK(21);
    rtl_vga_put_bitmap_w(W2(g_popup_bmp(0)), a0, a1 << 3);
    if ((g_window_buf_lo | g_window_buf_hi) == 0) {
        mem_free(g_popup_bmp_lo(0), g_popup_bmp_hi(0));
    }
    return;
}

/* 105E:3280  render the "TV family" view (tvfam_render) and put it on VGA at 208,127 unless g_tvfam_frozen; hides the cursor while it is over the view */
void tvfam_update(void) {
    u16 si;
    u32 l_4;
    u16 t1;
    u32 t3;
    FUNC_TICK(45);
    clip_reset();
    t1 = cursor_y();
    si = 0;
    if (!(((i16)t1) < 110 || ((i16)(cursor_x())) < 190 || g_cursor_shown != 1)) {
        cursor_hide();
        si = 1;
    }
    if (g_tvfam_frozen == 0) {
        t3 = tvfam_render();
        l_4 = t3;
        rtl_vga_put_bitmap_w(W2(t3), 0x1A, 0x7F);
        mem_free(LO16(l_4), HI16(l_4));
    }
    if (si == 1) {
        cursor_show(0 /* unused ax */);
    }
    return;
}

/* 105E:32ED  build the 104x58 TV-family picture: copy g_tvfam_bg, animate up to 5 viewers (blinks, eating/drinking, mood reactions), then TV switch-on/off shutter bars by g_tvfam_state/g_tvfam_shutter; returns new bitmap */
u32 tvfam_render(void) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u32 l_E;
    u16 t3;
    u16 t9;
    u16 t11;
    u16 t21;
    u16 t23;
    u16 t31;
    u16 t38;
    u16 t45;
    u16 t51;
    u16 t63;
    u16 t64;
    u16 t66;
    u16 t71;
    u16 t73;
    u16 t75;
    u16 t77;
    u16 t82;
    u16 t85;
    FUNC_TICK(789);
    l_E = bitmap_alloc(0x68, 0x3A);
    rtl_bitmap_fill_w(W2(l_E), 0, 0, 1, 0x3A, 0);
    g_tile_target = l_E;
    bitmap_copy(g_tvfam_bg_lo, g_tvfam_bg_hi, LO16(l_E), HI16(l_E));
    if (g_tvfam_state != 0) {
        if (G16(0x8336) != 0) {
            l_4 = DSP(0x832E);
            g_tile_atlas = FP32(l_4, 0);
            t3 = FP16(l_4, 4);
            FP16(l_4, 8) = FP16(l_4, 8) + 1;
            FP16(l_4, 0xC) = FP16(l_4, 0xC) + 1;
            di = t3;
            if (((i16)FP16(l_4, 0xC)) > 43) {
                draw_atlas_tile(0x22, di + 7, 9);
                if (FP16(l_4, 0xC) == 0x2D) {
                    FP16(l_4, 0xC) = 1;
                }
            }
            if (((i16)FP16(l_4, 8)) > 8) {
                FP16(l_4, 8) = 1;
            }
            t9 = (u16)((i32)(i16)(FP16(l_4, 8) + 1) / 2);
            si = t9;
            if (t9 == 1) {
L_33E8:
                si = 2;
            } else if (si != 3) {
                if (si == 2) {
                    si = 3;
                }
            } else {
                goto L_33E8;
            }
            t11 = col_to_px(si);
            draw_atlas_tile(t11, di + 0xA, 0x10);
            tvfam_idle_anim(l_4, 4, 8, 7, 0xD);
            tvfam_idle_anim(l_4, 6, 0xA, 7, 0xD);
        }
        l_4 = DSP(0x833E);
        g_tile_atlas = FP32(l_4, 0);
        di = FP16(l_4, 4);
        if (G16(0x8346) == 1) {
            FP16(l_4, 0xA) = FP16(l_4, 0xA) + 1;
            if (((i16)(u16)((u16)((i32)(i16)FP16(l_4, 0xA) % 50))) > 47) {
                draw_atlas_tile(0x52, di + 8, 0xB);
            }
            tvfam_idle_anim(l_4, 2, 0xA, 8, 0x11);
            tvfam_idle_anim(l_4, 4, 0xC, 8, 0x11);
        }
        if (G16(0x8346) == 2) {
            FP16(l_4, 0xA) = FP16(l_4, 0xA) + 1;
            if (((i16)FP16(l_4, 0xA)) > 185) {
                FP16(l_4, 0xA) = 0x7E;
            }
            si = FP16(l_4, 0xA);
            if (((i16)FP16(l_4, 0xA)) <= 35) {
                l_8 = 9;
            }
            if (((i16)si) > 85) {
                if (((i16)si) > 125) {
                    if ((u16)((u16)((i32)(i16)(si + 0xFFA0) / 30)) != 1) {
                        l_8 = 3;
                    } else {
                        l_8 = 0xE;
                    }
                } else {
                    l_8 = 3;
                }
            } else {
                l_8 = 2;
            }
            t21 = col_to_px(l_8);
            draw_atlas_tile(t21, di + 8, 7);
            if (((i16)si) > 75) {
                if (((i16)si) <= 80) {
                    draw_atlas_tile(0x1A, di + 7, 0x18);
                }
            }
            if (((i16)si) > 80) {
                if (((i16)si) <= 85) {
                    draw_atlas_tile(0x22, di + 7, 0x18);
                }
            }
            if (((i16)si) > 85) {
                draw_atlas_tile(0x2A, di + 7, 0x18);
            }
            if (((i16)si) > 90) {
                draw_atlas_tile(0x32, di + 0xC, 0x2C);
            }
            if (((i16)si) > 155) {
                draw_atlas_tile(0x3A, di + 7, 0x18);
            }
        }
        l_4 = DSP(0x834E);
        g_tile_atlas = FP32(l_4, 0);
        di = FP16(l_4, 4);
        if (g_tvfam_v2_state == 1) {
            t23 = g_tvfam_v2_counter;
            g_tvfam_v2_counter = g_tvfam_v2_counter + 1;
            if (((i16)t23) > 100) {
                g_tvfam_v2_counter = 1;
            }
        }
        if (g_tvfam_v2_state == 2) {
            FP16(l_4, 0xA) = FP16(l_4, 0xA) + 1;
            if (((i16)FP16(l_4, 0xA)) > 135) {
                FP16(l_4, 0xA) = 1;
            }
            draw_atlas_tile(0x72, di + 6, 5);
            l_6 = (u16)((u16)((i32)(i16)(FP16(l_4, 0xA) + 0xE) / 15));
            l_8 = 0;
            ax = (l_6 - 1) - 1;
            if (((u16)((l_6 - 1) - 1)) <= 7) {
                switch (ax) {
                case 0:
                case 2:
                case 5:
                    l_8 = 2;
                    break;
                case 1:
                case 3:
                case 7:
                    l_8 = 3;
                    break;
                case 4:
                case 6:
                    break;
                default:
                    cpu_halt("bad switch index %u in 105E:32ED", ax);
                }
            }
            if (l_8 != 0) {
                t31 = col_to_px(l_8);
                draw_atlas_tile(t31, di + 8, 0xE);
            }
        } else if (g_tvfam_v2_state == 3) {
            FP16(l_4, 0xA) = FP16(l_4, 0xA) + 1;
            if (((i16)FP16(l_4, 0xA)) > 285) {
                FP16(l_4, 0xA) = 0xE2;
            }
            if (((i16)FP16(l_4, 0xA)) > 225) {
                l_6 = (u16)((u16)((i32)(i16)(FP16(l_4, 0xA) + 0xFF3C) / 30));
                t45 = col_to_px(l_6 + 6);
                draw_atlas_tile(t45, di + 6, 4);
            } else {
                l_6 = (u16)((u16)((i32)(i16)(FP16(l_4, 0xA) + 0x4A) / 75));
                t38 = col_to_px(l_6 + 3);
                draw_atlas_tile(t38, di + 6, 4);
            }
        }
        if (g_tvfam_v2_state == 1) {
            l_4 = DSP(0x834E);
            if (((i16)(u16)((u16)((i32)(i16)FP16(l_4, 0xA) % 45))) >= 43) {
                draw_atlas_tile(0x4A, di + 6, 9);
            }
            tvfam_idle_anim(l_4, 0xB, 0xB, 6, 8);
            tvfam_idle_anim(l_4, 0xC, 0xC, 6, 8);
        }
        if (G16(0x8366) != 0) {
            l_4 = DSP(0x835E);
            g_tile_atlas = FP32(l_4, 0);
            t51 = FP16(l_4, 4);
            FP16(l_4, 8) = FP16(l_4, 8) + 1;
            di = t51;
            if (FP16(l_4, 6) == 7 || FP16(l_4, 6) == 6) {
                l_8 = 1;
            } else if (FP16(l_4, 6) != 5) {
                l_8 = 3;
            } else {
                l_8 = 2;
            }
            if (((i16)FP16(l_4, 8)) > ((i16)((l_8 * 9) + 0x4B))) {
                FP16(l_4, 8) = 1;
            }
            if (((i16)(u16)((u16)((i32)(i16)FP16(l_4, 8) % 40))) >= 37) {
                draw_atlas_tile(0x4A, di + 7, 9);
            }
            tvfam_idle_anim(l_4, 6, 6, 7, 0);
            tvfam_idle_anim(l_4, 7, 7, 7, 0);
            tvfam_idle_anim(l_4, 9, 9, 7, 0);
            if (((i16)FP16(l_4, 8)) > 78) {
                t63 = (u16)((i32)(i16)((u16)((u16)((i32)(i16)(FP16(l_4, 8) + 0xFFB7) / 3)) - 1) % 3);
                t64 = col_to_px(t63 + 2);
                draw_atlas_tile(t64, di + 2, 0xC);
            }
        }
        if (G16(0x8376) != 0) {
            l_4 = DSP(0x836E);
            g_tile_atlas = FP32(l_4, 0);
            t66 = FP16(l_4, 4);
            FP16(l_4, 0xA) = FP16(l_4, 0xA) + 1;
            di = t66;
            if (((i16)FP16(l_4, 0xA)) > 40) {
                FP16(l_4, 0xA) = 1;
            }
            if (((i16)FP16(l_4, 0xA)) >= 37) {
                draw_atlas_tile(0x52, di + 7, 0x10);
            }
            t71 = (u16)((i32)(i16)((u16)((u16)((i32)(i16)FP16(l_4, 0xA) % 20)) + 5) / 5);
            si = t71;
            if (t71 == 4) {
                si = 2;
            }
            t73 = col_to_px(si + 7);
            draw_atlas_tile(t73, di + 2, 0x26);
            t75 = col_to_px((si << 1) + 0xC);
            draw_atlas_tile(t75, di, 0xB);
            t77 = col_to_px((si << 1) + 0xD);
            draw_atlas_tile(t77, di + 0x10, 0xB);
            if (FP16(l_4, 8) == 2) {
                FP16(l_4, 0xC) = FP16(l_4, 0xC) + 1;
                if (((i16)FP16(l_4, 0xC)) > 51) {
                    FP16(l_4, 0xC) = 1;
                }
                t82 = (u16)((i32)(i16)(FP16(l_4, 0xC) + 2) / 3);
                t85 = col_to_px((u16)(i8)G8(t82 + 0xD9D));
                draw_atlas_tile(t85, di + 1, 0x18);
            }
        }
    }
    g_tile_atlas = g_tvfam_atlas;
    draw_atlas_tile(0xA, 0x54, 4);
    if (g_tvfam_state != 1) {
        if (g_tvfam_state == 2) {
            g_tvfam_shutter = g_tvfam_shutter - 1;
            if (g_tvfam_shutter == 0) {
                g_tvfam_state = 3;
            }
        }
    } else {
        g_tvfam_shutter = g_tvfam_shutter + 1;
        if (g_tvfam_shutter == 6) {
            g_tvfam_state = 0;
        }
    }
    l_A = 0;
    while (((i16)l_A) < ((i16)g_tvfam_shutter)) {
        LOOP_TICK(28);
        draw_atlas_tile(0x3A, 0, l_A * 5);
        draw_atlas_tile(0x3A, 0, 0x35 - (l_A * 5));
        l_A = l_A + 1;
    }
    draw_atlas_tile(0x42, 0, 0);
    draw_atlas_tile(0x4A, 0x5E, 0);
    draw_atlas_tile(0x52, 0, 0x33);
    ax = LO16(l_E);
    dx = HI16(l_E);
    return MK32(dx, ax);
}

/* 105E:3B14  draw the living-room/sofa background tiles into g_tvfam_bg */
void tvfam_draw_room(void) {
    u16 si;
    FUNC_TICK(31);
    g_tile_target = g_tvfam_bg;
    g_tile_atlas = g_tvfam_atlas;
    si = 0;
    while (((i16)si) <= 96) {
        LOOP_TICK(27);
        draw_atlas_tile(0x2A, si, 0);
        draw_atlas_tile(0x2A, si, 8);
        draw_atlas_tile(0x32, si, 0x32);
        si = si + 8;
    }
    si = 0x1F;
    while (((i16)si) <= 67) {
        LOOP_TICK(11);
        draw_atlas_tile(0x22, si, 0xE);
        si = si + 8;
    }
    draw_atlas_tile(0x12, 0, 0xE);
    draw_atlas_tile(0x1A, 0x43, 0xE);
    return;
}

/* 105E:3BAC  draw TV-family viewer 0 (rec 0x832E): base pose, mood face, stage +0xA (1..7) overlays */
void tvfam_draw_viewer0(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 t1;
    u16 t6;
    FUNC_TICK(131);
    l_4 = DSP(0x832E);
    t1 = FP16(l_4, 4);
    l_4 = DSP(0x832E);
    g_tile_atlas = FP32(l_4, 0);
    g_tile_target = g_tvfam_bg;
    draw_atlas_tile(2, t1, 4);
    si = t1;
    if (FP16(l_4, 6) != 0) {
        di = FP16(l_4, 6);
        if (FP16(l_4, 6) == 4) {
            di = 2;
        }
        if (di == 6) {
            di = 3;
        }
        t6 = col_to_px(di + 4);
        draw_atlas_tile(t6, si + 7, 0xD);
    }
    if (((i16)FP16(l_4, 0xA)) >= 2) {
        draw_atlas_tile(0x52, si, 0x1C);
    }
    if (((i16)FP16(l_4, 0xA)) >= 3) {
        draw_atlas_tile(0x5A, si, 0x25);
    }
    if (!(((i16)FP16(l_4, 0xA)) < 4 || FP16(l_4, 0xA) == 7)) {
        draw_atlas_tile(0x62, si, 0x2D);
    }
    if (FP16(l_4, 0xA) == 5) {
        draw_atlas_tile(0x6A, si + 0xC, 0x2F);
    }
    if (FP16(l_4, 0xA) == 6) {
        draw_atlas_tile(0x72, si + 1, 0x2E);
    }
    if (FP16(l_4, 0xA) == 7) {
        draw_atlas_tile(0x7A, si, 0x2D);
    }
    return;
}

/* 105E:3CE8  draw TV-family viewer 1 (rec 0x833E) */
void tvfam_draw_viewer1(void) {
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t4;
    FUNC_TICK(60);
    l_4 = DSP(0x833E);
    t2 = FP16(l_4, 4);
    g_tile_atlas = FP32(l_4, 0);
    g_tile_target = g_tvfam_bg;
    draw_atlas_tile(2, t2, 0x12);
    si = t2;
    if (FP16(l_4, 8) == 1) {
        draw_atlas_tile(0x42, si + 8, 7);
        if (FP16(l_4, 6) == 5) {
            t4 = col_to_px(FP16(l_4, 6) + 8);
            draw_atlas_tile(t4, si + 8, 0x11);
        }
    }
    return;
}

/* 105E:3D79  draw TV-family viewer 2 (rec 0x834E) */
void tvfam_draw_viewer2(void) {
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t4;
    FUNC_TICK(78);
    l_4 = DSP(0x834E);
    t2 = FP16(l_4, 4);
    g_tile_atlas = FP32(l_4, 0);
    g_tile_target = g_tvfam_bg;
    draw_atlas_tile(2, t2, 0xB);
    si = t2;
    if (FP16(l_4, 8) != 1) {
        if (FP16(l_4, 8) == 2) {
            draw_atlas_tile(0x72, si + 6, 5);
        }
    } else {
        draw_atlas_tile(0x42, si + 6, 5);
        if (!(FP16(l_4, 6) == 9 || FP16(l_4, 6) == 0xB || FP16(l_4, 6) == 0xC)) {
            t4 = col_to_px(FP16(l_4, 6));
            draw_atlas_tile(t4, si + 6, 8);
        }
    }
    return;
}

/* 105E:3E3B  draw TV-family viewer 3 (rec 0x835E) */
void tvfam_draw_viewer3(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t3;
    u16 t5;
    FUNC_TICK(53);
    l_4 = DSP(0x835E);
    t2 = FP16(l_4, 6);
    t3 = FP16(l_4, 4);
    g_tile_atlas = FP32(l_4, 0);
    g_tile_target = g_tvfam_bg;
    draw_atlas_tile(2, t3, 0);
    si = t2;
    di = t3;
    if (t2 == 6) {
L_3E8C:
        si = 5;
    } else {
        if (si == 7) goto L_3E8C;
    }
    if (si == 9) {
        si = 8;
    }
    t5 = col_to_px(si);
    draw_atlas_tile(t5, di + 7, 0);
    return;
}

/* 105E:3EB5  draw TV-family viewer 4 (rec 0x836E; ? the child: dropped after clock_hour 6) */
void tvfam_draw_viewer4(void) {
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t4;
    FUNC_TICK(70);
    l_4 = DSP(0x836E);
    t2 = FP16(l_4, 4);
    g_tile_atlas = FP32(l_4, 0);
    g_tile_target = g_tvfam_bg;
    draw_atlas_tile(2, t2 + 7, 0xB);
    si = t2;
    if (FP16(l_4, 6) != 0) {
        t4 = col_to_px(FP16(l_4, 6));
        draw_atlas_tile(t4, si + 7, 0x13);
    }
    draw_atlas_tile(0xA, si + 1, 0x23);
    if (FP16(l_4, 8) == 1) {
        draw_atlas_tile(0x12, si + 1, 0x18);
    }
    return;
}

/* 105E:3F58  tvfam_update when the view is not off (g_tvfam_state != 0) */
void tvfam_tick(void) {
    FUNC_TICK(5);
    if (g_tvfam_state != 0) {
        tvfam_update();
    }
    return;
}

/* 105E:3F64  screenplay agency (room 9): draw up to 5 script sprites (res 0xB9 */
void scriptagency_draw_scripts(void) {
    u16 si;
    u16 t3;
    u16 t4;
    FUNC_TICK(8);
    clip_reset();
    g_clip_y2 = 0x71;
    si = 0;
    while (((i16)si) <= 4) {
        LOOP_TICK(29);
        if (((i16)(G16(((si + 0x2A) << 1) + 0x79C6))) > 0) {
            t3 = G16(((si + 0x2A) << 1) + 0x79C6);
            t4 = col_to_px(si + 0x12);
            draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t4), t3, 0x64);
        }
        si = si + 1;
    }
    return;
}

/* 105E:3FB7  office (room 1, own office): draw the pile g_office_papers (max 36): first item at 122,69, up to 6 single sheets from table 0xE2B, beyond that stacks at table 0xDAF */
void office_draw_paper_pile(void) {
    u16 si;
    u16 t1;
    u16 t2;
    u16 t3;
    FUNC_TICK(31);
    if (((i16)g_office_papers) > 36) {
        g_office_papers = 0x24;
    }
    if (((i16)g_office_papers) >= 1) {
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA), 0x7A, 0x45);
    }
    if (((i16)g_office_papers) >= 2 && ((i16)g_office_papers) <= 6) {
        si = 0;
        while (((i16)(g_office_papers + 0xFFFE)) >= ((i16)si)) {
            LOOP_TICK(45);
            t1 = (G16((si * 6) + 0xE2D)) + G16(0xDB1);
            t2 = (G16((si * 6) + 0xE2B)) + G16(0xDAF);
            t3 = col_to_px(G16((si * 6) + 0xE2F));
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t3), t2, t1);
            si = si + 1;
        }
    } else {
        si = g_office_papers + 0xFFFA;
        while (((i16)si) >= 0) {
            LOOP_TICK(25);
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x32), G16((si << 2) + 0xDAF), G16((si << 2) + 0xDB1));
            si = si - 1;
        }
    }
    return;
}

/* 105E:40B0  office: draw a letter on the desk if g_player_letter[owner] != 0; text 0x29 (Betty's love letter) also draws the red seal */
void office_draw_letter(void) {
    FUNC_TICK(35);
    if (g_betty_letter(g_room_owner) != 0) {
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x42), 0xB0, 0x4C);
        if (g_betty_letter(g_room_owner) == 0x29) {
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x4A), 0xC4, 0x58);
        }
    }
    return;
}

/* 105E:4107  film agency (room 8): draw Mr. Jabble at his terminal in the pose res g_dealer_pose (0xBC..0xBF, changes every 2 game hours) */
void dealer_draw_pose(void) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(35);
    ax = g_dealer_pose - 0xBC;
    if (((u16)(g_dealer_pose - 0xBC)) <= 3) {
        switch (ax) {
        case 0:
            si = 0xA1;
            di = 0x1B;
            break;
        case 1:
            si = 0xA0;
            di = 0x22;
            break;
        case 2:
            si = 0xA0;
            di = 0x26;
            break;
        case 3:
            si = 0xA0;
            di = 0x1C;
            break;
        default:
            cpu_halt("bad switch index %u in 105E:4107", ax);
        }
    }
    draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), si, di);
    return;
}

/* 105E:415C  estate agent (room 10): portrait by g_estate_wealth (0-2,3-4,5-7,8-10 -> 4 ever richer outfits, res 0xC2..) plus 4 banknote stacks (wealth*5/10 bundles) and 8 coin stacks (wealth*6/10 - i/2) with random jitter */
void estate_draw_wealth(void) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 t13;
    u16 t14;
    u16 t15;
    u16 t16;
    u16 t19;
    u16 t29;
    u16 t30;
    u16 t35;
    FUNC_TICK(60);
    l_6 = 0;
    clip_reset();
    g_clip_y2 = 0x71;
    ax = g_estate_wealth;
    if (g_estate_wealth <= 0xA) {
        switch (ax) {
        case 0:
        case 1:
        case 2:
            g_estate_portrait = 0;
            break;
        case 3:
        case 4:
            g_estate_portrait = 1;
            break;
        case 5:
        case 6:
        case 7:
            g_estate_portrait = 2;
            break;
        case 8:
        case 9:
        case 0xA:
            g_estate_portrait = 3;
            break;
        default:
            cpu_halt("bad switch index %u in 105E:415C", ax);
        }
    }
    draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), G8((g_estate_portrait << 1) + 0xE61), G8((g_estate_portrait << 1) + 0xE62));
    di = 3;
    while (((i16)di) >= 0) {
        LOOP_TICK(78);
        l_C = (u16)((u16)((i32)(i16)(g_estate_wealth * 5) / 10)) - (3 - di);
        if (((i16)l_C) > 0) {
            l_4 = MK32(DSEG, (di << 1) + 0xE49);
            l_8 = 0;
            si = 1;
            while (((i16)si) <= ((i16)l_C)) {
                LOOP_TICK(53);
                l_A = (u16)((u16)((i32)(i16)(si + 1) % 2)) + 1;
                if (l_A != 1) {
                    ax = 5;
                } else {
                    ax = 7;
                }
                l_8 = l_8 + ax;
                t13 = FP8(l_4, 1) - l_8;
                t14 = l_6;
                l_6 = l_6 + 1;
                t15 = random_n(3, t14);
                t16 = (t15 + FP8(l_4, 0)) + 0xFFFE;
                t19 = col_to_px((u16)((u16)((i32)(i16)(si + 1) % 2)) + 1);
                draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t19), t16, t13);
                si = si + 1;
            }
        }
        di = di - 1;
    }
    di = 0;
    while (((i16)di) <= 7) {
        LOOP_TICK(62);
        l_C = (u16)((u16)((i32)(i16)(g_estate_wealth * 6) / 10)) - (u16)((u16)((i32)(i16)di / 2));
        if (((i16)l_C) > 0) {
            l_4 = MK32(DSEG, (di << 1) + 0xE51);
            si = 1;
            while (((i16)si) <= ((i16)l_C)) {
                LOOP_TICK(34);
                t29 = (FP8(l_4, 1) - (si << 2)) + 4;
                t30 = FP8(l_4, 0);
                t35 = col_to_px((u16)((u16)((i32)(i16)di % 2)) + 3);
                draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t35), t30, t29);
                si = si + 1;
            }
        }
        di = di + 1;
    }
    return;
}

/* 105E:434D  -1 if the cursor box (24x10, or the drag image size) overlaps rect x0,y0,x1,y1 else 0 */
u16 cursor_over_rect(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u16 t5;
    u16 t7;
    FUNC_TICK(59);
    if ((g_drag_image_lo | g_drag_image_hi) != 0 || ((i16)((cursor_x()) + 0x18)) < ((i16)a0) || ((i16)(cursor_x())) > ((i16)a2) || ((i16)((cursor_y()) + 0xA)) < ((i16)a1) || ((i16)(cursor_y())) > ((i16)a3)) {
        if ((g_drag_image_lo | g_drag_image_hi) == 0) {
L_43D2:
            ax = 0;
        } else {
            t5 = cursor_x();
            if (((i16)((t5 + g_drag_w) + 8)) < ((i16)a0) || ((i16)(cursor_x())) > ((i16)a2)) {
                goto L_43D2;
            } else {
                t7 = cursor_y();
                if (((i16)(t7 + g_drag_h)) < ((i16)a1) || ((i16)(cursor_y())) > ((i16)a3)) {
                    goto L_43D2;
                } else {
L_43CB:
                    ax = 0xFFFF;
                }
            }
        }
    } else {
        goto L_43CB;
    }
    return ax;
}

/* 105E:43D8  for a viewer whose mood == m: 1/50 chance per frame to start a 10-frame overlay (blink/mouth) drawn at viewer.x+dx, y */
void tvfam_idle_anim(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(43);
    if (FP16(a0, 6) == a2) {
        if (FP16(a0, 0xE) == 0) {
            if ((random_n(0x32, 1)) == 1) {
                FP16(a0, 0xE) = 0xA;
            }
        } else {
            FP16(a0, 0xE) = FP16(a0, 0xE) - 1;
            sprite_draw_target(MK32(FP16(a0, 2), (FP16(a0, 0) + (a3 << 3)) + 0xFFFA), FP16(a0, 4) + a4, a5);
        }
    }
    return;
}

/* 105E:4447  1 if (cursor_x, cursor_y) lies inside the inclusive rect x0,y0,x1,y1 */
u16 mouse_in_rect(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u16 si;
    u16 t1;
    FUNC_TICK(26);
    t1 = cursor_x();
    si = 0;
    if (!(((i16)t1) < ((i16)a0) || ((i16)(cursor_x())) > ((i16)a2) || ((i16)(cursor_y())) < ((i16)a1) || ((i16)(cursor_y())) > ((i16)a3))) {
        si = 1;
    }
    ax = si;
    return ax;
}

/* 105E:447B  game won: free room data, show the "Just Married" screen (res 0x7D/0xEF/0x4B + car animation res 0xF0, 105 frames of 3 ticks from table 0xE69), wait for key/click, wipe out and exit */
void ending_play(void) {
    u16 di;
    u16 si;
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t6;
    u16 t15;
    u16 t16;
    u16 t17;
    u16 t18;
    u8 fr_[26] = {0};                  /* bytes bp-26 .. bp-1 of the original frame */
    FUNC_TICK(240);
    room_free_assets();
    rtl_snd_room_music_stop(0x42, 0);
    mem_free(g_scripts_lo, g_scripts_hi);
    mem_free(g_series_lo, g_series_hi);
    mem_free(g_movies_lo, g_movies_hi);
    snd_room_music_start(0x42, 0, 0 /* unused ax */);
    mem_free(g_screen_buf_lo, g_screen_buf_hi);
    t1 = bitmap_alloc(0x140, 0xC8);
    g_screen_buf = t1;
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0xC8, 0);
    t2 = res_load0(0x7D);
    g_room_gfx = t2;
    bitmap_put_screen(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0, 1);
    mem_free_ptr(HPTR(DSP(0x100)));
    t3 = res_load0(0x4B);
    g_room_gfx = t3;
    bitmap_put_screen(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0xD0, 1);
    mem_free_ptr(HPTR(DSP(0x100)));
    t4 = res_load0(0xEF);
    g_room_gfx = t4;
    bitmap_put_screen(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0x70, 1);
    mem_free_ptr(HPTR(DSP(0x100)));
    t5 = res_load(0xF0);
    g_room_gfx = t5;
    t6 = bitmap_alloc(0x80, 0x8E);
    g_room_gfx1 = t6;
    g_clip_on = 0;
    blit_opaque_rect(g_screen_buf, g_room_gfx1, 0, 0, 0x60, 0x23, 0x80, 0x8E, 0 /* unused ax */);
    draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0x60, 0x23);
    screen_wipe(g_screen_buf_lo, g_screen_buf_hi, 0, 0, 0x140, 0xC8, 2, 1);
    di = 1;
    while (((i16)di) <= 105) {
        LOOP_TICK(97);
        timer_start(0);
        FR16(fr_, 26, 0x18) = 0;
        si = 0;
        while (((i16)si) < 6) {
            LOOP_TICK(41);
            if (g_ending_anim(si * 4) == di) {
                FR16(fr_, 26, 0x18) = 1;
                FR16(fr_, 26, 0x12) = g_ending_anim((si * 4) + 1);
                FR16(fr_, 26, 0x16) = g_ending_anim((si * 4) + 2);
                FR16(fr_, 26, 0x14) = g_ending_anim((si * 4) + 3);
            }
            si = si + 1;
        }
        if (FR16(fr_, 26, 0x18) != 0) {
            bitmap_put_screen(g_room_gfx1, 0x60, 0x23);
            if (FR16(fr_, 26, 0x12) != 0) {
                t15 = FR16(fr_, 26, 0x14);
                t16 = FR16(fr_, 26, 0x16);
                t17 = col_to_px(FR16(fr_, 26, 0x12));
                draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t17), t16, t15);
            }
            t18 = FPOFF(g_screen_buf);
            FR16(fr_, 26, 0xE) = FPSEG(g_screen_buf);
            FR16(fr_, 26, 0x10) = t18;
            FR16(fr_, 26, 0x4) = 0xC;
            FR16(fr_, 26, 0x2) = 0x23;
            FR16(fr_, 26, 0xC) = 0xC;
            FR16(fr_, 26, 0xA) = 0x23;
            FR16(fr_, 26, 0x6) = 0x8E;
            FR16(fr_, 26, 0x8) = 0x10;
            rtl_vga_put_rect((fr_ + 26 - 0x10));
        }
        for (;;) {
            LOOP_TICK(3);
            if ((timer_read()) < 3) {
                seat_pass();             /* seats: a wait point (runtime/seat.h) */
                continue;
            } else {
                break;
            }
        }
        di = di + 1;
    }
    for (;;) {
        LOOP_TICK(6);
        if (LO16((rtl_key_read())) != 0xFFFF || LO16((rtl_mouse_read())) != 0) {
            break;
        } else {
            seat_pass();             /* seats: a wait point -- another seat's input comes only between its turns */
            continue;
        }
    }
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0xC8, 0);
    screen_wipe(g_screen_buf_lo, g_screen_buf_hi, 0, 0, 0x140, 0xC8, 1, 1);
    rtl_exit(0);
    return;
}

/* 105E:47DF  copy a 120x12 strip of panel sprite (res 9, from row 64) to VGA at 0,186 (clears the panel's lower-left line) */
void panel_restore_strip(void) {
    u32 t1;
    u16 t2;
    u8 fr_[22] = {0};                  /* bytes bp-22 .. bp-1 of the original frame */
    FUNC_TICK(33);
    t1 = res_load0(9);
    FR16(fr_, 22, 0x2) = HI16(t1);
    FR16(fr_, 22, 0x4) = LO16(t1);
    t2 = (FPOFF(FR32(fr_, 22, 0x4))) + 2;
    FR16(fr_, 22, 0x12) = FPSEG(FR32(fr_, 22, 0x4));
    FR16(fr_, 22, 0x14) = t2;
    FR16(fr_, 22, 0x10) = 0;
    FR16(fr_, 22, 0xE) = 0x40;
    FR16(fr_, 22, 0xC) = 0xF;
    FR16(fr_, 22, 0xA) = 0xC;
    FR16(fr_, 22, 0x8) = 0;
    FR16(fr_, 22, 0x6) = 0xBA;
    rtl_vga_put_rect((fr_ + 22 - 0x14));
    mem_free(FR16(fr_, 22, 0x4), FR16(fr_, 22, 0x2));
    return;
}
