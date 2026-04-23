# Project Tick — Coding Standards

## Overview

Project Tick spans multiple programming languages across its sub-projects.
This document defines the coding standards for each language used in the
monorepo. While each sub-project follows conventions appropriate to its
upstream lineage, these standards provide the organizational baseline.

---

## C Style (neozip, cmark, genqrcode, cgit, corebinutils, mnv)

### General Principles

- Follow the existing style of the upstream codebase you are modifying
- Use C11 as the minimum standard (C23 preferred for new code in meshmc)
- Keep functions short and focused
- Prefer stack allocation over heap allocation where possible

### Formatting

- **Indentation:** 4 spaces for neozip, cmark, genqrcode; tabs for cgit, mnv
- **Line length:** 80 characters preferred, 120 maximum
- **Braces:** K&R style (opening brace on same line for functions in some
  components, next line in others — follow the file you are editing)
- **Spaces:** Space after keywords (`if`, `for`, `while`, `switch`), no space
  before parentheses in function calls

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Functions | `snake_case` | `compress_block()` |
| Variables | `snake_case` | `block_size` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_BUFFER_SIZE` |
| Macros | `UPPER_SNAKE_CASE` | `ZLIB_VERSION` |
| Types (struct/enum) | `snake_case` or `PascalCase` (follow upstream) | `z_stream`, `inflate_state` |
| Struct members | `snake_case` | `total_out` |

### Memory Management

- Always check `malloc`/`calloc`/`realloc` return values
- Free resources in reverse order of allocation
- Use `sizeof(*ptr)` instead of `sizeof(type)` for allocation
- Avoid variable-length arrays (VLAs) — use heap allocation instead

### Error Handling

- Return error codes (negative values for errors, zero for success)
- Document error codes in function headers
- Avoid `goto` except for error-cleanup patterns

### neozip-Specific

NeoZip follows zlib-ng conventions:
- Maintain zlib API compatibility
- Use SIMD intrinsics via architecture-specific files in `arch/`
- Guard intrinsics with appropriate CPU feature checks
- Use `zng_` prefixed names for native API functions

### corebinutils-Specific

CoreBinUtils follows FreeBSD kernel style:
- Tabs for indentation
- BSD `err()` / `warn()` family for error reporting
- `POSIX.1-2008` compliance where feasible
- musl-compatible system calls

### cgit-Specific

cgit follows its own established style:
- Tabs for indentation
- Functions prefixed with module name (e.g., `ui_log_`, `cache_`)
- HTML output via `html()`, `htmlf()`, `html_attr()` functions

### mnv-Specific

MNV follows Vim coding conventions:
- Tabs for indentation
- VimScript naming conventions for script functions
- `FEAT_*` macros for feature gating
- Descriptive function names with module prefixes

---

## C++ Style (meshmc, json4cpp, tomlplusplus, libnbtplusplus)

### General Principles

- Use modern C++ idioms (RAII, smart pointers, range-based for, etc.)
- Prefer `std::string_view` over `const char*` for read-only strings
- Prefer `std::optional` over sentinel values
- Avoid raw `new`/`delete` — use smart pointers or containers
- Use `auto` judiciously — prefer explicit types for public APIs

### C++ Standard by Component

| Component | Standard | Reason |
|-----------|----------|--------|
| meshmc | C++23 | Active development, modern compiler requirement |
| json4cpp | C++11 (minimum), C++17 (full features) | Wide compatibility |
| tomlplusplus | C++17 | Modern features, wide support |
| libnbtplusplus | C++11 | Compatibility with older compilers |

### Formatting (meshmc)

MeshMC uses `clang-format` for automated formatting. The `.clang-format`
file at `meshmc/.clang-format` defines the canonical style. Key settings:

- **Indentation:** 4 spaces
- **Line length:** 120 characters
- **Braces:** Allman style (braces on their own lines) for functions;
  attached for control flow
- **Pointer alignment:** Left-aligned (`int* ptr`, not `int *ptr`)
- **Include ordering:** Sorted, grouped by category

Always run clang-format before committing:

```bash
clang-format -i path/to/file.cpp
```

### Naming (meshmc)

| Element | Convention | Example |
|---------|-----------|---------|
| Classes | `PascalCase` | `InstanceList` |
| Methods | `camelCase` | `loadInstance()` |
| Member variables | `m_camelCase` | `m_instanceList` |
| Static members | `s_camelCase` | `s_instance` |
| Local variables | `camelCase` | `blockSize` |
| Constants | `UPPER_SNAKE_CASE` or `k_PascalCase` | `MAX_RETRIES` |
| Namespaces | `PascalCase` or `lowercase` | `Application`, `net` |
| Template params | `PascalCase` | `typename ValueType` |
| Enum values | `PascalCase` | `LoadState::Ready` |
| Files | `PascalCase` | `InstanceList.cpp`, `InstanceList.h` |

### Headers

- Use `#pragma once` instead of include guards
- Include what you use (IWYU principle)
- Forward-declare when possible to reduce compile times
- Order includes: own header first, project headers, third-party, standard

