# Code Formatting

## Overview

Project Tick enforces consistent code formatting across the entire monorepo using
native CI jobs in `ci-lint.yml`. Each formatter runs as an independent job and covers
JavaScript, YAML, GitHub Actions workflows, and sorted-list enforcement.

---

## Configured Formatters

### Summary Table

| Formatter    | Language/Files                | CI Job       | Key Settings                              |
|-------------|-------------------------------|--------------|-------------------------------------------|
| `actionlint` | GitHub Actions YAML          | `actionlint` | Default (syntax + best practices)         |
| `biome`      | JavaScript / TypeScript      | `biome`      | Single quotes, optional semicolons        |
| `keep-sorted`| Any (marked sections)        | —            | Enforced by convention                    |
| `yamlfmt`    | YAML files                   | `yamlfmt`    | Retain line breaks                        |
| `zizmor`     | GitHub Actions YAML          | `zizmor`     | Security scanning                         |

---

### actionlint

**Purpose**: Validates GitHub Actions workflow files for syntax errors, type mismatches,
and best practices.

**Scope**: `.github/workflows/*.yml`

**CI**: `raven-actions/actionlint@v2`

**What it catches**:
- Invalid workflow syntax
- Missing or incorrect `runs-on` values
- Type mismatches in expressions
- Unknown action references

---

### biome

**Purpose**: Formats JavaScript and TypeScript source files with consistent style.

**Scope**: `ci/github-script/` — all `.js` and `.ts` files except `*.min.js`

**CI**: `npm install --global @biomejs/biome && biome check --formatter-enabled=true`

**Style rules**:

| Setting              | Value          | Effect                                    |
|---------------------|----------------|-------------------------------------------|
| `useEditorconfig`   | `true`         | Respects `.editorconfig` (indent, etc.)   |
| `quoteStyle`        | `"single"`     | Uses `'string'` instead of `"string"`    |
| `semicolons`        | `"asNeeded"`   | Only inserts `;` where ASI requires it   |
| `json.formatter`    | `disabled`     | JSON files are not formatted by biome     |

**Exclusions**: `*.min.js` — Minified JavaScript files are never reformatted.

---

### keep-sorted

**Purpose**: Enforces alphabetical ordering in marked sections of any file type.

**Scope**: Files containing `keep-sorted` markers.

**Usage**: Add markers around sections that should stay sorted:

```
# keep-sorted start
apple
banana
cherry
# keep-sorted end
```

---

### yamlfmt

**Purpose**: Formats YAML files with consistent indentation and structure.

**Scope**: All `.yml` and `.yaml` files in `.github/workflows/`

**CI**: `go install github.com/google/yamlfmt/cmd/yamlfmt@latest && yamlfmt -dry -lint`

**Key setting**: `retain_line_breaks=true` — Preserves intentional blank lines between
YAML sections, preventing the formatter from collapsing the file into a dense block.

---

### zizmor

**Purpose**: Security scanner for GitHub Actions workflows. Detects injection
vulnerabilities, insecure defaults, and untrusted input handling.

**Scope**: `.github/workflows/*.yml`

**CI**: `woodruffw/zizmor-action@v1` — results uploaded as SARIF

**What it detects**:
- Script injection via `${{ github.event.* }}` in `run:` steps
- Insecure use of `pull_request_target`
- Unquoted expressions that could be exploited
- Dangerous permission configurations

---

## Running Formatters Locally

### biome (JS/TS)

```bash
npm install --global @biomejs/biome
biome check --formatter-enabled=true ci/github-script/
biome format --write ci/github-script/
```

### yamlfmt (YAML)

```bash
go install github.com/google/yamlfmt/cmd/yamlfmt@latest
# Check:
yamlfmt -dry -lint -formatter retain_line_breaks=true .github/workflows/*.yml
# Fix:
yamlfmt -formatter retain_line_breaks=true .github/workflows/*.yml
```

### actionlint (GitHub Actions)

```bash
# Install: https://github.com/rhysd/actionlint
actionlint .github/workflows/*.yml
```

### zizmor (security)

```bash
# Install: cargo install zizmor  OR  pip install zizmor
zizmor .github/workflows/
```

---

## Troubleshooting

### "Biome check failed"

```bash
# Auto-fix formatting:
biome format --write ci/github-script/
git add -u
git commit -m "style(ci): apply biome formatting"
```

### "yamlfmt reports diff"

```bash
yamlfmt -formatter retain_line_breaks=true .github/workflows/*.yml
git add -u
git commit -m "style(ci): apply yamlfmt formatting"
```

### Editor Integration

For real-time formatting in VS Code:

1. Install the **Biome** extension for JavaScript/TypeScript
2. Install the **YAML** extension for YAML formatting
3. Configure single quotes and optional semicolons to match CI settings
