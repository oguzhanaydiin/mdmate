#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"MDMateWindowClass";
constexpr wchar_t kAppTitle[] = L"MDMate";

constexpr int IDC_EDITOR = 101;
constexpr int IDC_PREVIEW = 102;
constexpr int IDC_STATUS = 103;

constexpr UINT_PTR kPreviewTimerId = 1;
constexpr UINT kPreviewDelayMs = 120;

constexpr int IDM_FILE_NEW = 40001;
constexpr int IDM_FILE_OPEN = 40002;
constexpr int IDM_FILE_SAVE = 40003;
constexpr int IDM_FILE_SAVE_AS = 40004;
constexpr int IDM_FILE_EXIT = 40005;
constexpr int IDM_VIEW_TOGGLE_PREVIEW = 40101;
constexpr int IDM_VIEW_FULLSCREEN = 40102;
constexpr int IDM_HELP_ABOUT = 40201;

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_editor = nullptr;
HWND g_preview = nullptr;
HWND g_status = nullptr;
HFONT g_editorFont = nullptr;
HFONT g_previewFont = nullptr;
HACCEL g_accelerators = nullptr;

std::wstring g_currentFilePath;
bool g_isDirty = false;
bool g_showPreview = true;
bool g_suppressEditorChange = false;
bool g_isFullscreen = false;
WINDOWPLACEMENT g_windowPlacement{sizeof(WINDOWPLACEMENT)};
DWORD g_windowStyle = 0;
DWORD g_windowExStyle = 0;

std::wstring TrimLeft(std::wstring_view text) {
    size_t i = 0;
    while (i < text.size() && std::iswspace(text[i])) {
        ++i;
    }
    return std::wstring(text.substr(i));
}

std::wstring TrimRight(std::wstring_view text) {
    size_t i = text.size();
    while (i > 0 && std::iswspace(text[i - 1])) {
        --i;
    }
    return std::wstring(text.substr(0, i));
}

std::wstring Trim(std::wstring_view text) {
    return TrimRight(TrimLeft(text));
}

bool StartsWith(std::wstring_view text, std::wstring_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::wstring ToUpper(std::wstring_view text) {
    std::wstring upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return upper;
}

std::wstring GetFileNameFromPath(std::wstring_view path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring_view::npos) {
        return std::wstring(path);
    }
    return std::wstring(path.substr(slash + 1));
}

std::wstring ReadControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length), L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    return text;
}

void SetControlText(HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

std::wstring DecodeBytesUtf8OrAnsi(const std::vector<char>& bytes, size_t offset) {
    const char* data = bytes.data() + offset;
    const int byteCount = static_cast<int>(bytes.size() - offset);

    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, byteCount, nullptr, 0);
    if (required > 0) {
        std::wstring out(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, byteCount, out.data(), required);
        return out;
    }

    required = MultiByteToWideChar(CP_ACP, 0, data, byteCount, nullptr, 0);
    std::wstring out(static_cast<size_t>(required), L'\0');
    if (required > 0) {
        MultiByteToWideChar(CP_ACP, 0, data, byteCount, out.data(), required);
    }
    return out;
}

bool LoadTextFile(const std::wstring& path, std::wstring& outText) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        outText.clear();
        return true;
    }

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        outText = DecodeBytesUtf8OrAnsi(bytes, 3);
        return true;
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t wcharCount = (bytes.size() - 2) / sizeof(wchar_t);
        outText.assign(wcharCount, L'\0');
        memcpy(outText.data(), bytes.data() + 2, wcharCount * sizeof(wchar_t));
        return true;
    }

    outText = DecodeBytesUtf8OrAnsi(bytes, 0);
    return true;
}

std::vector<char> EncodeUtf8(const std::wstring& text) {
    const int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0,
                                             nullptr, nullptr);
    std::vector<char> bytes(static_cast<size_t>(required));
    if (required > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(), required, nullptr,
                            nullptr);
    }
    return bytes;
}

