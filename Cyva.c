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

/* -------- globals -------- */
Topic aTopics[cMaxTopics];
int   nTopicCount = 0;
char  aszDomains[cMaxDomains][cStrLen];
int   nDomainCount = 0;

/* =========================================================================
   Safe integer input: fgets + strtol; re-prompts on bad entry.
   Returns 1 on success, 0 on EOF.
   ========================================================================= */
int promptInt(const char* msg, int* out) {
    char buf[32], * end;
    while (1) {
        printf("%s", msg);
        if (!fgets(buf, sizeof buf, stdin)) return 0;
        long v = strtol(buf, &end, 10);
        if (end == buf) { puts("Enter a number."); continue; }
        *out = (int)v;
        return 1;
    }
}

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

/*=================================================================
  readFullLine
  ------------
  Reads an entire line from fp, regardless of length.  A trailing
  '\n' (if any) is stripped.  On success:
      returns 1   and  *outLine points to heap memory
  On EOF or error:
      returns 0   and  *outLine is unchanged.
  Caller *must* free(*outLine) after use.
==================================================================*/
static int readFullLine(FILE* fp, char** outLine)
{
    /* caller supplies pointer which we only set on success */
    const size_t kChunk = 1024U;
    size_t       cap = kChunk;
    size_t       len = 0U;
    char* buf = (char*)malloc(cap);

    if (buf == NULL)
        return 0;                       /* malloc failed */

    while (1) {
        /* make sure there is always at least one byte left for '\0' */
        if (cap - len < kChunk / 4U) {
            size_t newCap = cap + kChunk;
            char* tmp = (char*)realloc(buf, newCap);
            if (tmp == NULL) {          /* realloc failed */
                free(buf);
                return 0;
            }
            buf = tmp;
            cap = newCap;
        }

        if (fgets(buf + len, (int)(cap - len), fp) == NULL) {
            /* EOF before any char read? -> fail */
            if (len == 0U) {
                free(buf);
                return 0;
            }
            /* else treat last partial line as valid */
            break;
        }

        len += strlen(buf + len);

        /* If we ended with '\n', we have the full line */
        if (len > 0U && buf[len - 1U] == '\n') {
            buf[len - 1U] = '\0';       /* strip newline */
            break;
        }
    }

    *outLine = buf;
    return 1;
}

/*=================================================================
  loadTopicsFromFile  –  NEW VERSION
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

/* ---------------------------------------------------------------------------
   showSuggestionsForTopic
   -----------------------
   Prints the “Suggestions” list for topic *p*.
   Uses ID → index conversion so the correct names are displayed.
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
   ID  →  array-index helper
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

            printf("\nSelect ID to explore ('x' = back to '%s'): ", domain);
            if (!fgets(buf, sizeof buf, stdin)) return;
            if (buf[0] == 'x' || buf[0] == 'X') break;

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

int main(void) {
    loadTopicsFromFile();

    while (1) {
        puts("\n=== MAIN MENU ===");
        puts("1. Explore domain");
        puts("2. View domain overview");
        puts("3. Exit");
        puts("4. Search by tag");
        int ch;
        if (!promptInt("Choice: ", &ch)) continue;
        if (ch == 0 || ch == 3) break;

        if (ch == 1) {
            char domain[cStrLen];
            printf("\nEnter domain to explore: ");
            if (!fgets(domain, sizeof domain, stdin)) continue;
            strtok(domain, "\r\n");
            browseDomainTopics(domain, true);
        }
        else if (ch == 2) {
            showDomainOverview();
        }
        else if (ch == 4) {
            searchByTag();
        }
    }
    return 0;
}
