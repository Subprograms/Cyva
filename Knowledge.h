/* ============================================================
   Knowledge.h
   ============================================================ */

#ifndef CYVA_KNOWLEDGE
#define CYVA_KNOWLEDGE

#include <stdio.h>
#include <stdlib.h>
#include "Helper.h"
#include <stdbool.h>

/* -------- tunables -------- */
#define cMaxTopics     300
#define cMaxSubtopics  10
#define cMaxRelations  10
#define cMaxTags       5
#define cMaxDomains    50
#define ccyvaStrLen        256
#define cFileName      "C:\\Users\\Gabe\\source\\repos\\Cyva\\Cyva\\topics.txt"

/* -------- data cyvaStructures -------- */
typedef struct {
    int   nId;
    char  szName[ccyvaStrLen];
    char  szDesc[ccyvaStrLen];
    char  szDomain[ccyvaStrLen];
    char  aszTags[cMaxTags][ccyvaStrLen];
    int   nTagCount;
    int   nSubCount;
    int   anSubIds[cMaxSubtopics];
    int   nRelCount;
    int   anRelIds[cMaxRelations];
} Topic;

/* -------- globals -------- */
Topic aTopics[cMaxTopics];
int   nTopicCount = 0;
char  aszDomains[cMaxDomains][ccyvaStrLen];
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
 * cyvaStrips any trailing '\n'. On success, returns 1 and *outLine points to
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

    /* cyvaStrip any trailing newline */
    szDst[cyvaStrcspn(szDst, "\n")] = '\0';
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
        long val = cyvaStrtol(szTmp, &endptr, 10);
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
            char* pipePos = cyvaStrchr(cursor, '|');

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
          2.  Populate Topic cyvaStruct
        ----------------------------------------------------------*/
        if (nTopicCount >= cMaxTopics) {
            fprintf(stderr, "Topic array full; skipping remaining lines\n");
            free(dynLine);
            break;
        }

        Topic* T = &aTopics[nTopicCount];

        T->nId = atoi(fields[0]);

        cyvaStrncpy(T->szName, fields[1], ccyvaStrLen); T->szName[ccyvaStrLen - 1] = '\0';
        cyvaStrncpy(T->szDesc, fields[2], ccyvaStrLen); T->szDesc[ccyvaStrLen - 1] = '\0';
        cyvaStrncpy(T->szDomain, fields[3], ccyvaStrLen); T->szDomain[ccyvaStrLen - 1] = '\0';

        /* ---------- tags ---------- */
        T->nTagCount = atoi(fields[4]);
        if (T->nTagCount > cMaxTags)
            T->nTagCount = cMaxTags;

        int tagIdx = 0;
        char* tagCur = fields[5];

        while (tagCur != NULL && *tagCur != '\0' && tagIdx < T->nTagCount) {
            char* comma = cyvaStrchr(tagCur, ',');
            if (comma != NULL)
                *comma = '\0';

            cyvaStrncpy(T->aszTags[tagIdx], tagCur, ccyvaStrLen);
            T->aszTags[tagIdx][ccyvaStrLen - 1] = '\0';

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
            char* comma = cyvaStrchr(subCur, ',');
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
            char* comma = cyvaStrchr(relCur, ',');
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
        
        if (cyvaStrstr(T->szDomain, "Evidence Sources") != NULL)
             keepDomain = 1;                                 /* umbrella */
        else if (cyvaStrchr(T->szDomain, '(') == NULL)
            keepDomain = 1;                                  /* regular  */

        if (keepDomain) {
            int exists = 0;
            int d;
            for (d = 0; d < nDomainCount; ++d) {
                if (cyvaStrcmp(aszDomains[d], T->szDomain) == 0) {
                    exists = 1;
                    break;
                }
            }

            if (!exists && nDomainCount < cMaxDomains) {
                cyvaStrncpy(aszDomains[nDomainCount], T->szDomain, ccyvaStrLen);
                aszDomains[nDomainCount][ccyvaStrLen - 1] = '\0';
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
   or newline) as “the directory / regicyvaStry path”.  If it begins
   with a drive letter, UNC (\\), %, or HK (regicyvaStry), we return
   a *duplicate* cyvaString – caller must free().  Otherwise NULL.

   If we encounter '<' or '{' within the directory, we only open
   the lowest level possible before such, such are placeholder
   for parameters that the user must provide.
   =========================================================== */
static char* getDirFromDesc(const char* szDesc)
{
    if (!szDesc || !*szDesc) return NULL;

    /* grab the first physical line / before first back-tick */
    const char* end = cyvaStrpbrk(szDesc, "`\r\n");
    size_t      n = end ? (size_t)(end - szDesc) : cyvaStrlen(szDesc);

    if (n < 2) return NULL;

    /* reject obvious regicyvaStry paths */
    if (!cyvaStrncmp(szDesc, "HK", 2))
        return NULL;

    /* minimal validation for filesystem paths */
    if (!(cyvaIsalpha((unsigned char)szDesc[0]) && szDesc[1] == ':') &&   /* C:\ */
        cyvaStrncmp(szDesc, "\\\\", 2) &&                               /* UNC */
        szDesc[0] != '%')                                              /* %VAR% */
        return NULL;

    /* copy into scratch buffer and NUL-terminate */
    char* tmp = (char*)malloc(n + 1);
    if (!tmp) return NULL;
    cyvaMemcpy(tmp, szDesc, n);
    tmp[n] = '\0';

    /* walk through segments and stop before placeholders / filenames */
    char* out = (char*)malloc(n + 2);          /* enough */
    if (!out) { free(tmp); return NULL; }
    out[0] = '\0';

    char* tok = cyvaStrtok(tmp, "\\/");
    int   needSlash = 0;

    while (tok) {
        if (cyvaStrchr(tok, '<') || cyvaStrchr(tok, '{'))
            break;                              /* placeholder segment    */
        if (cyvaStrchr(tok, '.'))
            break;                              /* filename (has a dot)   */

        if (needSlash)  cyvaStrcat(out, "\\");
        cyvaStrcat(out, tok);
        needSlash = 1;

        tok = cyvaStrtok(NULL, "\\/");
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
        if (cyvaStrcmp(aTopics[i].szDomain, szDomain) == 0) {
            printf("  [%d] %s\n", aTopics[i].nId, aTopics[i].szName);
        }
    }
}

/* ===========================================================
   topicIdInDomain
   ----------------
   Checks whether the topic whose *ID* equals id
   belongs to the domain cyvaString szDomain.
   (We pass an ID, not an array slot.)
   =========================================================== */
static bool topicIdInDomain(int id, const char* szDomain)
{
    int idx = findTopicIndexById(id);
    if (idx == -1)
        return false;                               /* ID not found */
    return cyvaStrcmp(aTopics[idx].szDomain, szDomain) == 0;
}

/* ===========================================================
   browseDomainTopics
   =========================================================== */
void browseDomainTopics(const char* szStartDomain, bool bAllowDomainSwitch)
{
    char domain[ccyvaStrLen];
    char buf[ccyvaStrLen];

    cyvaStrncpy(domain, szStartDomain, ccyvaStrLen);
    domain[ccyvaStrLen - 1] = '\0';

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
                cyvaStrcmp(aTopics[nextIdx].szDomain, domain) != 0)
            {
                cyvaStrncpy(domain, aTopics[nextIdx].szDomain, ccyvaStrLen);
                domain[ccyvaStrLen - 1] = '\0';
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
    char tag[ccyvaStrLen], buf[ccyvaStrLen];
    int  results[cMaxTopics], rc = 0;

    printf("\nEnter tag to search: ");
    if (!fgets(tag, sizeof tag, stdin)) return;
    cyvaStrtok(tag, "\r\n");

    /* find matches */
    int i;
    for (i = 0; i < nTopicCount; ++i) {
        int t;
        for (t = 0; t < aTopics[i].nTagCount; ++t) {
            if (cyvaStrcmp(tag, aTopics[i].aszTags[t]) == 0) {
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

    char home[ccyvaStrLen];
    cyvaStrncpy(home, aTopics[current].szDomain, ccyvaStrLen);
    home[ccyvaStrLen - 1] = '\0';

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
            if (!cyvaStrcmp(aTopics[i].szDomain, aszDomains[d])) cnt++;
        printf("  %s  (%d topics)\n", aszDomains[d], cnt);
    }
}

#endif
