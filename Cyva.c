#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------- tunables -------- */
#define cMaxTopics     300
#define cMaxSubtopics  10
#define cMaxRelations  10
#define cMaxTags       5
#define cMaxDomains    50
#define cStrLen        256
#define cFileName      "C:\\Users\\Gabe\\source\\repos\\Cyva\\Cyva\\topics.txt"

/* -------- data structures -------- */
typedef struct {
    int   nId;
    char  szName[cStrLen];
    char  szDesc[cStrLen];
    char  szDomain[cStrLen];
    char  aszTags[cMaxTags][cStrLen];
    int   nTagCount;
    int   nSubCount;
    int   anSubIds[cMaxSubtopics];
    int   nRelCount;
    int   anRelIds[cMaxRelations];
} Topic;

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

/* -------- globals -------- */
Topic aTopics[cMaxTopics];
int   nTopicCount = 0;
char  aszDomains[cMaxDomains][cStrLen];
int   nDomainCount = 0;

/*=================================================================
  printDesc
  ---------
  Prints sz, but converts every back-tick (`) to a newline.
==================================================================*/
static void printDesc(const char* sz)
{
    const char* p;
    for (p = sz; *p != '\0'; ++p) {
        if (*p == '`')
            putchar('\n');
        else
            putchar(*p);
    }
}

/**
 * readFullLine
 * ---------------
 * Reads an entire line from fp (e.g. stdin), regardless of length.
 * Strips any trailing '\n'. On success, returns 1 and *outLine points to
 * a heap-allocated buffer (caller must free). On EOF/error returns 0
 * and leaves *outLine unchanged.
 */
static int readFullLine(FILE* fp, char** outLine)
{
    const size_t kChunk = 1024;
    size_t       cap = kChunk;
    size_t       len = 0;
    char* buf = malloc(cap);
    if (!buf) return 0;

    while (1) {
        int ch = fgetc(fp);
        if (ch == EOF || ch == '\n') {
            buf[len] = '\0';
            *outLine = buf;
            return (len || ch == '\n') ? 1 : 0;
        }
        buf[len++] = (char)ch;
        if (len + 1 >= cap) {
            cap *= 2;
            char* tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                return 0;
            }
            buf = tmp;
        }
    }
}

static int promptText(const char* szMsg, char* szDst, size_t nCap)
{
    printf("%s", szMsg);
    fflush(stdout);

    /* Read a line (up to nCap‐1 chars) */
    if (!fgets(szDst, (int)nCap, stdin)) {
        return 0;   /* EOF or read error */
    }

    /* Strip any trailing newline */
    szDst[strcspn(szDst, "\n")] = '\0';
    return 1;
}

static int promptChar(const char* szMsg, char* pchOut)
{
    char szTmp[64] = { 0 };
    if (!promptText(szMsg, szTmp, sizeof szTmp)) {
        return 0;   /* EOF or read error */
    }
    if (szTmp[0] == '\0') {
        puts("[!] Must enter a character.");
        return 0;
    }
    *pchOut = szTmp[0];
    return 1;
}

static int promptInt(const char* szMsg, int nLow, int nHigh, int* pnOut)
{
    char szTmp[64] = { 0 };

    while (1) {
        if (!promptText(szMsg, szTmp, sizeof szTmp)) {
            return 0;   /* EOF or error */
        }

        errno = 0;
        char* endptr = NULL;
        long val = strtol(szTmp, &endptr, 10);
        if (errno || endptr == szTmp || *endptr != '\0') {
            puts("[!] Please enter a valid integer.");
            continue;
        }
        if (val < nLow || val > nHigh) {
            printf("[!] Enter a number between %d and %d.\n", nLow, nHigh);
            continue;
        }
        *pnOut = (int)val;
        return 1;
    }
}

