/* ============================================================
   RegEx.h
   ============================================================ */
   
#ifndef CYVA_REGEX
#define CYVA_REGEX

#include <stdio.h>
#include <stdlib.h>
#include "Helper.h"
#include <stdbool.h>

typedef enum
{
    tokNone = 0,   /* nothing yet                           */
    tokItem = 1,   /* literal, char-class, group, etc.      */
    tokQuant = 2,   /* *, +, ?, {m,n}                        */
    tokAnchor = 3    /* ^ or $                                */
} TokType_e;

#define kMaxRegex  512

typedef struct Regex_t
{
    char      szBuf[kMaxRegex]; /* current pattern text          */
    int       nLen;             /* length in bytes               */
    TokType_e nLast;            /* what kind of token was last   */
} Regex_t;

/*--------------------------------------------------------------------------
  Primitive append / state checks
  --------------------------------------------------------------------------*/
static void rxReset(Regex_t* p)
{
    p->szBuf[0] = '\0';
    p->nLen = 0;
    p->nLast = tokNone;
}

static void rxAppend(Regex_t* p, const char* s)
{
    size_t len = cyvaStrlen(s);
    if (p->nLen + (int)len >= kMaxRegex - 1) {
        puts("[!] Pattern too long");
        return;
    }
    cyvaStrcpy(p->szBuf + p->nLen, s);
    p->nLen += (int)len;
}

/* If the last token was not an “item” (literal, class, group, etc.),
   then you can’t immediately apply a quantifier. */
static int rxNeedsItem(const Regex_t* p)
{
    return (p->nLast != tokItem);
}

/* If the last token is already a quantifier, you can’t add another. */
static int rxLastQuant(const Regex_t* p)
{
    return (p->nLast == tokQuant);
}

/*--------------------------------------------------------------------------
  OPTIMISATION PASSES
  - We canonicalise character classes (merge ranges, remove duplicates)
  - Compress back-to-back identical tokens (e.g. \d\d\d to \d{3})
  - cyvaStrip {1} or {1,1} where it’s pointless
  - Drop trivial capture groups around plain literals
  --------------------------------------------------------------------------*/

  /* Helper: compare two chars for sorting */
static int cmpChar(const void* a, const void* b)
{
    return (*(const char*)a - *(const char*)b);
}

/* Pass A: Inside any "[...]" block, gather all characters/ranges and merge them.
   E.g. turn “[c-aabdf]” into “[a-df]”.
   We do this by:
     1) Extract the contents between '[' and ']'
     2) Build an array of individual chars and expand any “x-y” ranges into all letters
     3) Sort + dedupe that array
     4) Reassemble as a minimal set of ranges + singletons.
*/
static void rxCanonicaliseClass(char* buf)
{
    char out[kMaxRegex];
    int  i = 0, o = 0;

    while (buf[i]) {
        if (buf[i] == '[') {
            int start = i;
            int end = start + 1;

            // Find closing bracket
            while (buf[end] && buf[end] != ']') end++;
            if (buf[end] != ']') {
                // Unmatched [, copy as-is
                while (i <= end && buf[i])
                    out[o++] = buf[i++];
                continue;
            }

            // Extract inside [...]
            char raw[256];
            int  rlen = 0;
            for (int k = start + 1; k < end; ++k)
                raw[rlen++] = buf[k];
            raw[rlen] = '\0';

            // Expand ranges into present[] array
            bool present[256] = { false };
            for (int k = 0; k < rlen; ++k) {
                if (k + 2 < rlen && raw[k + 1] == '-') {
                    unsigned char a = (unsigned char)raw[k];
                    unsigned char b = (unsigned char)raw[k + 2];
                    if (a > b) { unsigned char t = a; a = b; b = t; }
                    for (unsigned char c = a; c <= b; ++c)
                        present[c] = true;
                    k += 2;
                }
                else {
                    present[(unsigned char)raw[k]] = true;
                }
            }

            // Rebuild minimal character class
            out[o++] = '[';
            for (int c = 0; c < 256;) {
                if (!present[c]) { c++; continue; }

                int start_c = c;
                while (c + 1 < 256 && present[c + 1]) c++;
                int end_c = c;

                if (end_c > start_c + 1) {
                    out[o++] = (char)start_c;
                    out[o++] = '-';
                    out[o++] = (char)end_c;
                }
                else if (end_c == start_c + 1) {
                    out[o++] = (char)start_c;
                    out[o++] = (char)end_c;
                }
                else {
                    out[o++] = (char)start_c;
                }

                c++;
            }
            out[o++] = ']';
            i = end + 1;
        }
        else {
            out[o++] = buf[i++];
        }
    }

    out[o] = '\0';
    cyvaStrncpy(buf, out, kMaxRegex);
}