bool SaveTextFileUtf8(const std::wstring& path, const std::wstring& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const std::vector<char> bytes = EncodeUtf8(text);
    if (!bytes.empty()) {
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(file);
}

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

struct InlineRun {
    std::wstring text;
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool code = false;
    bool link = false;
};

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

// Parses inline markdown into styled text runs, inheriting outer bold/italic/strike state.
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

bool IsHorizontalRule(std::wstring_view line) {
    const std::wstring trimmed = Trim(line);
    if (trimmed.size() < 3) {
        return false;
    }

    const wchar_t marker = trimmed[0];
    if (marker != L'-' && marker != L'*' && marker != L'_') {
        return false;
    }

    for (wchar_t c : trimmed) {
        if (c != marker && !std::iswspace(c)) {
            return false;
        }
    }
    return true;
}

bool IsSetextUnderline(std::wstring_view line, wchar_t* markerOut = nullptr) {
    const std::wstring trimmed = Trim(line);
    if (trimmed.empty()) {
        return false;
    }

    const wchar_t marker = trimmed[0];
    if (marker != L'=' && marker != L'-') {
        return false;
    }

    for (wchar_t c : trimmed) {
        if (c != marker && !std::iswspace(c)) {
            return false;
        }
    }

    if (markerOut != nullptr) {
        *markerOut = marker;
    }
    return true;
}

bool TryParseAtxHeading(std::wstring_view trimmedLeft, size_t* headingLevelOut, std::wstring& contentOut) {
    size_t headingLevel = 0;
    while (headingLevel < trimmedLeft.size() && trimmedLeft[headingLevel] == L'#') {
        ++headingLevel;
    }

    if (headingLevel == 0 || headingLevel > 6) {
        return false;
    }

    size_t contentStart = headingLevel;
    while (contentStart < trimmedLeft.size() && std::iswspace(trimmedLeft[contentStart])) {
        ++contentStart;
    }

    std::wstring content = Trim(trimmedLeft.substr(contentStart));

    size_t end = content.size();
    while (end > 0 && content[end - 1] == L'#') {
        --end;
    }
    if (end < content.size()) {
        size_t ws = end;
        while (ws > 0 && std::iswspace(content[ws - 1])) {
            --ws;
        }
        content = Trim(content.substr(0, ws));
    }

    contentOut = content;
    if (headingLevelOut != nullptr) {
        *headingLevelOut = headingLevel;
    }
    return true;
}

bool TryParseUnorderedListItem(std::wstring_view trimmedLeft, std::wstring& restOut) {
    if (trimmedLeft.size() < 2) {
        return false;
    }
    const wchar_t marker = trimmedLeft[0];
    if ((marker == L'-' || marker == L'*' || marker == L'+') && trimmedLeft[1] == L' ') {
        restOut = std::wstring(trimmedLeft.substr(2));
        return true;
    }
    return false;
}

bool TryParseOrderedListItem(std::wstring_view trimmedLeft, std::wstring& markerOut, std::wstring& restOut) {
    size_t i = 0;
    while (i < trimmedLeft.size() && std::iswdigit(trimmedLeft[i])) {
        ++i;
    }
    if (i == 0 || i + 1 >= trimmedLeft.size()) {
        return false;
    }
    if (trimmedLeft[i] != L'.' && trimmedLeft[i] != L')') {
        return false;
    }
    if (trimmedLeft[i + 1] != L' ') {
        return false;
    }

    markerOut = std::wstring(trimmedLeft.substr(0, i + 1)) + L" ";
    restOut = std::wstring(trimmedLeft.substr(i + 2));
    return true;
}

bool TryParseBlockquote(std::wstring_view trimmedLeft, std::wstring& restOut) {
    if (trimmedLeft.empty() || trimmedLeft[0] != L'>') {
        return false;
    }

    size_t i = 0;
    while (i < trimmedLeft.size() && trimmedLeft[i] == L'>') {
        ++i;
    }
    while (i < trimmedLeft.size() && trimmedLeft[i] == L' ') {
        ++i;
    }

    restOut = std::wstring(trimmedLeft.substr(i));
    return true;
}

bool TryParseFence(std::wstring_view trimmedLeft, wchar_t& fenceChar, size_t& fenceLen) {
    if (trimmedLeft.empty()) {
        return false;
    }

    const wchar_t marker = trimmedLeft[0];
    if (marker != L'`' && marker != L'~') {
        return false;
    }

    size_t count = 0;
    while (count < trimmedLeft.size() && trimmedLeft[count] == marker) {
        ++count;
    }

    if (count < 3) {
        return false;
    }

    fenceChar = marker;
    fenceLen = count;
    return true;
}

int ComputeIndentLevel(std::wstring_view rawLine) {
    int spaces = 0;
    for (wchar_t c : rawLine) {
        if (c == L' ') {
            ++spaces;
        } else if (c == L'\t') {
            spaces += 4;
        } else {
            break;
        }
    }
    return spaces / 2;
}

enum class PreviewBlockType {
    Paragraph,
    Heading,
    Quote,
    Code,
    List,
    Rule,
};

struct PreviewSpan {
    LONG start = 0;
    LONG length = 0;
    PreviewBlockType type = PreviewBlockType::Paragraph;
    int headingLevel = 0;
};

struct PreviewInlineSpan {
    LONG start = 0;
    LONG length = 0;
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool code = false;
    bool link = false;
};

struct PreviewDocument {
    std::wstring text;
    std::vector<PreviewSpan> blockSpans;
    std::vector<PreviewInlineSpan> inlineSpans;
};

void AppendPreviewRuns(PreviewDocument& doc, const std::vector<InlineRun>& runs, PreviewBlockType type,
                       int headingLevel = 0) {
    const LONG lineStart = static_cast<LONG>(doc.text.size());

    for (const InlineRun& run : runs) {
        if (run.text.empty()) {
            continue;
        }

        const LONG start = static_cast<LONG>(doc.text.size());
        doc.text += run.text;
        const LONG length = static_cast<LONG>(run.text.size());

        PreviewInlineSpan inlineSpan;
        inlineSpan.start = start;
        inlineSpan.length = length;
        inlineSpan.bold = run.bold;
        inlineSpan.italic = run.italic;
        inlineSpan.strike = run.strike;
        inlineSpan.code = run.code;
        inlineSpan.link = run.link;
        doc.inlineSpans.push_back(inlineSpan);
    }

    const LONG lineLength = static_cast<LONG>(doc.text.size()) - lineStart;
    doc.text += L"\n";

    if (lineLength > 0) {
        PreviewSpan blockSpan;
        blockSpan.start = lineStart;
        blockSpan.length = lineLength;
        blockSpan.type = type;
        blockSpan.headingLevel = headingLevel;
        doc.blockSpans.push_back(blockSpan);
    }
}

void AppendPreviewLiteral(PreviewDocument& doc, const std::wstring& text, PreviewBlockType type) {
    std::vector<InlineRun> runs;
    if (!text.empty()) {
        InlineRun run;
        run.text = text;
        run.code = (type == PreviewBlockType::Code);
        runs.push_back(std::move(run));
    }
    AppendPreviewRuns(doc, runs, type);
}

PreviewDocument RenderMarkdownPreview(const std::wstring& markdown) {
    std::wstringstream input(markdown);
    std::wstring rawLine;
    std::vector<std::wstring> lines;
    while (std::getline(input, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == L'\r') {
            rawLine.pop_back();
        }
        lines.push_back(rawLine);
    }

    PreviewDocument doc;
    bool inCodeBlock = false;
    wchar_t fenceChar = 0;
    size_t fenceLen = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::wstring& line = lines[i];
        const std::wstring trimmedLeft = TrimLeft(line);

        wchar_t thisFenceChar = 0;
        size_t thisFenceLen = 0;
        if (TryParseFence(trimmedLeft, thisFenceChar, thisFenceLen)) {
            if (!inCodeBlock) {
                inCodeBlock = true;
                fenceChar = thisFenceChar;
                fenceLen = thisFenceLen;
                continue;
            }
            if (thisFenceChar == fenceChar && thisFenceLen >= fenceLen) {
                inCodeBlock = false;
                continue;
            }
        }

        if (inCodeBlock) {
            AppendPreviewLiteral(doc, line, PreviewBlockType::Code);
            continue;
        }

        if (Trim(line).empty()) {
            doc.text += L"\n";
            continue;
        }

        if (IsHorizontalRule(line)) {
            AppendPreviewLiteral(doc, std::wstring(40, static_cast<wchar_t>(0x2500)), PreviewBlockType::Rule);
            continue;
        }

        if (i + 1 < lines.size()) {
            wchar_t setextMarker = 0;
            if (IsSetextUnderline(lines[i + 1], &setextMarker)) {
                const int level = (setextMarker == L'=') ? 1 : 2;
                const std::vector<InlineRun> runs = ParseInlineRuns(Trim(line), true, false, false);
                AppendPreviewRuns(doc, runs, PreviewBlockType::Heading, level);
                ++i;
                continue;
            }
        }

        size_t headingLevel = 0;
        std::wstring headingContent;
        if (TryParseAtxHeading(trimmedLeft, &headingLevel, headingContent)) {
            const std::vector<InlineRun> runs = ParseInlineRuns(headingContent, true, false, false);
            AppendPreviewRuns(doc, runs, PreviewBlockType::Heading, static_cast<int>(headingLevel));
            continue;
        }

        std::wstring quoteContent;
        if (TryParseBlockquote(trimmedLeft, quoteContent)) {
            std::vector<InlineRun> runs;
            InlineRun marker;
            marker.text = std::wstring(1, static_cast<wchar_t>(0x2503)) + L" ";
            runs.push_back(marker);
            std::vector<InlineRun> inner = ParseInlineRuns(quoteContent, false, true, false);
            for (InlineRun& r : inner) {
                runs.push_back(std::move(r));
            }
            AppendPreviewRuns(doc, runs, PreviewBlockType::Quote);
            continue;
        }

        std::wstring unorderedRest;
        if (TryParseUnorderedListItem(trimmedLeft, unorderedRest)) {
            const int indentLevel = ComputeIndentLevel(line);
            std::vector<InlineRun> runs;
            InlineRun marker;
            marker.text = std::wstring(static_cast<size_t>(indentLevel) * 2, L' ') +
                          std::wstring(1, static_cast<wchar_t>(0x2022)) + L" ";
            runs.push_back(marker);
            std::vector<InlineRun> inner = ParseInlineRuns(unorderedRest, false, false, false);
            for (InlineRun& r : inner) {
                runs.push_back(std::move(r));
            }
            AppendPreviewRuns(doc, runs, PreviewBlockType::List);
            continue;
        }

        std::wstring orderedMarker;
        std::wstring orderedRest;
        if (TryParseOrderedListItem(trimmedLeft, orderedMarker, orderedRest)) {
            const int indentLevel = ComputeIndentLevel(line);
            std::vector<InlineRun> runs;
            InlineRun marker;
            marker.text = std::wstring(static_cast<size_t>(indentLevel) * 2, L' ') + orderedMarker;
            runs.push_back(marker);
            std::vector<InlineRun> inner = ParseInlineRuns(orderedRest, false, false, false);
            for (InlineRun& r : inner) {
                runs.push_back(std::move(r));
            }
            AppendPreviewRuns(doc, runs, PreviewBlockType::List);
            continue;
        }

        const std::vector<InlineRun> runs = ParseInlineRuns(line, false, false, false);
        AppendPreviewRuns(doc, runs, PreviewBlockType::Paragraph);
    }

    return doc;
}