/*=================================================================
  loadTopicsFromFile
==================================================================*/
void loadTopicsFromFile(void)
{
    FILE* fp = fopen(cFileName, "r");
    if (fp == NULL) {
        perror("Failed to open topics.txt");
        return;
    }

    char* dynLine = NULL;

    while (readFullLine(fp, &dynLine)) {

        /*---------------------------------------------------------
          1.  Split into exactly 10 pipe-separated fields
        ----------------------------------------------------------*/
        char* fields[10];
        char* cursor = dynLine;
        int   i;

        for (i = 0; i < 9; ++i) {
            fields[i] = cursor;

            /* find next pipe */
            char* pipePos = strchr(cursor, '|');

            if (pipePos != NULL) {
                *pipePos = '\0';         /* terminate this field   */
                cursor = pipePos + 1;  /* advance to next field  */
            }
            else {
                /* fewer than 10 pipes – pad remaining */
                int j;
                for (j = i + 1; j < 10; ++j)
                    fields[j] = (char*)"";
                break;
            }
        }

        if (i == 9)
            fields[9] = cursor;          /* last field */
        else
            fields[9] = (char*)"";

        /*---------------------------------------------------------
          2.  Populate Topic struct
        ----------------------------------------------------------*/
        if (nTopicCount >= cMaxTopics) {
            fprintf(stderr, "Topic array full; skipping remaining lines\n");
            free(dynLine);
            break;
        }

        Topic* T = &aTopics[nTopicCount];

        T->nId = atoi(fields[0]);

        strncpy(T->szName, fields[1], cStrLen); T->szName[cStrLen - 1] = '\0';
        strncpy(T->szDesc, fields[2], cStrLen); T->szDesc[cStrLen - 1] = '\0';
        strncpy(T->szDomain, fields[3], cStrLen); T->szDomain[cStrLen - 1] = '\0';

        /* ---------- tags ---------- */
        T->nTagCount = atoi(fields[4]);
        if (T->nTagCount > cMaxTags)
            T->nTagCount = cMaxTags;

        int tagIdx = 0;
        char* tagCur = fields[5];

        while (tagCur != NULL && *tagCur != '\0' && tagIdx < T->nTagCount) {
            char* comma = strchr(tagCur, ',');
            if (comma != NULL)
                *comma = '\0';

            strncpy(T->aszTags[tagIdx], tagCur, cStrLen);
            T->aszTags[tagIdx][cStrLen - 1] = '\0';

            ++tagIdx;
            if (comma == NULL)
                break;
            tagCur = comma + 1;
        }

        /* ---------- sub-topics ---------- */
        T->nSubCount = atoi(fields[6]);
        if (T->nSubCount > cMaxSubtopics)
            T->nSubCount = cMaxSubtopics;

        int subIdx = 0;
        char* subCur = fields[7];

        while (subCur != NULL && *subCur != '\0' && subIdx < T->nSubCount) {
            char* comma = strchr(subCur, ',');
            if (comma != NULL)
                *comma = '\0';

            T->anSubIds[subIdx] = atoi(subCur);
            ++subIdx;

            if (comma == NULL)
                break;
            subCur = comma + 1;
        }

        /* ---------- related ---------- */
        T->nRelCount = atoi(fields[8]);
        if (T->nRelCount > cMaxRelations)
            T->nRelCount = cMaxRelations;

        int relIdx = 0;
        char* relCur = fields[9];

        while (relCur != NULL && *relCur != '\0' && relIdx < T->nRelCount) {
            char* comma = strchr(relCur, ',');
            if (comma != NULL)
                *comma = '\0';

            T->anRelIds[relIdx] = atoi(relCur);
            ++relIdx;

            if (comma == NULL)
                break;
            relCur = comma + 1;
        }

        /*---------------------------------------------------------
          3.  Register domain  (umbrella OR no '(' char)
        ----------------------------------------------------------*/
        int   keepDomain = 0;
        if (strstr(T->szDomain, "Evidence Sources") != NULL)
            keepDomain = 1;                                  /* umbrella */
        else if (strchr(T->szDomain, '(') == NULL)
            keepDomain = 1;                                  /* regular  */

        if (keepDomain) {
            int exists = 0;
            int d;
            for (d = 0; d < nDomainCount; ++d) {
                if (strcmp(aszDomains[d], T->szDomain) == 0) {
                    exists = 1;
                    break;
                }
            }

            if (!exists && nDomainCount < cMaxDomains) {
                strncpy(aszDomains[nDomainCount], T->szDomain, cStrLen);
                aszDomains[nDomainCount][cStrLen - 1] = '\0';
                ++nDomainCount;
            }
        }

        ++nTopicCount;
        free(dynLine);      /* release current line buffer */
        dynLine = NULL;
    }

    fclose(fp);
}

