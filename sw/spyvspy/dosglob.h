#pragma once

#include <dirent.h>
#include "spydata.h"

// a simple DOS-style globbor

class Globor {
    char m_pattern[11];
    DIR* m_dir;
    char* m_path;

public:
    Globor(const char* path) {
        m_path = new char[strlen(path) + 1];
        strcpy(m_path, path);
    }

    void SetPattern(const char* pattern) {
        memcpy(m_pattern, pattern, sizeof(m_pattern));
    }

    const char* SearchFirst(const char* pattern) {
        memcpy(m_pattern, pattern, sizeof(m_pattern));
        m_dir = opendir(m_path);

        if (m_dir == 0) {
            info("SearchFirst: opendir fail\n");
            return 0;
        }

        return SearchNext();
    }

    char* SearchNext() {
        if (m_dir == 0) 
            return 0;

        struct dirent* entry;

        while ((entry = readdir(m_dir)) != 0) {
            if (match(entry->d_name))
                return entry->d_name;
        }

        return 0;
    }

    int match(const char* fname) const 
    {
        char dosname[11];
        int i = -1;

        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
            return 0;
        
        if (Util::dosname(fname, dosname)) {
          //info("["); for (int q = 0; q < 11; q++) info("%c", m_pattern[q]); info("]");
          for (i = 0; 
                i < sizeof(dosname) &&
                (m_pattern[i] == dosname[i] || m_pattern[i] == '?'); 
                i++);
        }

        return i == sizeof(dosname);
    }
};