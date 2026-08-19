#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace mdmate {

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

// Renders Markdown into preview text and formatting spans.
PreviewDocument RenderMarkdownPreview(const std::wstring& markdown);

}