```cpp
#include "InstanceList.h"   // Own header

#include "Application.h"    // Project headers
#include "FileSystem.h"

#include <nlohmann/json.hpp> // Third-party
#include <toml++/toml.hpp>

#include <memory>            // Standard library
#include <string>
#include <vector>
```

### Qt Conventions (meshmc)

- Use Qt container types only when interfacing with Qt APIs
- Prefer `std::` containers for internal logic
- Use `Q_OBJECT` macro for all QObject subclasses
- Use signal/slot connections with lambda syntax
- Use `QStringLiteral()` for string literals in Qt contexts
- Follow Qt naming: signals as verbs (`clicked()`), slots as verb-phrases
  (`handleClicked()`)

### Error Handling (meshmc)

- Use exceptions for truly exceptional conditions
- Use `std::optional` for expected absence of values
- Use result types for operations that can fail in expected ways
- Log errors with Qt's logging categories (`QLoggingCategory`)

### Smart Pointer Usage

```cpp
// Owned heap objects
std::unique_ptr<Instance> instance = std::make_unique<Instance>();

// Shared ownership
std::shared_ptr<Settings> settings = std::make_shared<Settings>();

// Non-owning observation (prefer raw pointer or reference)
Instance* observer = instance.get();

// Qt parent-child ownership
auto* widget = new QWidget(parent); // Qt manages lifetime
```

### json4cpp-Specific

json4cpp follows nlohmann/json conventions:
- Header-only library
- Heavy template metaprogramming
- ADL-based serialization (`to_json`/`from_json`)
- Namespace: `nlohmann`

### tomlplusplus-Specific

toml++ follows its own established conventions:
- Header-only by default
- Namespace: `toml`
- Works without RTTI or exceptions
- C++17 with optional C++20 features

### libnbtplusplus-Specific

libnbt++ uses C++11:
- Tag types as class hierarchy (`nbt::tag_compound`, `nbt::tag_list`, etc.)
- Stream-based I/O
- Namespace: `nbt`

---

## Rust Style (tickborg)

### General Principles

