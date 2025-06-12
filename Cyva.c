#include "Knowledge.h"
#include "RegEx.h"
#include "Logs.h"

/* ==============================================================
   MAIN  - CYVA console
   ==============================================================*/
int main(void)
{
    loadTopicsFromFile();

    while (1)
    {
        puts("\n=== CYVA MAIN MENU ===");
        puts("1. Explore domain topics");
        puts("2. Domain overview");
        puts("3. Search topics by tag");
        puts("4. Launch Regex Generator");
        puts("5. NXLog Logs Analyzer");
        puts("0. Exit");
        int nChoice;
        if (!promptInt("Choice: ", 0, 5, &nChoice))
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
        
        case 5:
            LogAnalysisMenu();                      /* NXLog Logs Analyzer */
            break;

        default:
            puts("[!] Invalid option.");
        }
    }
}
