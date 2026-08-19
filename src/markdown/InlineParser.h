#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mdmate {

struct InlineRun {
    std::wstring text;
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool code = false;
    bool link = false;
};

// Converts inline Markdown syntax into styled text runs.
std::vector<InlineRun> ParseInlineRuns(std::wstring_view input, bool bold, bool italic, bool strike);

}
