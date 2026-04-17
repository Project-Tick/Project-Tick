# Code Formatting

## Overview

Project Tick uses [treefmt](https://github.com/numtide/treefmt) orchestrated through
[treefmt-nix](https://github.com/numtide/treefmt-nix) to enforce consistent code formatting
across the entire monorepo. The formatting configuration lives in `ci/default.nix` and
covers JavaScript, Nix, YAML, GitHub Actions workflows, and sorted-list enforcement.

---

## Configured Formatters

### Summary Table

| Formatter    | Language/Files                | Key Settings                              |
|-------------|-------------------------------|-------------------------------------------|
| `actionlint` | GitHub Actions YAML          | Default (syntax + best practices)         |
| `biome`      | JavaScript / TypeScript      | Single quotes, optional semicolons        |
| `keep-sorted`| Any (marked sections)        | Default                                   |
| `nixfmt`     | Nix expressions              | nixfmt-rfc-style                          |
| `yamlfmt`    | YAML files                   | Retain line breaks                        |
| `zizmor`     | GitHub Actions YAML          | Security scanning                         |

---

### actionlint

**Purpose**: Validates GitHub Actions workflow files for syntax errors, type mismatches,
and best practices.

**Scope**: `.github/workflows/*.yml`

**Configuration**: Default — no custom settings.

```nix
programs.actionlint.enable = true;
```

**What it catches**:
- Invalid workflow syntax
- Missing or incorrect `runs-on` values
- Type mismatches in expressions
- Unknown action references

---

### biome

**Purpose**: Formats JavaScript and TypeScript source files with consistent style.

**Scope**: All `.js` and `.ts` files except `*.min.js`

**Configuration**:

```nix
programs.biome = {
  enable = true;
  validate.enable = false;
  settings.formatter = {
    useEditorconfig = true;
  };
  settings.javascript.formatter = {
    quoteStyle = "single";
    semicolons = "asNeeded";
  };
  settings.json.formatter.enabled = false;
};
settings.formatter.biome.excludes = [
  "*.min.js"
];
```

**Style rules**:

| Setting              | Value          | Effect                                    |
|---------------------|----------------|-------------------------------------------|
| `useEditorconfig`   | `true`         | Respects `.editorconfig` (indent, etc.)   |
| `quoteStyle`        | `"single"`     | Uses `'string'` instead of `"string"`    |
| `semicolons`        | `"asNeeded"`   | Only inserts `;` where ASI requires it   |
| `validate.enable`   | `false`        | No lint-level validation, only formatting |
| `json.formatter`    | `disabled`     | JSON files are not formatted by biome     |

**Exclusions**: `*.min.js` — Minified JavaScript files are never reformatted.

---

### keep-sorted

**Purpose**: Enforces alphabetical ordering in marked sections of any file type.

**Scope**: Files containing `keep-sorted` markers.

```nix
programs.keep-sorted.enable = true;
```

**Usage**: Add markers around sections that should stay sorted:

```
# keep-sorted start
apple
banana
cherry
# keep-sorted end
```

---

### nixfmt

**Purpose**: Formats Nix expressions according to the RFC-style convention.

**Scope**: All `.nix` files.

```nix
programs.nixfmt = {
  enable = true;
  package = pkgs.nixfmt;
};
```

The `pkgs.nixfmt` package from the pinned Nixpkgs provides the formatter. This
is `nixfmt-rfc-style`, the official Nix formatting standard.

---

### yamlfmt

**Purpose**: Formats YAML files with consistent indentation and structure.

**Scope**: All `.yml` and `.yaml` files.

```nix
programs.yamlfmt = {
  enable = true;
  settings.formatter = {
    retain_line_breaks = true;
  };
};
```

**Key setting**: `retain_line_breaks = true` — Preserves intentional blank lines between
YAML sections, preventing the formatter from collapsing the file into a dense block.

---

### zizmor

**Purpose**: Security scanner for GitHub Actions workflows. Detects injection
vulnerabilities, insecure defaults, and untrusted input handling.

**Scope**: `.github/workflows/*.yml`

```nix
programs.zizmor.enable = true;
```

**What it detects**:
- Script injection via `${{ github.event.* }}` in `run:` steps
- Insecure use of `pull_request_target`
- Unquoted expressions that could be exploited
- Dangerous permission configurations

---

## treefmt Global Settings

```nix
projectRootFile = ".git/config";
settings.verbose = 1;
settings.on-unmatched = "debug";
```

| Setting             | Value         | Purpose                                      |
|--------------------|---------------|----------------------------------------------|
| `projectRootFile`  | `.git/config` | Identifies repository root for treefmt       |
| `settings.verbose` | `1`           | Logs which files each formatter processes    |
| `settings.on-unmatched` | `"debug"` | Files with no matching formatter are logged at debug level |

---

## Running Formatters

### In CI

The formatting check runs as a Nix derivation:

```bash
nix-build ci/ -A fmt.check
```

This:
1. Copies the full source tree (excluding `.git`) into the Nix store
2. Runs all configured formatters
3. Fails with a diff if any file would be reformatted

### Locally (Nix Shell)

```bash
cd ci/
nix-shell         # enter CI dev shell
treefmt           # format all files
treefmt --check   # check without modifying (dry run)
```

### Locally (Nix Build)

```bash
# Just check (no modification):
nix-build ci/ -A fmt.check

# Get the formatter binary:
nix-build ci/ -A fmt.pkg
./result/bin/treefmt
```

---

## Source Tree Construction

The treefmt check operates on a clean copy of the source tree:

```nix
fs = pkgs.lib.fileset;
src = fs.toSource {
  root = ../.;
  fileset = fs.difference ../. (fs.maybeMissing ../.git);
};
```

This:
- Takes the entire repository directory (`../.` from `ci/`)
- Excludes the `.git` directory (which is large and irrelevant for formatting)
- `fs.maybeMissing` handles the case where `.git` doesn't exist (e.g., in tarballs)

The resulting source is passed to`fmt.check`:

```nix
check = treefmtEval.config.build.check src;
```

---

## Formatter Outputs

The formatting system exposes three Nix attributes:

```nix
{
  shell = treefmtEval.config.build.devShell;   # Interactive shell
  pkg = treefmtEval.config.build.wrapper;      # treefmt binary
  check = treefmtEval.config.build.check src;  # CI check derivation
}
```

| Attribute   | Use Case                                               |
|------------|--------------------------------------------------------|
| `fmt.shell` | `nix develop .#fmt.shell` — interactive formatting     |
| `fmt.pkg`   | The treefmt wrapper with all formatters bundled        |
| `fmt.check` | `nix build .#fmt.check` — CI formatting check          |

---

## Troubleshooting

### "File would be reformatted"

If CI fails with formatting issues:

```bash
# Enter the CI shell to get the exact same formatter versions:
cd ci/
nix-shell

# Format all files:
treefmt

# Stage and commit the changes:
git add -u
git commit -m "style(repo): apply treefmt formatting"
```

### Editor Integration

For real-time formatting in VS Code:

1. Use the biome extension for JavaScript/TypeScript
2. Configure single quotes and optional semicolons to match CI settings
3. Use nixpkgs-fmt or nixfmt for Nix files

### Formatter Conflicts

Each file type has exactly one formatter assigned by treefmt. If a file matches
multiple formatters, treefmt reports a conflict. The current configuration avoids
this by:
- Disabling biome's JSON formatter
- Having non-overlapping file type coverage