/*--------------------------------------------------------------------------
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  --------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
   showSuggestionsForTopic
   -----------------------
   Prints the “Suggestions” list for topic *p*.
   Uses ID - index conversion so the correct names are displayed.
--------------------------------------------------------------------------- */
static void showSuggestionsForTopic(const Topic* p)
{
    int i;

    printf("\n=== Suggestions ===\n");
    if (p->nRelCount == 0) {
        printf("  (none)\n");
        return;
    }

    for (i = 0; i < p->nRelCount; ++i) {
        int  relId = p->anRelIds[i];
        int  relIdx = findTopicIndexById(relId);

        if (relIdx != -1) {
            printf("  [%d] %s\n", aTopics[relIdx].nId,
                aTopics[relIdx].szName);
        }
    }
}

/* ===========================================================
   getDirFromDesc()
   -----------------
   We treat the FIRST chunk of szDesc (up to the first back-tick
   or newline) as “the directory / registry path”.  If it begins
   with a drive letter, UNC (\\), %, or HK (registry), we return
   a *duplicate* string – caller must free().  Otherwise NULL.

   If we encounter '<' or '{' within the directory, we only open
   the lowest level possible before such, such are placeholder
   for parameters that the user must provide.
   =========================================================== */
static char* getDirFromDesc(const char* szDesc)
{
    if (!szDesc || !*szDesc) return NULL;

    /* grab the first physical line / before first back-tick */
    const char* end = strpbrk(szDesc, "`\r\n");
    size_t      n = end ? (size_t)(end - szDesc) : strlen(szDesc);

    if (n < 2) return NULL;

    /* reject obvious registry paths */
    if (!strncmp(szDesc, "HK", 2))
        return NULL;

    /* minimal validation for filesystem paths */
    if (!(isalpha((unsigned char)szDesc[0]) && szDesc[1] == ':') &&   /* C:\ */
        strncmp(szDesc, "\\\\", 2) &&                               /* UNC */
        szDesc[0] != '%')                                              /* %VAR% */
        return NULL;

    /* copy into scratch buffer and NUL-terminate */
    char* tmp = (char*)malloc(n + 1);
    if (!tmp) return NULL;
    memcpy(tmp, szDesc, n);
    tmp[n] = '\0';

    /* walk through segments and stop before placeholders / filenames */
    char* out = (char*)malloc(n + 2);          /* enough */
    if (!out) { free(tmp); return NULL; }
    out[0] = '\0';

    char* tok = strtok(tmp, "\\/");
    int   needSlash = 0;

    while (tok) {
        if (strchr(tok, '<') || strchr(tok, '{'))
            break;                              /* placeholder segment    */
        if (strchr(tok, '.'))
            break;                              /* filename (has a dot)   */

        if (needSlash)  strcat(out, "\\");
        strcat(out, tok);
        needSlash = 1;

        tok = strtok(NULL, "\\/");
    }

    free(tmp);

    /* if we never copied anything, it wasn’t usable */
    if (out[0] == '\0') { free(out); return NULL; }

    return out;                                 /* caller frees */
}

/* ===========================================================
   openDir()
   ---------
   Cross-platform “best effort” opener.  Windows uses explorer,
   others fall back to xdg-open or open.
   =========================================================== */
static void openDir(const char* path)
{
# if defined(_WIN32)
    /* Surround with quotes in case of spaces */
    char cmd[512];
    snprintf(cmd, sizeof cmd, "explorer \"%s\"", path);
    system(cmd);
# elif defined(__APPLE__)
    char cmd[512];
    snprintf(cmd, sizeof cmd, "open \"%s\"", path);
    system(cmd);
# else
    char cmd[512];
    snprintf(cmd, sizeof cmd, "xdg-open \"%s\"", path);
    system(cmd);
# endif
}

