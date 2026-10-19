/* runtime/settings.c on its own (make settings-test; verify.sh): a settings file as an
 * editor may save it -- with a UTF-8 byte order mark (Windows' Notepad), CRLF line ends --
 * reads like the plain one, and a change written back keeps it.  Each case in a child
 * process of its own (settings.c keeps its values in statics).  POSIX only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "runtime.h"

static char dir[] = "/tmp/settings_test_XXXXXX";
static int failed;

static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    static char b[65536];
    size_t n = fread(b, 1, sizeof b - 1, f);
    fclose(f);
    b[n] = 0;
    return strdup(b);
}
static void put(const char *p, const char *s) {
    FILE *f = fopen(p, "wb");
    fputs(s, f);
    fclose(f);
}
/* s with every "from" replaced by "to" (once each line is enough here) */
static char *subst(const char *s, const char *from, const char *to) {
    char *o = malloc(strlen(s) * 2 + 64), *w = o;
    for (const char *p = s; *p;)
        if (!strncmp(p, from, strlen(from))) { strcpy(w, to); w += strlen(to); p += strlen(from); }
        else *w++ = *p++;
    *w = 0;
    return o;
}
static int count(const char *s, const char *what) {
    int n = 0;
    for (const char *p = s; (p = strstr(p, what)); p += strlen(what)) n++;
    return n;
}
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); exit(1); } } while (0)
static void run(const char *name, void (*f)(void)) {
    fflush(stdout);
    pid_t p = fork();
    if (!p) { f(); exit(0); }
    int st = 0;
    waitpid(p, &st, 0);
    int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    printf("%-34s %s\n", name, ok ? "ok" : "FAILS");
    failed += !ok;
}

static char defaults_ini[1100], full_ini[1100], part_ini[1100];
static const char *defaults_text;

static void make_defaults(void) {
    CHECK(settings_load(defaults_ini), "no defaults file");
}
/* the whole file with a BOM and CRLF: the player's values count, nothing is added,
 * a change goes into its line */
static void bom_full(void) {
    char *want = slurp(full_ini);
    CHECK(settings_load(full_ini), "not read");
    const char *fs = settings_get("display", "fullscreen"), *sc = settings_get("display", "scale");
    CHECK(fs && !strcmp(fs, "yes"), "display.fullscreen = %s, not yes", fs ? fs : "(none)");
    CHECK(sc && !strcmp(sc, "5"), "display.scale = %s, not 5", sc ? sc : "(none)");
    char *now = slurp(full_ini);
    CHECK(!strcmp(now, want), "the file was changed on reading (%d [display] sections)", count(now, "[display]"));
    settings_set("display", "scale", "4");
    now = slurp(full_ini);
    char *after = subst(want, "scale = 5\r\n", "scale = 4\n");
    CHECK(!strcmp(now, after), "settings_set did not change the line in place (%d [display] sections, BOM %s)",
          count(now, "[display]"), !strncmp(now, "\xEF\xBB\xBF", 3) ? "kept" : "lost");
}
/* a short one (the review's bom0.ini): the values given count, the missing ones are
 * appended -- not those given */
static void bom_part(void) {
    CHECK(settings_load(part_ini), "not read");
    const char *fs = settings_get("display", "fullscreen"), *sc = settings_get("display", "scale");
    CHECK(fs && !strcmp(fs, "yes"), "display.fullscreen = %s, not yes", fs ? fs : "(none)");
    CHECK(sc && !strcmp(sc, "5"), "display.scale = %s, not 5", sc ? sc : "(none)");
    char *now = slurp(part_ini);
    CHECK(count(now, "fullscreen =") == 1 && count(now, "scale =") == 1, "the given values were appended again");
}

int main(void) {
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(defaults_ini, sizeof defaults_ini, "%s/defaults.ini", dir);
    snprintf(full_ini, sizeof full_ini, "%s/full.ini", dir);
    snprintf(part_ini, sizeof part_ini, "%s/part.ini", dir);
    run("the defaults file", make_defaults);
    defaults_text = slurp(defaults_ini);
    if (!defaults_text) { printf("no defaults file\n"); return 1; }
    const char *sec = strstr(defaults_text, "[display]");   /* the section header the first line */
    if (!sec) { printf("no [display] in the defaults\n"); return 1; }
    char *full = subst(subst(subst(sec, "fullscreen = no", "fullscreen = yes"), "scale = 3", "scale = 5"), "\n", "\r\n");
    char *bom = malloc(strlen(full) + 4);
    strcpy(bom, "\xEF\xBB\xBF");
    strcat(bom, full);
    put(full_ini, bom);
    put(part_ini, "\xEF\xBB\xBF[display]\r\nfullscreen = yes\r\nscale = 5\r\n[game]\r\nfixes = all\r\n");
    run("BOM, CRLF, every setting", bom_full);
    run("BOM, CRLF, a few settings", bom_part);
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd)) {}
    printf(failed ? "settings: %d FAIL\n" : "settings: all ok\n", failed);
    return failed != 0;
}
