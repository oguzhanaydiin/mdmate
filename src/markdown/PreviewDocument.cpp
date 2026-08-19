#include "PreviewDocument.h"

#include <sstream>

#include "../core/StringUtils.h"
#include "BlockParser.h"
#include "InlineParser.h"

namespace mdmate {

namespace {

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

}
