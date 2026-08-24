#pragma once

#include <string>

namespace mdmate {

struct SessionPaths {
    std::wstring file;
    std::wstring folder;
};

// Writes the current file and folder paths to a tiny LocalAppData ini.
void SaveSession();

// Reads the last saved file and folder paths. Missing or unreadable values stay empty.
SessionPaths LoadSession();

}
