#include "InlineParser.h"

#include "../core/StringUtils.h"

namespace mdmate {

namespace {

bool IsEscapedAt(std::wstring_view text, size_t index) {
    if (index == 0 || index >= text.size()) {
        return false;
    }

    size_t slashCount = 0;
    size_t i = index;
    while (i > 0) {
        --i;
        if (text[i] == L'\\') {
            ++slashCount;
        } else {
            break;
        }
    }
    return (slashCount % 2) == 1;
}

size_t FindUnescaped(std::wstring_view text, std::wstring_view needle, size_t from) {
    if (needle.empty()) {
        return std::wstring::npos;
    }

    size_t pos = from;
    while (true) {
        pos = text.find(needle, pos);
        if (pos == std::wstring::npos) {
            return pos;
        }
        if (!IsEscapedAt(text, pos)) {
            return pos;
        }
        ++pos;
    }
}

void AppendInlineRun(std::vector<InlineRun>& runs, std::wstring& plainBuffer, bool bold, bool italic, bool strike) {
    if (!plainBuffer.empty()) {
        InlineRun run;
        run.text = plainBuffer;
        run.bold = bold;
        run.italic = italic;
        run.strike = strike;
        runs.push_back(std::move(run));
        plainBuffer.clear();
    }
}

}

std::vector<InlineRun> ParseInlineRuns(std::wstring_view input, bool bold, bool italic, bool strike) {
    std::vector<InlineRun> runs;
    std::wstring plainBuffer;

    size_t i = 0;
    while (i < input.size()) {
        const wchar_t c = input[i];

        if (c == L'\\' && i + 1 < input.size()) {
            plainBuffer.push_back(input[i + 1]);
            i += 2;
            continue;
        }

        if (c == L'`') {
            size_t tickCount = 1;
            while (i + tickCount < input.size() && input[i + tickCount] == L'`') {
                ++tickCount;
            }

            const std::wstring ticks(tickCount, L'`');
            const size_t close = input.find(ticks, i + tickCount);
            if (close != std::wstring_view::npos) {
                AppendInlineRun(runs, plainBuffer, bold, italic, strike);
                std::wstring codeText(input.substr(i + tickCount, close - (i + tickCount)));
                if (codeText.size() >= 2 && codeText.front() == L' ' && codeText.back() == L' ') {
                    codeText = codeText.substr(1, codeText.size() - 2);
                }
                InlineRun run;
                run.text = codeText;
                run.code = true;
                runs.push_back(std::move(run));
                i = close + tickCount;
                continue;
            }
        }

        if (c == L'!' && i + 1 < input.size() && input[i + 1] == L'[') {
            const size_t closeBracket = FindUnescaped(input, L"]", i + 2);
            const size_t openParen =
                closeBracket == std::wstring::npos ? std::wstring::npos : FindUnescaped(input, L"(", closeBracket + 1);
            const size_t closeParen =
                openParen == std::wstring::npos ? std::wstring::npos : FindUnescaped(input, L")", openParen + 1);

            if (closeBracket != std::wstring::npos && openParen == closeBracket + 1 && closeParen != std::wstring::npos) {
                AppendInlineRun(runs, plainBuffer, bold, italic, strike);
                const std::wstring alt(input.substr(i + 2, closeBracket - (i + 2)));
                const std::wstring href(input.substr(openParen + 1, closeParen - openParen - 1));
                InlineRun run;
                run.text = L"[image: " + (alt.empty() ? href : alt) + L"]";
                run.italic = true;
                run.link = true;
                runs.push_back(std::move(run));
                i = closeParen + 1;
                continue;
            }
        }

        if (c == L'[') {
            const size_t closeBracket = FindUnescaped(input, L"]", i + 1);
            const size_t openParen =
                closeBracket == std::wstring::npos ? std::wstring::npos : FindUnescaped(input, L"(", closeBracket + 1);
            const size_t closeParen =
                openParen == std::wstring::npos ? std::wstring::npos : FindUnescaped(input, L")", openParen + 1);

            if (closeBracket != std::wstring::npos && openParen == closeBracket + 1 && closeParen != std::wstring::npos) {
                AppendInlineRun(runs, plainBuffer, bold, italic, strike);
                const std::wstring_view label = input.substr(i + 1, closeBracket - (i + 1));
                std::vector<InlineRun> inner =
                    label.empty() ? std::vector<InlineRun>{} : ParseInlineRuns(label, bold, italic, strike);
                if (inner.empty()) {
                    const std::wstring href(input.substr(openParen + 1, closeParen - openParen - 1));
                    InlineRun run;
                    run.text = href.empty() ? L"link" : href;
                    run.link = true;
                    runs.push_back(std::move(run));
                } else {
                    for (InlineRun& r : inner) {
                        r.link = true;
                        runs.push_back(std::move(r));
                    }
                }
                i = closeParen + 1;
                continue;
            }
        }

        if (c == L'<' && i + 1 < input.size()) {
            const size_t close = FindUnescaped(input, L">", i + 1);
            if (close != std::wstring::npos) {
                const std::wstring inner(input.substr(i + 1, close - i - 1));
                if (StartsWith(inner, L"http://") || StartsWith(inner, L"https://") || StartsWith(inner, L"mailto:")) {
                    AppendInlineRun(runs, plainBuffer, bold, italic, strike);
                    InlineRun run;
                    run.text = inner;
                    run.link = true;
                    runs.push_back(std::move(run));
                    i = close + 1;
                    continue;
                }
            }
        }

        std::wstring_view token;
        if (i + 2 < input.size() && input[i] == L'*' && input[i + 1] == L'*' && input[i + 2] == L'*') {
            token = L"***";
        } else if (i + 2 < input.size() && input[i] == L'_' && input[i + 1] == L'_' && input[i + 2] == L'_') {
            token = L"___";
        } else if (i + 1 < input.size() && input[i] == L'*' && input[i + 1] == L'*') {
            token = L"**";
        } else if (i + 1 < input.size() && input[i] == L'_' && input[i + 1] == L'_') {
            token = L"__";
        } else if (i + 1 < input.size() && input[i] == L'~' && input[i + 1] == L'~') {
            token = L"~~";
        } else if (input[i] == L'*') {
            token = L"*";
        } else if (input[i] == L'_') {
            token = L"_";
        }

        if (!token.empty()) {
            const size_t close = FindUnescaped(input, token, i + token.size());
            if (close != std::wstring::npos && close > i + token.size()) {
                AppendInlineRun(runs, plainBuffer, bold, italic, strike);

                bool nestedBold = bold;
                bool nestedItalic = italic;
                bool nestedStrike = strike;
                if (token == L"***" || token == L"___") {
                    nestedBold = true;
                    nestedItalic = true;
                } else if (token == L"**" || token == L"__") {
                    nestedBold = true;
                } else if (token == L"~~") {
                    nestedStrike = true;
                } else {
                    nestedItalic = true;
                }

                std::vector<InlineRun> inner = ParseInlineRuns(
                    input.substr(i + token.size(), close - (i + token.size())), nestedBold, nestedItalic, nestedStrike);
                for (InlineRun& r : inner) {
                    runs.push_back(std::move(r));
                }
                i = close + token.size();
                continue;
            }
        }

        plainBuffer.push_back(c);
        ++i;
    }

    AppendInlineRun(runs, plainBuffer, bold, italic, strike);
    return runs;
}

}
