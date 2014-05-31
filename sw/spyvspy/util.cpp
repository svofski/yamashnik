#include <string.h>
#include <ctype.h>
#include "util.h"
#include "diags.h"

int Util::dosname(const char* fname, char* buf83) {
    if (fname == 0 || buf83 == 0) return 0;
    
    char* shrt = (shrt = strrchr(fname, '/')) ? (shrt + 1) : (char *)fname;

    char* dot = strchr(shrt, '.');
    int namelen = dot == 0 ? strlen(shrt) : dot - shrt;
    int extlen = strlen(shrt) - namelen - 1;
    if (namelen > 8 || extlen > 3) {
        return 0;
    }

    memset(buf83, ' ', 11);

    for (int i = 0; i < namelen; i++) {
        buf83[i] = (char) toupper(shrt[i]);
    }
    for (int i = 0; i < extlen; i++) {
        buf83[8+i] = (char) toupper(shrt[namelen + i + 1]);
    }

    return 1;
}

int Util::testSuite() {
    return
        test_dosname_1("a.b", "A       B  XXXX") &&
        test_dosname_1("a", "A          XXXX") &&
        test_dosname_1("/a","A          XXXX") &&
        test_dosname_1("../../a/b/c/a", "A          XXXX") &&
        test_dosname_1("../../a/b/c/autoexec.bat", "AUTOEXECBATXXXX");
}

int Util::test_dosname_1(const char* fileName, const char* expect) {
    char buf83[16];
    memset(buf83, 'X', sizeof(buf83));
    buf83[sizeof(buf83) - 1] = 0;   
    Util::dosname(fileName, buf83);

    if (strcmp(expect, buf83) != 0) {
        info("test_dosname_1: a.b->'%s', expected='%s'", buf83, expect);
        return 0;
    }

    return 1;
}
