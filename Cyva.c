#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------- tunables -------- */
#define cMaxTopics     100
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
int promptInt(const char* msg, int* out)
{
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

/* ---------------------------------------------------------------------------
   loadTopicsFromFile()
   Reads exactly 10 pipe-separated fields per line (preserving empties)
   and parses them into your Topic struct.
   ------------------------------------------------------------------------- */
void loadTopicsFromFile(void) {
    FILE* fp = fopen(cFileName, "r");
    if (!fp) {
        perror("Failed to open topics.txt");
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // trim newline
        line[strcspn(line, "\r\n")] = '\0';

        // split into exactly 10 fields, preserving empty ones
        char* fields[10];
        char* p = line;
        for (int i = 0; i < 9; i++) {
            fields[i] = p;
            char* sep = strchr(p, '|');
            if (sep) {
                *sep = '\0';
                p = sep + 1;
            }
            else {
                // missing fields: set rest to empty
                for (int j = i + 1; j < 10; j++)
                    fields[j] = "";
                p = NULL;
                break;
            }
        }
        fields[9] = p ? p : "";

        // now we have fields[0]..fields[9]
        Topic* T = &aTopics[nTopicCount];

        // ID, name, description, domain
        T->nId = atoi(fields[0]);
        strncpy(T->szName, fields[1], cStrLen);  T->szName[cStrLen - 1] = '\0';
        strncpy(T->szDesc, fields[2], cStrLen);  T->szDesc[cStrLen - 1] = '\0';
        strncpy(T->szDomain, fields[3], cStrLen);  T->szDomain[cStrLen - 1] = '\0';

        // tags
        T->nTagCount = atoi(fields[4]);
        if (T->nTagCount > cMaxTags) T->nTagCount = cMaxTags;
        int t = 0;
        char* tk = fields[5];
        while (tk && *tk && t < T->nTagCount) {
            char* comma = strchr(tk, ',');
            if (comma) *comma = '\0';
            strncpy(T->aszTags[t], tk, cStrLen);
            T->aszTags[t][cStrLen - 1] = '\0';
            t++;
            if (!comma) break;
            tk = comma + 1;
        }

        // subtopics
        T->nSubCount = atoi(fields[6]);
        if (T->nSubCount > cMaxSubtopics) T->nSubCount = cMaxSubtopics;
        int s = 0;
        char* sub = fields[7];
        while (sub && *sub && s < T->nSubCount) {
            char* comma = strchr(sub, ',');
            if (comma) *comma = '\0';
            T->anSubIds[s++] = atoi(sub);
            if (!comma) break;
            sub = comma + 1;
        }

        // related
        T->nRelCount = atoi(fields[8]);
        if (T->nRelCount > cMaxRelations) T->nRelCount = cMaxRelations;
        int r = 0;
        char* rel = fields[9];
        while (rel && *rel && r < T->nRelCount) {
            char* comma = strchr(rel, ',');
            if (comma) *comma = '\0';
            T->anRelIds[r++] = atoi(rel);
            if (!comma) break;
            rel = comma + 1;
        }

        // collect domain
        int exists = 0;
        for (int d = 0; d < nDomainCount; d++) {
            if (strcmp(aszDomains[d], T->szDomain) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists && nDomainCount < cMaxDomains) {
            strncpy(aszDomains[nDomainCount++], T->szDomain, cStrLen);
        }

        nTopicCount++;
    }

    fclose(fp);
}

/* ---------------------------------------------------------------------------
   showSuggestionsForTopic()
   Now returns void and prints only the related IDs you actually parsed.
   ------------------------------------------------------------------------- */
void showSuggestionsForTopic(int id) {
    Topic* p = &aTopics[id];
    printf("\n=== Suggestions ===\n");
    if (p->nRelCount == 0) {
        printf("  (none)\n");
        return;
    }
    for (int i = 0; i < p->nRelCount; i++) {
        int rid = p->anRelIds[i];
        if (rid >= 0 && rid < nTopicCount) {
            printf("  [%d] %s\n", aTopics[rid].nId, aTopics[rid].szName);
        }
    }
}

/* ===========================================================
   Recursive topic viewer (handles subtopics)
   =========================================================== */
void exploreTopic(int nId) {
    Topic* p = &aTopics[nId];
    printf("\n--- %s ---\n%s\nDomain: %s\nTags:", p->szName, p->szDesc, p->szDomain);
    for (int i = 0; i < p->nTagCount; i++)
        printf(" %s", p->aszTags[i]);
    printf("\n");

    // Subtopics
    if (p->nSubCount > 0) {
        printf("\nSubtopics:\n");
        for (int i = 0; i < p->nSubCount; i++) {
            int sid = p->anSubIds[i];
            if (sid >= 0 && sid < nTopicCount)
                printf("[%d] %s\n", sid, aTopics[sid].szName);
        }
    }
    else {
        printf("(no subtopics)\n");
    }

    // Related topics
    showSuggestionsForTopic(nId);
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
   helper: check if a topic ID belongs to this domain
========================================================== = */
static bool topicInDomain(int nId, const char* szDomain) {
    for (int i = 0; i < nTopicCount; i++) {
        if (aTopics[i].nId == nId
            && strcmp(aTopics[i].szDomain, szDomain) == 0)
            return true;
    }
    return false;
}

/* ===========================================================
   Domain → topic loop  (shows suggestions each drill)
   =========================================================== */
void browseDomainTopics(const char* szStartDomain, bool bAllowDomainSwitch) {
    char domain[cStrLen], buf[cStrLen];
    strncpy(domain, szStartDomain, cStrLen);
    domain[cStrLen - 1] = '\0';

    while (1) {
        system("cls");
        printDomainTopics(domain);

        printf("\nSelect ID to explore ('x' = main menu): ");
        if (!fgets(buf, sizeof buf, stdin)) return;
        if (buf[0] == 'x' || buf[0] == 'X') return;

        int current = atoi(buf);
        if (!topicInDomain(current, domain)) continue;

        // drill‐down loop
        while (1) {
            system("cls");
            exploreTopic(current);  // prints desc, subtopics (or "(no subtopics)"), suggestions

            printf("\nSelect ID to explore ('x' = back to '%s'): ", domain);
            if (!fgets(buf, sizeof buf, stdin)) return;
            if (buf[0] == 'x' || buf[0] == 'X') break;

            int next = atoi(buf);
            bool ok = false;

            // allow subtopics
            for (int i = 0; i < aTopics[current].nSubCount; i++) {
                if (aTopics[current].anSubIds[i] == next) {
                    ok = true; break;
                }
            }
            // allow suggestions
            for (int i = 0; i < aTopics[current].nRelCount; i++) {
                if (aTopics[current].anRelIds[i] == next) {
                    ok = true; break;
                }
            }
            if (!ok) continue;

            // switch domain only if allowed by mode 1
            if (bAllowDomainSwitch
                && strcmp(aTopics[next].szDomain, domain) != 0)
            {
                strncpy(domain, aTopics[next].szDomain, cStrLen);
                domain[cStrLen - 1] = '\0';
            }

            current = next;
        }
        // back to domain list
    }
}

/* ---------------------------------------------------------------------------
   Option 4: searchByTag
   - prompt for a tag
   - list matches (or “none”)
   - let user pick one, then drill immediately into that topic
   - on 'x', return to a fixed-domain browse of its home domain
   ------------------------------------------------------------------------- */
void searchByTag(void) {
    char tag[cStrLen], buf[cStrLen];
    int  results[cMaxTopics], rc = 0;

    // 1) Get tag from user
    printf("\nEnter tag to search: ");
    if (!fgets(tag, sizeof tag, stdin)) return;
    strtok(tag, "\r\n");

    // 2) Find all topics with that tag
    for (int i = 0; i < nTopicCount; i++) {
        for (int t = 0; t < aTopics[i].nTagCount; t++) {
            if (strcmp(tag, aTopics[i].aszTags[t]) == 0) {
                results[rc++] = aTopics[i].nId;
                break;
            }
        }
    }

    // 3) No matches?
    if (rc == 0) {
        printf("\nNo topics found with tag '%s'.\n", tag);
        return;
    }

    // 4) Show matches
    printf("\nTopics matching '%s':\n", tag);
    for (int i = 0; i < rc; i++) {
        Topic* T = &aTopics[results[i]];
        printf("  [%d] %s  (domain: %s)\n",
            T->nId, T->szName, T->szDomain);
    }

    // 5) Pick one
    printf("\nSelect ID to explore ('x' = main menu): ");
    if (!fgets(buf, sizeof buf, stdin)) return;
    if (buf[0] == 'x' || buf[0] == 'X') return;

    int startId = atoi(buf);
    bool ok = false;
    for (int i = 0; i < rc; i++) {
        if (results[i] == startId) { ok = true; break; }
    }
    if (!ok) return;

    // 6) Remember its home domain
    char home[cStrLen];
    strncpy(home, aTopics[startId].szDomain, cStrLen);
    home[cStrLen - 1] = '\0';

    // 7) Drill directly into startId
    int current = startId;
    while (1) {
        system("cls");
        exploreTopic(current);  // name/desc, subtopics or "(no subtopics)", suggestions

        printf("\nSelect ID to explore ('x' = back to domain '%s'): ", home);
        if (!fgets(buf, sizeof buf, stdin)) return;
        if (buf[0] == 'x' || buf[0] == 'X') break;

        int next = atoi(buf);
        bool valid = false;
        // allow its subtopics
        for (int i = 0; i < aTopics[current].nSubCount; i++) {
            if (aTopics[current].anSubIds[i] == next) {
                valid = true; break;
            }
        }
        // allow its suggestions
        for (int i = 0; i < aTopics[current].nRelCount; i++) {
            if (aTopics[current].anRelIds[i] == next) {
                valid = true; break;
            }
        }
        if (valid) {
            current = next;
        }
    }

    // 8) Finally, drop into a fixed-domain browse of home
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