/* Pass B: Collapse consecutive identical “\d” or “\s” or “.” tokens
   into quantifiers. E.g., “\d\d\d” to “\d{3}”.
   We also collapse consecutive literal‐character tokens “x” to “x{n}” if they’re the same.
   Implementation detail:
     - Scan buf[] with index i
     - If you see a backslash and next char is ‘d’ or ‘s’, count how many in a row
     - If count > 1, replace them with “\d{count}” or “\s{count}”
     - Similar for “.” if it’s repeated (but careful: “…” means any‐any‐any, so we collapse to “.{3}”)
     - We do a in‐place rebuild into a temporary cyvaString, then copy back.
*/
static void rxCollapseSpecials(char* buf)
{
    char out[kMaxRegex];
    int  i = 0, o = 0;
    while (buf[i]) {
        /* \d or \s */
        if (buf[i] == '\\' && buf[i + 1] && (buf[i + 1] == 'd' || buf[i + 1] == 's')) {
            char c = buf[i + 1];
            int cnt = 0;
            while (buf[i] == '\\' && buf[i + 1] == c) {
                cnt++;
                i += 2;
            }
            if (cnt > 1) {
                o += snprintf(out + o, sizeof(out) - o, "\\%c{%d}", c, cnt);
            }
            else {
                out[o++] = '\\';
                out[o++] = c;
            }
        }
        /* literal . */
        else if (buf[i] == '.') {
            int cnt = 0;
            while (buf[i] == '.') {
                cnt++;
                i++;
            }
            if (cnt > 1) {
                o += snprintf(out + o, sizeof(out) - o, ".{%d}", cnt);
            }
            else {
                out[o++] = '.';
            }
        }
        else {
            out[o++] = buf[i++];
        }
    }
    out[o] = '\0';
    cyvaStrncpy(buf, out, kMaxRegex);
}

/* Pass C: Remove any quantifier that is “{1}” or “{1,1}” since it’s redundant. */
static void rxRemoveTrivialQuant(char* buf)
{
    char out[kMaxRegex];
    int  i = 0, o = 0;
    while (buf[i]) {
        if (buf[i] == '{') {
            /* look ahead for “1}” or “1,1}” */
            if (cyvaStrncmp(buf + i, "{1}", 3) == 0) {
                i += 3;  /* skip “{1}” */
                continue;
            }
            if (cyvaStrncmp(buf + i, "{1,1}", 5) == 0) {
                i += 5;  /* skip “{1,1}” */
                continue;
            }
        }
        out[o++] = buf[i++];
    }
    out[o] = '\0';
    cyvaStrcpy(buf, out);
}

/* Pass D: Drop trivial capture groups around a pure literal (e.g., “(x)” to “x”).
   We only remove if the inside is exactly one character and not a special character “\d”/“\s”/etc. */
static void rxDropTrivialGroup(char* buf)
{
    char out[kMaxRegex];
    int  i = 0, o = 0;
    while (buf[i]) {
        if (buf[i] == '(' && buf[i + 2] == ')' && buf[i + 1] != '\\') {
            /* single‐char group “(x)” - just ‘x’ */
            out[o++] = buf[i + 1];
            i += 3;
        }
        else {
            out[o++] = buf[i++];
        }
    }
    out[o] = '\0';
    cyvaStrcpy(buf, out);
}

/* Wrapper: Run all four passes until no further change */
static void rxOptimiseBuffer(char* buf)
{
    char prev[kMaxRegex];
    do {
        cyvaStrcpy(prev, buf);
        rxCanonicaliseClass(buf);
        rxCollapseSpecials(buf);
        rxRemoveTrivialQuant(buf);
        rxDropTrivialGroup(buf);
    } while (cyvaStrcmp(prev, buf) != 0);
}

/* Since our helpers call rxOptimiseBuffer on p->szBuf after each append,
   we only need a short wrapper here. */
static void rxOptimise(Regex_t* p)
{
    rxOptimiseBuffer(p->szBuf);
    p->nLen = (int)cyvaStrlen(p->szBuf);
}

/*--------------------------------------------------------------------------
  HIGH-LEVEL “ADD” HELPERS FOR REGEX
  --------------------------------------------------------------------------*/

  /* 1) Add a single literal character “x” */