CHARFORMAT2W BuildCharFormat(const wchar_t* faceName, LONG pointSizeTwips, COLORREF color, bool bold = false) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD;
    format.yHeight = pointSizeTwips;
    format.crTextColor = color;
    format.dwEffects = bold ? CFE_BOLD : 0;
    wcsncpy_s(format.szFaceName, faceName, _TRUNCATE);
    return format;
}

CHARFORMAT2W ResolveBlockFormat(const PreviewSpan& span) {
    switch (span.type) {
        case PreviewBlockType::Heading:
            switch (span.headingLevel) {
                case 1:
                    return BuildCharFormat(L"Segoe UI", 400, RGB(15, 15, 15), true);
                case 2:
                    return BuildCharFormat(L"Segoe UI", 340, RGB(20, 20, 20), true);
                case 3:
                    return BuildCharFormat(L"Segoe UI", 300, RGB(28, 28, 28), true);
                case 4:
                    return BuildCharFormat(L"Segoe UI", 260, RGB(35, 35, 35), true);
                case 5:
                    return BuildCharFormat(L"Segoe UI", 240, RGB(45, 45, 45), true);
                default:
                    return BuildCharFormat(L"Segoe UI", 220, RGB(55, 55, 55), true);
            }
        case PreviewBlockType::Quote:
            return BuildCharFormat(L"Segoe UI", 220, RGB(95, 95, 95));
        case PreviewBlockType::Code:
            return BuildCharFormat(L"Consolas", 210, RGB(120, 40, 100));
        case PreviewBlockType::List:
            return BuildCharFormat(L"Segoe UI", 220, RGB(30, 30, 30));
        case PreviewBlockType::Rule:
            return BuildCharFormat(L"Segoe UI", 220, RGB(150, 150, 150));
        case PreviewBlockType::Paragraph:
        default:
            return BuildCharFormat(L"Segoe UI", 220, RGB(30, 30, 30));
    }
}

