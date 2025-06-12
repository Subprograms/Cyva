/* ============================================================
   LogAnalysis.h
   ============================================================ */
#ifndef LOG_ANALYSIS_H
#define LOG_ANALYSIS_H

/* -- 0.  MSVC quirks ---------------------------------------- */
#if defined(_MSC_VER)
#   pragma warning(disable: 4996)
#   define _CRT_SECURE_NO_WARNINGS
    typedef __int64 ssize_t;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "Helper.h"        /* cyva* primitives & safe-string helpers */

/* -- 1.  sugar wrappers ------------------------------------- */
#define LEN      cyvaStrlen
#define CMP      cyvaStrcmp
#define NCMP     cyvaStrncmp
#define CSPRINT  cyvaStrcspn
#define TOK      cyvaStrtok

static char *ci_strstr(const char *h, const char *n)      /* case-insens strstr */
{
    if (!h || !n || !*n) return (char *)h;
    size_t nl = LEN(n);
    for (; *h; ++h)
        if (tolower((unsigned char)*h) == tolower((unsigned char)*n) &&
            !NCMP(h, n, nl)) return (char *)h;
    return NULL;
}
static char *lastChr(const char *s, int ch)               /* portable strrchr  */
{ char *r = NULL; while (s && *s) { if (*s == ch) r = (char *)s; ++s; } return r; }

static void  *xmalloc(size_t n) { void *p = malloc(n); if (!p){perror("OOM");exit(1);} return p; }
static char  *xstrdup(const char *s){ char *p = (char*)xmalloc(LEN(s)+1); cyvaStrcpy_cap(p, LEN(s)+1, s); return p; }

/* -- 2.  forward prototypes so the compiler knows the type -- */
static char *afterTag(const char *msg, const char *tag);
static char *afterTagFull(const char *msg, const char *tag);

/* -----------------------------------------------------------
   afterTag  –  single-token field (“Status: 0xC000006E”)
   ----------------------------------------------------------- */
static char *afterTag(const char *msg, const char *tag)
{
    char *p = ci_strstr(msg, tag);
    if (!p) return NULL;

    p += LEN(tag);
    while (*p == ' ' || *p == ':' || *p == '\t') ++p;

    char buf[128]; int i = 0;
    while (*p && !isspace((unsigned char)*p) && i < 127)
        buf[i++] = *p++;
    buf[i] = '\0';

    return i ? xstrdup(buf) : NULL;
}

/* -----------------------------------------------------------
   afterTagFull  –  whole-phrase field (“Failure Reason: …”)
   ----------------------------------------------------------- */
static char *afterTagFull(const char *msg, const char *tag)
{
    char *p = ci_strstr(msg, tag);
    if (!p) return NULL;

    p += LEN(tag);
    while (*p == ' ' || *p == ':' || *p == '\t') ++p;

    char buf[256]; int i = 0;
    while (*p && i < 255) {
        if ((*p == ' ' && p[1] == ' ') || *p == ',' ||
            *p == '\r' || *p == '\n')
            break;
        buf[i++] = *p++;
    }
    while (i && (buf[i-1] == ' ' || buf[i-1] == '.')) --i;
    buf[i] = '\0';

    return i ? xstrdup(buf) : NULL;
}

