#pragma once

// ToolkitError — the std::expected error payload used at the toolkit's fallible
// boundaries (file IO for settings/sessions). Per conventions.md: std::expected at
// API boundaries, never exceptions across the public surface. Per-entry JSON load
// stays log-and-skip (it does NOT surface as an error) — only the I/O / parse
// boundary returns a ToolkitError.

#include <string>

namespace imtool {

enum class ErrorKind {
    JsonParse,        // the file was not valid JSON
    JsonType,         // a value had the wrong JSON type at a boundary that can't skip
    IoFailure,        // open/read/write failed
    Unsupported,      // operation not supported in this build/config
    InvalidArgument,  // caller passed something nonsensical
};

struct ToolkitError {
    ErrorKind   kind;
    std::string what;          // human-readable cause
    std::string where = {};    // optional context (e.g. the file path)
};

}  // namespace imtool