CHARFORMAT2W ResolveInlineOverlay(const PreviewInlineSpan& span) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT;
    format.dwEffects =
        (span.bold ? CFE_BOLD : 0) | (span.italic ? CFE_ITALIC : 0) | (span.strike ? CFE_STRIKEOUT : 0);

    if (span.code) {
        format.dwMask |= CFM_FACE | CFM_COLOR;
        format.crTextColor = RGB(170, 40, 110);
        wcsncpy_s(format.szFaceName, L"Consolas", _TRUNCATE);
    } else if (span.link) {
        format.dwMask |= CFM_COLOR | CFM_UNDERLINE;
        format.dwEffects |= CFE_UNDERLINE;
        format.crTextColor = RGB(20, 90, 200);
    }

    return format;
}

void ApplyPreviewStyles(const PreviewDocument& doc) {
    SendMessageW(g_preview, WM_SETREDRAW, FALSE, 0);

    CHARRANGE previousSelection{};
    SendMessageW(g_preview, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&previousSelection));

    SetControlText(g_preview, doc.text);

    CHARFORMAT2W body = BuildCharFormat(L"Segoe UI", 220, RGB(30, 30, 30));
    CHARRANGE allRange{};
    allRange.cpMin = 0;
    allRange.cpMax = -1;
    SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&allRange));
    SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&body));

    for (const PreviewSpan& span : doc.blockSpans) {
        CHARRANGE range{};
        range.cpMin = span.start;
        range.cpMax = span.start + span.length;
        SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W style = ResolveBlockFormat(span);
        SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&style));
    }

    for (const PreviewInlineSpan& span : doc.inlineSpans) {
        CHARRANGE range{};
        range.cpMin = span.start;
        range.cpMax = span.start + span.length;
        SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W overlay = ResolveInlineOverlay(span);
        SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&overlay));
    }

    SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&previousSelection));
    SendMessageW(g_preview, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_preview, nullptr, TRUE);
}