static void rxAddLit(Regex_t* p, char c)
{
    char tmp[2] = { c, '\0' };
    rxAppend(p, tmp);
    p->nLast = tokItem;
    rxOptimise(p);
}

/* 2) Add a raw token cyvaString (\, ., [..], or entire group or preset). */
static void rxAddTok(Regex_t* p, const char* s)
{
    rxAppend(p, s);
    p->nLast = tokItem;
    rxOptimise(p);
}

/* 3) Add a character‐class “[abcd]” from the inside text “abcd” */
static void rxAddRange(Regex_t* p, const char* s)
{
    char tmp[kMaxRegex];
    snprintf(tmp, sizeof tmp, "[%s]", s);
    rxAddTok(p, tmp);
}

/* 4) Repeat exactly “{n}” – requires that there is already an item and no existing quant */
static void rxAddExact(Regex_t* p, int n)
{
    if (rxNeedsItem(p) || rxLastQuant(p)) {
        puts("[!] Nothing to repeat");
        return;
    }
    char tmp[24];
    snprintf(tmp, sizeof tmp, "{%d}", n);
    rxAppend(p, tmp);
    p->nLast = tokQuant;
    rxOptimise(p);
}

/* 5) Repeat between “{m,n}” – same conditions as above */
static void rxAddBetween(Regex_t* p, int m, int n)
{
    if (rxNeedsItem(p) || rxLastQuant(p)) {
        puts("[!] Nothing to repeat");
        return;
    }
    char tmp[32];
    snprintf(tmp, sizeof tmp, "{%d,%d}", m, n);
    rxAppend(p, tmp);
    p->nLast = tokQuant;
    rxOptimise(p);
}

/* 6) Make previous token optional (“?”) – same rules */
static void rxAddOpt(Regex_t* p)
{
    if (rxNeedsItem(p) || rxLastQuant(p)) {
        puts("[!] Nothing to mark optional");
        return;
    }
    rxAppend(p, "?");
    p->nLast = tokQuant;
    rxOptimise(p);
}

/* 7) Group “(...)” around raw text. We trust the user to supply valid inside. */
static void rxAddGroup(Regex_t* p, const char* s)
{
    char tmp[kMaxRegex];
    snprintf(tmp, sizeof tmp, "(%s)", s);
    rxAddTok(p, tmp);
}

/* 8) Alternation “(a|b|c)” – same as group wrapper */
static void rxAddAlt(Regex_t* p, const char* s)
{
    char tmp[kMaxRegex];
    snprintf(tmp, sizeof tmp, "(%s)", s);
    rxAddTok(p, tmp);
}

/* 9) Anchor “^” or “$”.  Must not place “^” if pattern already started, and not place “$” twice. */
static void rxAddAnchor(Regex_t* p, char a)
{
    if (a == '^' && p->nLen > 0) {
        puts("[!] '^' must be first");
        return;
    }
    if (a == '$' && p->nLast == tokAnchor) {
        puts("[!] Already ended");
        return;
    }
    char tmp[2] = { a, '\0' };
    rxAppend(p, tmp);
    p->nLast = tokAnchor;
    rxOptimise(p);
}

static void rxAddPreset(Regex_t* p, int id)
{
    p->nLast = tokItem;  // Presets always add an item
    switch (id) {
    case 1:
        cyvaStrcpy(p->szBuf,
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)");
        break;

    case 2:
        cyvaStrcpy(p->szBuf,
            "[A-Za-z0-9._%+-]+@"
            "[A-Za-z0-9.-]+\\.[A-Za-z]{2,}");
        break;

    case 3:
        cyvaStrcpy(p->szBuf,
            "https?://"
            "[A-Za-z0-9._~:/?#@!$&'()*+,;=-]+");
        break;

    case 4:
        cyvaStrcpy(p->szBuf, "[A-Fa-f0-9]{32}");
        break;

    case 5:
        cyvaStrcpy(p->szBuf, "[A-Fa-f0-9]{64}");
        break;

    case 6:
        cyvaStrcpy(p->szBuf, "[A-Za-z]:\\\\(?:[^\\\\/:*?\"<>|\\r\\n]+\\\\?)*");
        break;

    default:
        puts("[!] Unknown preset");
        return;
    }

    p->nLen = (int)cyvaStrlen(p->szBuf);
    // SKIP rxOptimise() — it's not safe for preset patterns
}

