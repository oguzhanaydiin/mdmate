#pragma once

#include <string>
#include <vector>

namespace mdmate {

struct SessionPaths {
    std::wstring folder;
    std::vector<std::wstring> files;
    int active = 0;
};

// Writes the current file and folder paths to a tiny LocalAppData ini.
void SaveSession();

// Reads the last saved file and folder paths. Missing or unreadable values stay empty.
SessionPaths LoadSession();

}