- Follow the [Rust API Guidelines](https://rust-lang.github.io/api-guidelines/)
- Use `rustfmt` for formatting (default configuration)
- Use `clippy` for linting
- Handle errors with `Result<T, E>` — avoid `unwrap()` in library code

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Crates | `snake_case` | `tickborg` |
| Modules | `snake_case` | `build_runner` |
| Types | `PascalCase` | `BuildResult` |
| Functions | `snake_case` | `run_build()` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_RETRIES` |
| Traits | `PascalCase` | `Buildable` |
| Enum variants | `PascalCase` | `Status::Success` |

### Cargo Workspace

The tickborg Cargo workspace uses `resolver = "2"` with two crates:

```toml
[workspace]
members = [
    "tickborg",
    "tickborg-simple-build"
]
resolver = "2"

[profile.release]
debug = true    # Debug info in release builds
```

### Error Handling

```rust
// Use anyhow for application errors
use anyhow::{Context, Result};

fn process_pr(pr: &PullRequest) -> Result<BuildResult> {
    let changes = detect_changes(pr)
        .context("failed to detect changed projects")?;
    // ...
}

// Use thiserror for library errors
use thiserror::Error;

#[derive(Debug, Error)]
enum BuildError {
    #[error("build failed for {project}: {reason}")]
    BuildFailed { project: String, reason: String },
    #[error("project not found: {0}")]
    ProjectNotFound(String),
}
```

---

## Java Style (forgewrapper)

### General Principles

- Follow standard Java conventions (Oracle / Google style)
- Target JDK 17 as the minimum
- Use JPMS (Java Platform Module System) where applicable

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Packages | `lowercase.dotted` | `io.github.zekerzhayard.forgewrapper` |
| Classes | `PascalCase` | `ForgeWrapper` |
| Interfaces | `PascalCase` (often prefixed with `I`) | `IFileDetector` |
| Methods | `camelCase` | `detectFiles()` |
| Constants | `UPPER_SNAKE_CASE` | `FORGE_VERSION` |
| Local variables | `camelCase` | `installerPath` |

### Gradle Build

ForgeWrapper uses Gradle with the wrapper script (`gradlew`/`gradlew.bat`):

```bash
./gradlew build        # Build
./gradlew test         # Test
./gradlew clean        # Clean
```

### Service Provider Interface

ForgeWrapper uses Java SPI for extension:

```java
// Service interface
public interface IFileDetector {
    // Custom file detection logic
}

// Registration via META-INF/services/
// META-INF/services/io.github.zekerzhayard.forgewrapper.installer.detector.IFileDetector
```

---

## Python Style (meta)

### General Principles

- Follow [PEP 8](https://peps.python.org/pep-0008/)
- Target Python 3.10+ (as specified in `pyproject.toml`)
- Use type hints for function signatures
- Use Poetry for dependency management

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Modules | `snake_case` | `generate_forge.py` |
| Functions | `snake_case` | `update_mojang()` |
| Classes | `PascalCase` | `VersionList` |
| Constants | `UPPER_SNAKE_CASE` | `DEPLOY_TO_GIT` |
| Variables | `snake_case` | `version_data` |

### Dependencies

From `pyproject.toml`:

```toml
[tool.poetry.dependencies]
python = ">=3.10,<4.0"
cachecontrol = "^0.14.0"
requests = "^2.31.0"
filelock = "^3.20.3"
packaging = "^25.0"
pydantic = "^1.10.13"
```

### CLI Entry Points

Meta provides Poetry scripts for each operation:

```bash
poetry run generateFabric
poetry run generateForge
poetry run generateNeoForge
poetry run generateQuilt
poetry run generateMojang
poetry run generateJava
poetry run updateFabric
poetry run updateForge
# ... etc.
```

---

## Shell Script Style (bootstrap.sh, hooks, CI scripts)

### General Principles

- Use Bash for complex scripts, POSIX sh for simple ones
- Start with `#!/usr/bin/env bash`
- Enable strict mode: `set -euo pipefail`
- Use `shellcheck` for linting

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Functions | `snake_case` | `detect_distro()` |
| Local variables | `snake_case` | `distro_id` |
| Environment variables | `UPPER_SNAKE_CASE` | `DISTRO_ID` |
| Constants | `UPPER_SNAKE_CASE` | `RED`, `GREEN`, `NC` |

### Error Handling

Use colored output functions as defined in `bootstrap.sh`:

```bash
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { printf "${CYAN}[INFO]${NC}  %s\n" "$*"; }
ok()    { printf "${GREEN}[ OK ]${NC}  %s\n" "$*"; }
warn()  { printf "${YELLOW}[WARN]${NC}  %s\n" "$*"; }
err()   { printf "${RED}[ERR]${NC}  %s\n" "$*" >&2; }
```

### Best Practices

- Quote all variable expansions: `"$var"` not `$var`
- Use `[[ ]]` for conditional tests (Bash)
- Use `$()` for command substitution (not backticks)
- Use `local` for function-scoped variables
- Check command existence with `command -v`

---

## JavaScript / Node.js Style (CI scripts)

### General Principles

The CI scripts in `ci/github-script/` follow the formatting enforced by
`biome` (configured in `ci/default.nix`):

- **Quotes:** Single quotes
- **Semicolons:** None (ASI)
- **Indentation:** 2 spaces

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Functions | `camelCase` | `lintCommits()` |
| Variables | `camelCase` | `prNumber` |
| Constants | `UPPER_SNAKE_CASE` or `camelCase` | `MAX_RETRIES` |
| Files | `kebab-case` | `lint-commits.js` |
| Modules | `camelCase` exports | `module.exports = { classify }` |

---

## Nix Style (flake.nix, ci/default.nix, deployment modules)

### General Principles

- Format with `nixfmt` (RFC style, as configured in CI)
- Use `let ... in` for local bindings
- Prefer attribute sets over positional arguments
- Pin all inputs with content hashes

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Attributes | `camelCase` | `devShells`, `nixpkgsFor` |
| Packages | `kebab-case` | `clang-tidy-diff` |
| Variables | `camelCase` | `forAllSystems` |
| Functions | `camelCase` | `mkShell` |

---

## CMake Style (meshmc, libraries)

### General Principles

- Minimum CMake version: 3.20 (meshmc), 3.15 (libnbtplusplus)
- Use `target_*` commands instead of directory-level `include_directories()`
- Export compile commands: `CMAKE_EXPORT_COMPILE_COMMANDS ON`
- Use `find_package()` for external dependencies

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Variables | `PascalCase` or `UPPER_CASE` | `MeshMC_VERSION_MAJOR` |
| Options | `PascalCase` | `ENABLE_LTO`, `NBT_BUILD_SHARED` |
| Targets | `PascalCase` | `MeshMC`, `nbt++` |
| Functions | `snake_case` | `query_qmake()` |

### CMake Options (meshmc)

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_LTO` | OFF | Enable Link Time Optimization |
| `MeshMC_DISABLE_JAVA_DOWNLOADER` | OFF | Disable built-in Java downloader |
| `MeshMC_ENABLE_CLANG_TIDY` | OFF | Enable clang-tidy during compilation |

---

## Dockerfile Style (images4docker)

### General Principles

- One base image per Dockerfile
- Use multi-stage builds where appropriate
- Minimize layer count
- Validate Qt 6 availability during build (fail fast)
- Pin base image tags

### Naming

- Dockerfiles: `<distro>.Dockerfile`
- Image tags: `ghcr.io/project-tick-infra/images/<target_name>:<target_tag>`

---

## Cross-Language Standards

### Git Commit Messages

All languages follow the same Conventional Commits format (see
[contributing.md](contributing.md)):

```
type(scope): short description
```

### SPDX Headers

All source files should include SPDX headers appropriate to their comment
syntax:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Project Tick
```

```python
# SPDX-License-Identifier: MS-PL
# SPDX-FileCopyrightText: 2026 Project Tick
```

```rust
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: NixOS Contributors & Project Tick
```

### Static Analysis

| Language | Tool | Integration |
|----------|------|-------------|
| C/C++ | clang-tidy | `MeshMC_ENABLE_CLANG_TIDY`, CI |
| C/C++ | clang-format | Pre-commit hook, CI |
| C/C++ | checkpatch.pl | Pre-commit hook |
| C/C++ | CodeQL | CI workflows |
| C/C++ | Coverity | CI (mnv only) |
| C/C++ | Flawfinder | CI (json4cpp) |
| C/C++ | Semgrep | CI (json4cpp) |
| Rust | clippy | `cargo clippy` |
| Rust | rustfmt | `cargo fmt` |
| Python | (standard linters) | Poetry dev deps |
| JavaScript | biome | treefmt CI |
| Nix | nixfmt | treefmt CI |
| YAML | yamlfmt | treefmt CI |
| GitHub Actions | actionlint, zizmor | treefmt CI |
