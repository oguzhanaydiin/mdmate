#pragma once

#include "../markdown/PreviewDocument.h"

namespace mdmate {

// Applies preview text and formatting to Rich Edit.
void ApplyPreviewStyles(const PreviewDocument& doc);

// Applies the active theme to editor controls.
void ApplyEditorTheme();

}
