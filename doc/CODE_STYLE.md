# Code style and commenting (FluidLevelMonitor)

## Language

- C++14 as used by Arduino ESP8266 core.
- Comments and Doxygen in **English**.

## Doxygen (headers)

Public APIs in `.h` files should include:

- `@file` — one-line purpose
- `@brief` on classes and non-trivial functions
- `@param` / `@return` where not obvious
- `@note` for ESP8266 constraints (single-threaded, no heap abuse, call from `loop` only)

## When to comment

- **Why** a non-obvious branch exists (OTA early return, filter order, WiFi state).
- **State machines** — transition table or bullet list in the module header.
- **Magic numbers** — named constants or `// reason` on the line.

Avoid commented-out dead code; delete or use `#if 0` only briefly during bring-up.

## Logging policy

| Level | Use |
|-------|-----|
| TRACE/DEBUG | Verbose (compile with `FLM_LOG_VERBOSE`); WiFi scan details, per-packet noise |
| INFO | Boot milestones, connect/disconnect summaries |
| WARN | Recoverable issues, validation clamps, retry |
| ERROR | Save failures, parse errors, OTA abort |

**Never log:** WiFi password, AP password, MQTT password, API bodies that contain credentials.

Use `Log::info("WiFi", "...")` etc. from `utils/Log.h`.

## Assertions

`FLM_ASSERT(expr)` — active when `FLM_DEBUG` defined; no-op in release builds. Use for impossible internal states, not user input.

## Formatting

- Match existing file style (sections with `// ==========`).
- Prefer early returns in long handlers.