/* ===========================================================
   exploreTopic
   ------------
   Shows one topic, its subtopics, and related suggestions.
   Uses findTopicIndexById() so IDs and names are accurate.
=========================================================== */
void exploreTopic(int nArrayIdx)
{
    const Topic* p = &aTopics[nArrayIdx];
    int          i;

    printf("\n--- %s ---\n", p->szName);
    printDesc(p->szDesc);
    printf("\nDomain: %s\nTags:", p->szDomain);
    for (i = 0; i < p->nTagCount; ++i)
        printf(" %s", p->aszTags[i]);
    printf("\n");

    /* ---------- Subtopics ---------- */
    if (p->nSubCount > 0) {
        printf("\nSubtopics:\n");
        for (i = 0; i < p->nSubCount; ++i) {
            int subId = p->anSubIds[i];
            int subIdx = findTopicIndexById(subId);

            if (subIdx != -1) {
                printf("[%d] %s\n", aTopics[subIdx].nId,
                    aTopics[subIdx].szName);
            }
        }
    }
    else {
        printf("(no subtopics)\n");
    }

    /* ---------- Suggestions ---------- */
    showSuggestionsForTopic(p);

    char* pathForO = getDirFromDesc(p->szDesc);
    if (pathForO)
    {
        printf("\n  (press 'o' to open %s)\n", pathForO);
        free(pathForO);   /* only show — real opening will be in loop */
    }
}

/* ===========================================================
   helper to print all topics under a given domain
========================================================== = */
static void printDomainTopics(const char* szDomain) {
    printf("\nTopics under '%s':\n", szDomain);
    for (int i = 0; i < nTopicCount; i++) {
        if (strcmp(aTopics[i].szDomain, szDomain) == 0) {
            printf("  [%d] %s\n", aTopics[i].nId, aTopics[i].szName);
        }
    }
}

/* ===========================================================
   ID  -  array-index helper
   Returns -1 if the ID does not exist.
   =========================================================== */
static int findTopicIndexById(int id)
{
    int idx;
    for (idx = 0; idx < nTopicCount; ++idx) {
        if (aTopics[idx].nId == id)
            return idx;
    }
    return -1;
}

/* ===========================================================
   topicIdInDomain
   ----------------
   Checks whether the topic whose *ID* equals id
   belongs to the domain string szDomain.
   (We pass an ID, not an array slot.)
   =========================================================== */
static bool topicIdInDomain(int id, const char* szDomain)
{
    int idx = findTopicIndexById(id);
    if (idx == -1)
        return false;                               /* ID not found */
    return strcmp(aTopics[idx].szDomain, szDomain) == 0;
}

/* ===========================================================
   browseDomainTopics
   =========================================================== */
void browseDomainTopics(const char* szStartDomain, bool bAllowDomainSwitch)
{
    char domain[cStrLen];
    char buf[cStrLen];

    strncpy(domain, szStartDomain, cStrLen);
    domain[cStrLen - 1] = '\0';

    while (1) {
        system("cls");
        printDomainTopics(domain);

        printf("\nSelect ID to explore ('x' = main menu): ");
        if (!fgets(buf, sizeof buf, stdin)) return;
        if (buf[0] == 'x' || buf[0] == 'X') return;

        /* ---------- convert typed ID to array index ---------- */
        int typedId = atoi(buf);
        int currentIdx = findTopicIndexById(typedId);
        if (currentIdx == -1)           continue;          /* bad ID */
        if (!topicIdInDomain(typedId, domain)) continue;   /* ID not in domain */

        /* ---------- drill-down loop for this selection ------- */
        while (1) {
            system("cls");
            exploreTopic(currentIdx);  /* prints name, desc, subtopics, suggestions */

            printf("\nSelect ID to explore "
                "('o' = open dir, 'x' = back to '%s'): ", domain);
            if (!fgets(buf, sizeof buf, stdin)) return;

            if (buf[0] == 'x' || buf[0] == 'X')
                break;

            /* ---------- open directory, if any ---------- */
            if (buf[0] == 'o' || buf[0] == 'O')
            {
                char* path = getDirFromDesc(aTopics[currentIdx].szDesc);
                if (path) {
                    openDir(path);
                    free(path);
                }
                else {
                    puts("No valid path found in this entry.");
                }
                continue;   /* stay on current topic */
            }

            int nextId = atoi(buf);
            int nextIdx = findTopicIndexById(nextId);
            if (nextIdx == -1) continue;            /* invalid ID */

            /* --- allow only legitimate transitions ----------- */
            bool ok = false;
            int i;

            for (i = 0; i < aTopics[currentIdx].nSubCount; ++i)
                if (aTopics[currentIdx].anSubIds[i] == nextId) { ok = true; break; }

            if (!ok) {
                for (i = 0; i < aTopics[currentIdx].nRelCount; ++i)
                    if (aTopics[currentIdx].anRelIds[i] == nextId) { ok = true; break; }
            }

            if (!ok) continue;                       /* not a valid child */

            /* --- optional automatic domain switch ------------ */
            if (bAllowDomainSwitch &&
                strcmp(aTopics[nextIdx].szDomain, domain) != 0)
            {
                strncpy(domain, aTopics[nextIdx].szDomain, cStrLen);
                domain[cStrLen - 1] = '\0';
            }

            currentIdx = nextIdx;                   /* move to chosen topic */
        }
        /* out of drill-down: show domain list again */
    }
}