/* -- 3.  Nice time helper ---------------------------------- */
static char *niceTime(const char *iso)
{
    if (!iso) return xstrdup("");

    char d[11] = {0}, t[9] = {0};
    if (sscanf(iso, "%10[^T]T%8[^U]", d, t) != 2)
        return xstrdup(iso);                     /* not ISO – keep raw */

    struct tm tm = {0};

#if defined(_MSC_VER)                            /* no strptime there */
    sscanf(d, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    sscanf(t, "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;  tm.tm_mon -= 1;
#else
    strptime(d, "%Y-%m-%d", &tm);
    strptime(t, "%H:%M:%S", &tm);
#endif
    tm.tm_isdst = -1;                            /* let libc decide */

    /* --- convert *UTC* tm → epoch seconds ----------------- */
    time_t epoch;
#if defined(_MSC_VER)
    epoch = _mkgmtime(&tm);                      /* MSVC: gmtime-like */
#else
#   if defined(__USE_BSD) || defined(__GNU_LIBRARY__)
    epoch = timegm(&tm);                         /* GNU / BSD ext     */
#   else
    epoch = timegm(&tm);                         /* or your own stub  */
#   endif
#endif

    /* --- epoch (UTC) → local struct tm -------------------- */
    struct tm loc;
#if defined(_MSC_VER)
    localtime_s(&loc, &epoch);
#else
    localtime_r(&epoch, &loc);
#endif

    char buf[32];
    strftime(buf, sizeof buf, "%a %b %d %H:%M:%S", &loc);
    return xstrdup(buf);
}

/* -- 4.  raw-event table ----------------------------------- */
typedef struct { char *msg, *ts; int id; } EVENT;
static EVENT *ev = NULL; static int evCnt = 0, evCap = 0;

static void pushEv(const char *m, const char *t, int id)
{
    if (evCnt == evCap) { evCap = evCap ? evCap * 2 : 1024;
                          ev    = (EVENT*)realloc(ev, evCap * sizeof *ev); }
    ev[evCnt].msg = xstrdup(m);
    ev[evCnt].ts  = xstrdup(t);
    ev[evCnt].id  = id;
    ++evCnt;
}

/* -- 5.  Per-account stats --------------------------------- */
typedef struct {
    char  *name;
    int    succ, fail, locks;
    char  *workstation;
    char **lockTS;  int ltCnt,  ltCap;
    char **failRS;  int frCnt,  frCap;
} ACCT;

static ACCT *acct = NULL; static int aCnt = 0, aCap = 0;

static ACCT *getAcct(const char *n, bool mk)
{
    for (int i = 0; i < aCnt; ++i)
        if (!CMP(acct[i].name, n)) return &acct[i];

    if (!mk) return NULL;
    if (aCnt == aCap) { aCap = aCap ? aCap * 2 : 64;
                        acct = (ACCT*)realloc(acct, aCap * sizeof *acct); }

    ACCT *a = &acct[aCnt++];
    a->name = xstrdup(n);
    a->succ = a->fail = a->locks = 0;
    a->workstation = NULL;
    a->lockTS = a->failRS = NULL;    a->ltCnt = a->frCnt = a->ltCap = a->frCap = 0;
    return a;
}
static void pushS(char ***arr, int *cnt, int *cap, const char *s)
{
    for (int i = 0; i < *cnt; ++i) if (!CMP((*arr)[i], s)) return;
    if (*cnt == *cap) { *cap = *cap ? *cap * 2 : 8;
                        *arr = (char**)realloc(*arr, *cap * sizeof **arr); }
    (*arr)[(*cnt)++] = xstrdup(s);
}

/* -- 6.  message-parsing helpers --------------------------- */
static char *userInSection(const char *msg,
                           const char *sectionHdr,
                           const char *tag)      /* usually "Account Name" */
{
    char *sec = ci_strstr(msg, sectionHdr);
    return sec ? afterTag(sec, tag) : NULL;
}

static char *userFromMsg(int id, const char *m)
{
    switch (id) {
    case 4740:  /* lock-out */
        return userInSection(m,
               "Account That Was Locked Out:", "Account Name");

    case 4625: { /* failed logon */
        char *u = userInSection(
               m, "Account For Which Logon Failed:", "Account Name");
        if (u && CMP(u, "-")) return u;          /* ignore “-” */
        free(u); return NULL;
    }

    case 4624:  /* success */
        return userInSection(m, "New Logon:", "Account Name");

    case 4776: case 4771:                       /* NTLM / Kerberos */
        return afterTag(m, "Logon Account");

    default:
        return NULL;
    }
}

static char *failReason(const char *msg)
{
    char *r = afterTagFull(msg, "Failure Reason");
    if (r && *r) return r;

    r = afterTagFull(msg, "Error Code");
    if (r && *r) return r;

    return afterTagFull(msg, "Status");
}

static char *workstationFromMsg(const char *m)
{
    char *w = afterTag(m, "Caller Computer Name"); if (w) return w;
    w = afterTag(m, "Source Workstation");          if (w) return w;
    return afterTag(m, "Workstation Name");
}
static bool isLock(int id, const char *m)
{
    if (id == 4740) return true;
    if (id == 4625 || id == 4776 || id == 4771)
        return ci_strstr(m, "account locked out") || ci_strstr(m, "0xC0000234");
    return false;
}
static bool isNtFail(int id, const char *m)
{
    return (id == 4776 || id == 4771) &&
           !(ci_strstr(m, "Error Code: 0") || ci_strstr(m, "Status: 0"));
}

/* -- 7.  getline-compat for MSVC --------------------------- */
#if defined(_MSC_VER)
static ssize_t getline_ms(char **buf, size_t *cap, FILE *fp)
{
    if (!*buf || !*cap) { *cap = 256; *buf = (char*)malloc(*cap); }
    size_t n = 0; int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (n + 2 >= *cap) { *cap <<= 1; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[n++] = (char)ch;
        if (ch == '\n') break;
    }
    if (n == 0 && ch == EOF) return -1;
    (*buf)[n] = '\0'; return (ssize_t)n;
}
#   define GETLINE getline_ms
#else
#   define GETLINE getline
#endif

/* -- 8.  CSV loader (robust) ------------------------------- */
static bool loadCSV(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return false; }

    char chunk[4096];

    /* drop header ------------------------------------------- */
    if (!fgets(chunk, sizeof chunk, fp)) { fclose(fp); return false; }

    size_t cap = 16384;
    char  *row = (char *)xmalloc(cap);

    while (1) {
        size_t len = 0; bool inQ = false;

        /* reassemble logical row --------------------------- */
        while (fgets(chunk, sizeof chunk, fp)) {
            size_t cLen = LEN(chunk);
            if (len + cLen + 1 > cap) { cap <<= 1; row = (char*)realloc(row, cap); }
            cyvaMemcpy(row + len, chunk, cLen + 1);
            len += cLen;

            for (size_t i = 0; i < cLen; ++i) if (chunk[i] == '"') inQ = !inQ;
            if (!inQ) break;
        }
        if (len == 0) break;                  /* EOF */

        while (len && (row[len-1] == '\n' || row[len-1] == '\r'))
            row[--len] = '\0';
        if (!*row) continue;

        char *cell[3] = { NULL, NULL, NULL };
        int col = 0; inQ = false;
        char *cptr = row, *beg = row;

        while (1) {
            if (*cptr == '"') { inQ = !inQ; ++cptr; continue; }
            bool delim = (!inQ && (*cptr == ',' || *cptr == '\t'));
            if (delim || *cptr == '\0') {
                if (col < 3) cell[col++] = beg;
                if (*cptr == '\0') break;
                *cptr = '\0'; beg = ++cptr; continue;
            }
            ++cptr;
        }
        if (col < 3) { fputs("Row with <3 columns skipped\n", stderr); continue; }

        for (int i = 0; i < 3; ++i) {
            if (cell[i][0] == '"') ++cell[i];
            size_t L = LEN(cell[i]);
            if (L && cell[i][L-1] == '"') cell[i][L-1] = '\0';
        }

        char *msg = cell[0], *ts = cell[1], *sid = cell[2];
        int id = (int)cyvaStrtol(sid, NULL, 10);
        pushEv(msg, ts, id);

        char *user = userFromMsg(id, msg);
        if (!user) continue;
        ACCT *a = getAcct(user, true); free(user);

        if (id == 4624) a->succ++;
        else if (id == 4625 || isNtFail(id, msg)) {
            a->fail++;
            char *r = failReason(msg);
            if (r) { pushS(&a->failRS, &a->frCnt, &a->frCap, r); free(r); }
        }
        if (isLock(id, msg)) {
            a->locks++;
            char *tsNice = niceTime(ts);
            pushS(&a->lockTS, &a->ltCnt, &a->ltCap, tsNice); free(tsNice);
            if (!a->workstation) a->workstation = workstationFromMsg(msg);
        }
    }

    free(row);
    fclose(fp);
    return true;
}

/* -- 9.  Reporting helpers -------------------------------- */
static void listAccountsAll(void)
{
    printf("\nAccounts (%d):\n", aCnt);
    for (int i = 0; i < aCnt; ++i) printf("  %s\n", acct[i].name);
    puts("");
}
static void listAccountsLocked(void)
{
    int n = 0; puts("\nLocked-out accounts:");
    for (int i = 0; i < aCnt; ++i) if (acct[i].locks) {
        printf("  %s\n", acct[i].name); ++n;
    }
    if (!n) puts("  (none)");
    puts("");
}
static void showAccount(const char *name)
{
    ACCT *a = getAcct(name, false);
    if (!a) { printf("  \"%s\" not found.\n\n", name); return; }

    printf("\n===== %s =====\n", a->name);
    printf("Successful logons : %d\n", a->succ);
    printf("Failed logons     : %d\n", a->fail);
    printf("Lock-out events   : %d\n", a->locks);
    printf("Workstation       : %s\n",
           a->workstation ? a->workstation : "(none)");

    puts("\nFailure reasons:");
    if (a->frCnt == 0) puts("  (none)");
    else
        for (int i = 0; i < a->frCnt; ++i)
            printf("  - %s\n", a->failRS[i]);

    puts("\nLock-out timestamps:");
    if (a->ltCnt == 0) puts("  (none)");
    else
        for (int i = 0; i < a->ltCnt; ++i)
            printf("  - %s\n", a->lockTS[i]);
}

/* -- 10.  Mini interactive driver ------------------------- */
static void LogAnalysisMenu(void)
{
    char path[260];
    printf("\nCSV path: "); fgets(path, sizeof path, stdin);
    path[CSPRINT(path, "\r\n")] = '\0';
    if (!*path) { puts("Cancelled.\n"); return; }

    if (!loadCSV(path)) return;

    for (;;) {
        puts("\n== NXLog Log-Analysis ==");
        puts(" 1  List all accounts");
        puts(" 2  List locked-out accounts");
        puts(" 3  Query account");
        puts(" 0  Back");
        printf("> ");

        int ch = getchar(); while (getchar() != '\n');
        if (ch == '0') break;
        if (ch == '1') { listAccountsAll();    continue; }
        if (ch == '2') { listAccountsLocked(); continue; }
        if (ch == '3') {
            char buf[128];
            printf("Account name: "); fgets(buf, sizeof buf, stdin);
            buf[CSPRINT(buf, "\r\n")] = '\0';
            if (*buf) showAccount(buf);
            continue;
        }
        puts("Invalid choice.\n");
    }
}

#endif /* LOG_ANALYSIS_H */