/* =========================================================================
   REGEX BUILDER – driver
   (uses PromptText, PromptChar, PromptInt defined earlier)
   =========================================================================*/

   /* print the current pattern at the top */
static void rxShow(const Regex_t* pRx)
{
    printf("\nCurrent pattern: /%s/\n\n",
        pRx->szBuf[0] ? pRx->szBuf : "(empty)");
}

/* main menu for building regex */
static void rxMenu(void)
{
    puts("REGEX BUILDER MENU:");
    puts(" 1  One exact character");
    puts(" 2  Range of characters [a-z]");
    puts(" 3  Any digit (\\d)");
    puts(" 4  Any whitespace (\\s)");
    puts(" 5  Any character (.)");
    puts(" 6  Repeat last exactly n times");
    puts(" 7  Repeat last between m and n times");
    puts(" 8  Make last optional");
    puts(" 9  Group (…)");
    puts("10  Alternation a|b|c");
    puts("11  Anchor ^ or $");
    puts("12  Common Cybersecurity RegEx Presets");
    puts("13  Clear pattern");
    puts(" 0  Return to main menu\n");
}

/* submenu for the six cyber-sec presets */
static void rxPresetMenu(void)
{
    puts("Common Cybersecurity RegEx Presets:");
    puts(" 1  IPv4 address");
    puts(" 2  Email address");
    puts(" 3  HTTP/HTTPS URL");
    puts(" 4  MD5 hash");
    puts(" 5  SHA-256 hash");
    puts(" 6  Windows file path");
    puts(" 0  Back\n");
}

static void regexBuilder(void)
{
    Regex_t rx;
    rxReset(&rx);

    char szBuf[64];
    int  nChoice;

    while (1)
    {
        /* 1) Show current built‐up pattern */
        rxShow(&rx);

        /* 2) Show menu of possible next steps */
        rxMenu();

        /* 3) Read the user’s menu choice as text, then atoi it */
        if (!promptText("Choice: ", szBuf, sizeof(szBuf))) {
            return;  /* EOF or error - go back to main menu */
        }
        nChoice = atoi(szBuf);

        switch (nChoice)
        {
        case 0:
            /* Return to CYVA main menu */
            return;

        case 1: {  /* One exact character */
            char ch;
            if (promptChar("Character: ", &ch)) {
                rxAddLit(&rx, ch);
            }
            break;
        }

        case 2:  /* Range of characters [a-z] */
            if (promptText("Range (e.g. a-z): ",
                szBuf, sizeof(szBuf))) {
                rxAddRange(&rx, szBuf);
            }
            break;

        case 3:  /* Any digit (\d) */
            rxAddTok(&rx, "\\d");
            break;

        case 4:  /* Any whitespace (\s) */
            rxAddTok(&rx, "\\s");
            break;

        case 5:  /* Any character (.) */
            rxAddTok(&rx, ".");
            break;

        case 6: {  /* Repeat last exactly n times */
            int n;
            if (promptInt("n = ", 1, 100, &n)) {
                rxAddExact(&rx, n);
            }
            break;
        }

        case 7: {  /* Repeat last between m and n times */
            int m, n;
            if (!promptInt("m = ", 1, 100, &m)) break;
            if (!promptInt("n = ", m, 100, &n)) break;
            rxAddBetween(&rx, m, n);
            break;
        }

        case 8:  /* Make last optional (?) */
            rxAddOpt(&rx);
            break;

        case 9:  /* Group (…) */
            if (promptText("Group content: ",
                szBuf, sizeof(szBuf))) {
                rxAddGroup(&rx, szBuf);
            }
            break;

        case 10: /* Alternation a|b|c */
            if (promptText("Alternatives (a|b|c): ",
                szBuf, sizeof(szBuf))) {
                rxAddAlt(&rx, szBuf);
            }
            break;

        case 11: { /* Anchor ^ or $ */
            char c;
            if (promptChar("^ or $: ", &c) && (c == '^' || c == '$')) {
                rxAddAnchor(&rx, c);
            }
            break;
        }

        case 12: { /* Common Cybersecurity RegEx Presets */
            int nPreset;
            rxPresetMenu();

            if (!promptInt("Preset #: ", 0, 6, &nPreset)) {
                /* EOF or error - return to main menu loop */
                break;
            }
            if (nPreset != 0) {
                rxAddPreset(&rx, nPreset);
            }
            break;
        }

        case 13:  /* Clear pattern */
            rxReset(&rx);
            break;

        default:
            puts("[!] Invalid option.");
            break;
        }
    }
}

#endif