/* ---------------------------------------------------------------------------
   Option 4: searchByTag
   - prompt for a tag
   - list matches (or “none”)
   - let user pick one, then drill immediately into that topic
   - on 'x', return to a fixed-domain browse of its home domain
   ------------------------------------------------------------------------- */
void searchByTag(void)
{
    char tag[cStrLen], buf[cStrLen];
    int  results[cMaxTopics], rc = 0;

    printf("\nEnter tag to search: ");
    if (!fgets(tag, sizeof tag, stdin)) return;
    strtok(tag, "\r\n");

    /* find matches */
    int i;
    for (i = 0; i < nTopicCount; ++i) {
        int t;
        for (t = 0; t < aTopics[i].nTagCount; ++t) {
            if (strcmp(tag, aTopics[i].aszTags[t]) == 0) {
                results[rc++] = aTopics[i].nId;   /* store *ID*, not index */
                break;
            }
        }
    }

    if (rc == 0) {
        printf("\nNo topics found with tag '%s'.\n", tag);
        return;
    }

    printf("\nTopics matching '%s':\n", tag);
    for (i = 0; i < rc; ++i) {
        int idx = findTopicIndexById(results[i]);
        printf("  [%d] %s  (domain: %s)\n",
            aTopics[idx].nId, aTopics[idx].szName, aTopics[idx].szDomain);
    }

    printf("\nSelect ID to explore ('x' = main menu): ");
    if (!fgets(buf, sizeof buf, stdin)) return;
    if (buf[0] == 'x' || buf[0] == 'X') return;

    int startId = atoi(buf);
    int current = findTopicIndexById(startId);
    if (current == -1) return;

    char home[cStrLen];
    strncpy(home, aTopics[current].szDomain, cStrLen);
    home[cStrLen - 1] = '\0';

    while (1) {
        system("cls");
        exploreTopic(current);

        printf("\nSelect ID to explore ('x' = back to domain '%s'): ", home);
        if (!fgets(buf, sizeof buf, stdin)) return;
        if (buf[0] == 'x' || buf[0] == 'X') break;

        int nextId = atoi(buf);
        int nextIdx = findTopicIndexById(nextId);
        if (nextIdx == -1) continue;

        /* allow transitions */
        int ok = 0;
        int s;
        for (s = 0; s < aTopics[current].nSubCount; ++s)
            if (aTopics[current].anSubIds[s] == nextId) { ok = 1; break; }
        for (s = 0; s < aTopics[current].nRelCount; ++s)
            if (aTopics[current].anRelIds[s] == nextId) { ok = 1; break; }
        if (ok)
            current = nextIdx;
    }

    browseDomainTopics(home, false);
}

/* ===========================================================
   Domain overview
   =========================================================== */
