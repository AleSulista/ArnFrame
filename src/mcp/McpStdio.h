#pragma once

namespace drift::mcp {

// Attach to a running Drift MCP server over stdio (Content-Length framed JSON-RPC).
// Returns a process exit code. Does not start the GUI.
int runStdioAttach();

} // namespace drift::mcp