int CountWords(std::wstring_view text) {
    int words = 0;
    bool inWord = false;

    for (const wchar_t c : text) {
        const bool isWordChar = std::iswalnum(c) || c == L'\'';
        if (isWordChar) {
            if (!inWord) {
                ++words;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }

    return words;
}

void UpdateWindowTitle() {
    std::wstring name = g_currentFilePath.empty() ? L"Untitled.md" : GetFileNameFromPath(g_currentFilePath);
    if (g_isDirty) {
        name += L" *";
    }

    const std::wstring title = name + L" - " + kAppTitle;
    SetWindowTextW(g_mainWindow, title.c_str());
}

void UpdateStatusText() {
    const std::wstring text = ReadControlText(g_editor);
    const int words = CountWords(text);
    const int chars = static_cast<int>(text.size());
    const int lines = static_cast<int>(SendMessageW(g_editor, EM_GETLINECOUNT, 0, 0));

    wchar_t buffer[256]{};
    swprintf_s(buffer, L"Words: %d   Chars: %d   Lines: %d", words, chars, lines);
    SendMessageW(g_status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(buffer));
}

void RefreshPreview() {
    if (!g_showPreview || g_preview == nullptr) {
        return;
    }

    const std::wstring markdown = ReadControlText(g_editor);
    const PreviewDocument preview = RenderMarkdownPreview(markdown);
    ApplyPreviewStyles(preview);
}

void QueuePreviewRefresh(HWND window) {
    KillTimer(window, kPreviewTimerId);
    SetTimer(window, kPreviewTimerId, kPreviewDelayMs, nullptr);
}

void OnEditorChanged(HWND window) {
    if (!g_suppressEditorChange) {
        g_isDirty = true;
        UpdateWindowTitle();
        QueuePreviewRefresh(window);
    }
}

void LayoutControls(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    SendMessageW(g_status, WM_SIZE, 0, 0);

    RECT statusRect{};
    GetWindowRect(g_status, &statusRect);
    const int statusHeight = statusRect.bottom - statusRect.top;

    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    const int contentHeight = std::max(0, height - statusHeight);

    if (g_showPreview) {
        const int editorWidth = (width * 58) / 100;
        MoveWindow(g_editor, 0, 0, editorWidth, contentHeight, TRUE);
        MoveWindow(g_preview, editorWidth + 1, 0, width - editorWidth - 1, contentHeight, TRUE);
        ShowWindow(g_preview, SW_SHOW);
    } else {
        MoveWindow(g_editor, 0, 0, width, contentHeight, TRUE);
        ShowWindow(g_preview, SW_HIDE);
    }

    MoveWindow(g_status, 0, contentHeight, width, statusHeight, TRUE);
}

std::wstring ShowOpenDialog(HWND owner) {
    wchar_t fileBuffer[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    ofn.lpstrDefExt = L"md";

    if (GetOpenFileNameW(&ofn)) {
        return fileBuffer;
    }

    return L"";
}

std::wstring ShowSaveDialog(HWND owner, const std::wstring& currentPath) {
    wchar_t fileBuffer[MAX_PATH]{};
    if (!currentPath.empty()) {
        wcsncpy_s(fileBuffer, currentPath.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"md";

    if (GetSaveFileNameW(&ofn)) {
        return fileBuffer;
    }

    return L"";
}

bool SaveDocument(HWND window, bool saveAs) {
    std::wstring targetPath = g_currentFilePath;
    if (saveAs || targetPath.empty()) {
        targetPath = ShowSaveDialog(window, g_currentFilePath);
        if (targetPath.empty()) {
            return false;
        }
    }

    const std::wstring content = ReadControlText(g_editor);
    if (!SaveTextFileUtf8(targetPath, content)) {
        MessageBoxW(window, L"Could not save this file.", kAppTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    g_currentFilePath = targetPath;
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);
    UpdateWindowTitle();
    UpdateStatusText();
    return true;
}

bool MaybeSavePendingChanges(HWND window) {
    if (!g_isDirty) {
        return true;
    }

    const int result = MessageBoxW(window, L"You have unsaved changes. Save now?", kAppTitle,
                                   MB_ICONQUESTION | MB_YESNOCANCEL);
    if (result == IDCANCEL) {
        return false;
    }
    if (result == IDYES) {
        return SaveDocument(window, false);
    }
    return true;
}

bool LoadDocumentIntoEditor(HWND window, const std::wstring& path) {
    std::wstring content;
    if (!LoadTextFile(path, content)) {
        MessageBoxW(window, L"Could not open this file.", kAppTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    g_suppressEditorChange = true;
    SetControlText(g_editor, content);
    g_suppressEditorChange = false;

    g_currentFilePath = path;
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
    return true;
}

void NewDocument(HWND window) {
    if (!MaybeSavePendingChanges(window)) {
        return;
    }

    g_suppressEditorChange = true;
    SetControlText(g_editor, L"");
    g_suppressEditorChange = false;

    g_currentFilePath.clear();
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
}

void OpenDocument(HWND window) {
    if (!MaybeSavePendingChanges(window)) {
        return;
    }

    const std::wstring path = ShowOpenDialog(window);
    if (!path.empty()) {
        LoadDocumentIntoEditor(window, path);
    }
}

void ToggleFullscreen(HWND window) {
    if (!g_isFullscreen) {
        g_windowStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
        g_windowExStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
        GetWindowPlacement(window, &g_windowPlacement);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        SetWindowLongPtrW(window, GWL_STYLE, g_windowStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(window, GWL_STYLE, g_windowStyle);
        SetWindowLongPtrW(window, GWL_EXSTYLE, g_windowExStyle);
        SetWindowPlacement(window, &g_windowPlacement);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    g_isFullscreen = !g_isFullscreen;
}

HMENU BuildMainMenu() {
    HMENU mainMenu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU viewMenu = CreatePopupMenu();
    HMENU helpMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");

    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_TOGGLE_PREVIEW, L"Toggle &Preview\tF6");
    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"&Fullscreen\tF11");

    AppendMenuW(helpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");

    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");
    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");

    return mainMenu;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            LoadLibraryW(L"Msftedit.dll");

            INITCOMMONCONTROLSEX commonControls{};
            commonControls.dwSize = sizeof(commonControls);
            commonControls.dwICC = ICC_BAR_CLASSES;
            InitCommonControlsEx(&commonControls);

            g_editor = CreateWindowExW(
                WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDITOR)), g_instance,
                nullptr);

            g_preview = CreateWindowExW(
                WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL | ES_READONLY,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW)), g_instance,
                nullptr);

            g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), g_instance,
                                       nullptr);

            g_editorFont = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       FIXED_PITCH | FF_MODERN, L"Consolas");

            g_previewFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            SendMessageW(g_editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_editorFont), TRUE);
            SendMessageW(g_preview, WM_SETFONT, reinterpret_cast<WPARAM>(g_previewFont), TRUE);

            SendMessageW(g_editor, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_preview, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_editor, EM_SETEVENTMASK, 0, ENM_CHANGE);

            DragAcceptFiles(window, TRUE);

            SetMenu(window, BuildMainMenu());
            UpdateWindowTitle();
            UpdateStatusText();
            RefreshPreview();
            return 0;
        }

        case WM_SIZE:
            LayoutControls(window);
            return 0;

        case WM_DROPFILES: {
            if (!MaybeSavePendingChanges(window)) {
                DragFinish(reinterpret_cast<HDROP>(wParam));
                return 0;
            }

            wchar_t path[MAX_PATH]{};
            DragQueryFileW(reinterpret_cast<HDROP>(wParam), 0, path, MAX_PATH);
            DragFinish(reinterpret_cast<HDROP>(wParam));
            LoadDocumentIntoEditor(window, path);
            return 0;
        }

        case WM_TIMER:
            if (wParam == kPreviewTimerId) {
                KillTimer(window, kPreviewTimerId);
                RefreshPreview();
                UpdateStatusText();
                return 0;
            }
            break;

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int notification = HIWORD(wParam);

            if (id == IDC_EDITOR && notification == EN_CHANGE) {
                OnEditorChanged(window);
                return 0;
            }

            switch (id) {
                case IDM_FILE_NEW:
                    NewDocument(window);
                    return 0;
                case IDM_FILE_OPEN:
                    OpenDocument(window);
                    return 0;
                case IDM_FILE_SAVE:
                    SaveDocument(window, false);
                    return 0;
                case IDM_FILE_SAVE_AS:
                    SaveDocument(window, true);
                    return 0;
                case IDM_FILE_EXIT:
                    SendMessageW(window, WM_CLOSE, 0, 0);
                    return 0;
                case IDM_VIEW_TOGGLE_PREVIEW:
                    g_showPreview = !g_showPreview;
                    LayoutControls(window);
                    if (g_showPreview) {
                        RefreshPreview();
                    }
                    return 0;
                case IDM_VIEW_FULLSCREEN:
                    ToggleFullscreen(window);
                    return 0;
                case IDM_HELP_ABOUT:
                    MessageBoxW(window,
                                L"MDMate\nA native, ultra-lightweight Markdown editor for Windows.\n\n"
                                L"Shortcuts:\n"
                                L"Ctrl+N New\nCtrl+O Open\nCtrl+S Save\nCtrl+Shift+S Save As\n"
                                L"F6 Toggle Preview\nF11 Fullscreen",
                                kAppTitle, MB_OK | MB_ICONINFORMATION);
                    return 0;
                default:
                    break;
            }
            break;
        }

        case WM_NOTIFY: {
            const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lParam);
            if (hdr != nullptr && hdr->idFrom == IDC_EDITOR && hdr->code == EN_CHANGE) {
                OnEditorChanged(window);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            if (!MaybeSavePendingChanges(window)) {
                return 0;
            }
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, kPreviewTimerId);
            if (g_editorFont != nullptr) {
                DeleteObject(g_editorFont);
                g_editorFont = nullptr;
            }
            if (g_previewFont != nullptr) {
                DeleteObject(g_previewFont);
                g_previewFont = nullptr;
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int commandShow) {
    g_instance = instance;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        const auto setDpiAwarenessContext =
            reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpiAwarenessContext != nullptr) {
            setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&windowClass)) {
        return 1;
    }

    g_mainWindow = CreateWindowExW(0, kWindowClassName, kAppTitle,
                                   WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 1300, 840,
                                   nullptr, nullptr, instance, nullptr);

    if (g_mainWindow == nullptr) {
        return 1;
    }

    ACCEL accelerators[] = {
        {FVIRTKEY | FCONTROL, 'N', IDM_FILE_NEW},
        {FVIRTKEY | FCONTROL, 'O', IDM_FILE_OPEN},
        {FVIRTKEY | FCONTROL, 'S', IDM_FILE_SAVE},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', IDM_FILE_SAVE_AS},
        {FVIRTKEY, VK_F6, IDM_VIEW_TOGGLE_PREVIEW},
        {FVIRTKEY, VK_F11, IDM_VIEW_FULLSCREEN},
    };
    g_accelerators = CreateAcceleratorTableW(accelerators, static_cast<int>(std::size(accelerators)));

    ShowWindow(g_mainWindow, commandShow);
    UpdateWindow(g_mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (g_accelerators == nullptr || !TranslateAcceleratorW(g_mainWindow, g_accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (g_accelerators != nullptr) {
        DestroyAcceleratorTable(g_accelerators);
        g_accelerators = nullptr;
    }

    return static_cast<int>(message.wParam);
}