static void showDomainOverview(void)
{
    puts("\nDomain overview:");
    for (int d = 0; d < nDomainCount; d++) {
        int cnt = 0;
        for (int i = 0; i < nTopicCount; i++)
            if (!strcmp(aTopics[i].szDomain, aszDomains[d])) cnt++;
        printf("  %s  (%d topics)\n", aszDomains[d], cnt);
    }
}

/*--------------------------------------------------------------------------
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  --------------------------------------------------------------------------*/

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
    size_t len = strlen(s);
    if (p->nLen + (int)len >= kMaxRegex - 1) {
        puts("[!] Pattern too long");
        return;
    }
    strcpy(p->szBuf + p->nLen, s);
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
  - Strip {1} or {1,1} where it’s pointless
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
    int  i = 0, j;
    while (buf[i]) {
        if (buf[i] == '[') {
            int start = i;
            /* find matching ']' */
            int end = start + 1;
            while (buf[end] && buf[end] != ']') end++;
            if (buf[end] != ']') {
                /* unbalanced, just copy as is */
                out[j = 0] = '\0';
                break;
            }
            /* extract everything between start+1 ... end-1 */
            char temp[kMaxRegex];
            int  tlen = 0, k;
            for (k = start + 1; k < end; k++) {
                temp[tlen++] = buf[k];
            }
            temp[tlen] = '\0';

            /* expand into an array of chars */
            int   present[256] = { 0 };
            char  expanded[256];
            int   exLen = 0;
            for (k = 0; k < tlen; k++) {
                if (k + 2 < tlen && temp[k + 1] == '-') {
                    /* “x-y” range */
                    char a = temp[k], b = temp[k + 2];
                    if (a <= b) {
                        for (char c = a; c <= b; c++) {
                            present[(unsigned char)c] = 1;
                        }
                    }
                    else {
                        for (char c = b; c <= a; c++) {
                            present[(unsigned char)c] = 1;
                        }
                    }
                    k += 2;
                }
                else {
                    present[(unsigned char)temp[k]] = 1;
                }
            }
            /* Build a sorted list of unique chars */
            for (k = 0; k < 256; k++) {
                if (present[k]) {
                    expanded[exLen++] = (char)k;
                }
            }
            /* Now convert expanded[] back into minimal ranges + singles */
            char rebuilt[kMaxRegex];
            int  rlen = 0;
            k = 0;
            while (k < exLen) {
                char startC = expanded[k];
                char endC = startC;
                while (k + 1 < exLen && (unsigned char)expanded[k + 1] == (unsigned char)endC + 1) {
                    endC = expanded[++k];
                }
                if (endC > startC + 1) {
                    rebuilt[rlen++] = startC;
                    rebuilt[rlen++] = '-';
                    rebuilt[rlen++] = endC;
                }
                else if (endC == startC + 1) {
                    /* two consecutive chars - either “ab” or just “a-b”?
                       For consistency, we’ll output “a-b”. */
                    rebuilt[rlen++] = startC;
                    rebuilt[rlen++] = '-';
                    rebuilt[rlen++] = endC;
                }
                else {
                    rebuilt[rlen++] = startC;
                }
                k++;
            }
            rebuilt[rlen] = '\0';

            /* Copy prefix (up to '[') */
            for (j = 0; j < start; j++) {
                out[j] = buf[j];
            }
            out[j++] = '[';
            /* Copy rebuilt inside */
            for (k = 0; k < rlen; k++) {
                out[j++] = rebuilt[k];
            }
            out[j++] = ']';

            /* Copy suffix (from end+1 onward) */
            int suffixStart = end + 1;
            while (buf[suffixStart]) {
                out[j++] = buf[suffixStart++];
            }
            out[j] = '\0';

            /* Overwrite buf with out */
            strcpy(buf, out);
            /* Start over (in case nested classes or multiple classes) */
            i = 0;
        }
        else {
            i++;
        }
    }
}

/* Pass B: Collapse consecutive identical “\d” or “\s” or “.” tokens
   into quantifiers. E.g., “\d\d\d” to “\d{3}”.
   We also collapse consecutive literal‐character tokens “x” to “x{n}” if they’re the same.
   Implementation detail:
     - Scan buf[] with index i
     - If you see a backslash and next char is ‘d’ or ‘s’, count how many in a row
     - If count > 1, replace them with “\d{count}” or “\s{count}”
     - Similar for “.” if it’s repeated (but careful: “…” means any‐any‐any, so we collapse to “.{3}”)
     - We do a in‐place rebuild into a temporary string, then copy back.
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
    strncpy(buf, out, kMaxRegex);
}

/* Pass C: Remove any quantifier that is “{1}” or “{1,1}” since it’s redundant. */
static void rxRemoveTrivialQuant(char* buf)
{
    char out[kMaxRegex];
    int  i = 0, o = 0;
    while (buf[i]) {
        if (buf[i] == '{') {
            /* look ahead for “1}” or “1,1}” */
            if (strncmp(buf + i, "{1}", 3) == 0) {
                i += 3;  /* skip “{1}” */
                continue;
            }
            if (strncmp(buf + i, "{1,1}", 5) == 0) {
                i += 5;  /* skip “{1,1}” */
                continue;
            }
        }
        out[o++] = buf[i++];
    }
    out[o] = '\0';
    strcpy(buf, out);
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
    strcpy(buf, out);
}

/* Wrapper: Run all four passes until no further change */
static void rxOptimiseBuffer(char* buf)
{
    char prev[kMaxRegex];
    do {
        strcpy(prev, buf);
        rxCanonicaliseClass(buf);
        rxCollapseSpecials(buf);
        rxRemoveTrivialQuant(buf);
        rxDropTrivialGroup(buf);
    } while (strcmp(prev, buf) != 0);
}

/* Since our helpers call rxOptimiseBuffer on p->szBuf after each append,
   we only need a short wrapper here. */
static void rxOptimise(Regex_t* p)
{
    rxOptimiseBuffer(p->szBuf);
    p->nLen = (int)strlen(p->szBuf);
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

/* 2) Add a raw token string (\, ., [..], or entire group or preset). */
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
        strcpy(p->szBuf,
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)");
        break;

    case 2:
        strcpy(p->szBuf,
            "[A-Za-z0-9._%+-]+@"
            "[A-Za-z0-9.-]+\\.[A-Za-z]{2,}");
        break;

    case 3:
        strcpy(p->szBuf,
            "https?://"
            "[A-Za-z0-9._~:/?#@!$&'()*+,;=-]+");
        break;

    case 4:
        strcpy(p->szBuf, "[A-Fa-f0-9]{32}");
        break;

    case 5:
        strcpy(p->szBuf, "[A-Fa-f0-9]{64}");
        break;

    case 6:
        strcpy(p->szBuf, "[A-Za-z]:\\\\(?:[^\\\\/:*?\"<>|\\r\\n]+\\\\?)*");
        break;

    default:
        puts("[!] Unknown preset");
        return;
    }

    p->nLen = (int)strlen(p->szBuf);
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

/*--------------------------------------------------------------------------
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  ##########################################################################
  --------------------------------------------------------------------------*/

/* ==============================================================
   MAIN  ─ CYVA console
   ==============================================================*/
int main(void)
{
    loadTopicsFromFile();                   /* your topic loader */

    while (1)
    {
        puts("\n=== CYVA MAIN MENU ===");
        puts("1. Explore domain topics");
        puts("2. Domain overview");
        puts("3. Search topics by tag");
        puts("4. Launch Regex Generator");
        puts("0. Exit");
        int nChoice;
        if (!promptInt("Choice: ", 0, 4, &nChoice))
            continue;

        switch (nChoice)
        {
        case 0:
            return 0;                               /* exit program */

        case 1: {                                   /* explore */
            char szDom[256];
            if (promptText("Enter domain: ", szDom, sizeof szDom))
                browseDomainTopics(szDom, true);
            break;
        }

        case 2:                                     /* overview */
            showDomainOverview();
            break;

        case 3:                                     /* tag search */
            searchByTag();
            break;

        case 4:                                     /* regex tool */
            regexBuilder();
            break;

        default:
            puts("[!] Invalid option.");
        }
    }
}
