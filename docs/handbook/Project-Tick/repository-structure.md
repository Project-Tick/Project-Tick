# Project Tick — Repository Structure

## Overview

The Project Tick monorepo contains all source code, libraries, applications,
infrastructure tooling, documentation, and archived projects under a single
Git repository. This document provides a complete map of every top-level
directory and significant file.

---

## Root Directory

```
Project-Tick/
│
├── .envrc                   # direnv configuration
├── .gitattributes           # Git attribute rules
├── .gitignore               # Root-level ignore patterns
├── .gitmodules              # Git submodule definitions
├── bootstrap.cmd            # Windows bootstrap script
├── bootstrap.sh             # Linux/macOS bootstrap script
├── CODE_OF_CONDUCT.md       # Code of Conduct v2
├── CONTRIBUTING.md          # Contribution guidelines, CLA, DCO, AI policy
├── flake.lock               # Nix flake lock file
├── flake.nix                # Top-level Nix flake
├── lefthook.yml             # Git hooks config
├── README.md                # Root README with project overview
├── REUSE.toml               # REUSE 3.0 license annotations
├── SECURITY.md              # Security vulnerability reporting
├── TRADEMARK.md             # Trademark and brand policy
├── tree.txt                 # Static directory tree snapshot
│
├── .github/                 # GitHub configuration
├── archived/                # Deprecated sub-projects
├── cgit/                    # cgit Git web interface
├── ci/                      # CI infrastructure and tooling
├── cmark/                   # cmark Markdown parser
├── corebinutils/            # BSD/FreeBSD core utilities
├── docs/                    # Documentation
├── forgewrapper/            # ForgeWrapper Java shim
├── genqrcode/               # QR code encoding library
├── hooks/                   # Git hook scripts
├── images4docker/           # Docker build environments
├── json4cpp/                # JSON library
├── libnbtplusplus/          # NBT library
├── LICENSES/                # SPDX license texts
├── meshmc/                  # MeshMC Minecraft launcher
├── meta/                    # Metadata generator
├── mnv/                     # MNV text editor
├── neozip/                  # Compression library
├── ofborg/                  # tickborg CI bot
└── tomlplusplus/            # TOML library
```

---

## .github/ — GitHub Configuration

```
.github/
├── CODEOWNERS                    # Review routing
├── dco.yml                       # DCO bot config
├── pull_request_template.md      # PR template
│
├── ISSUE_TEMPLATE/
│   ├── bug_report.yml            # Structured bug report form
│   ├── config.yml                # Issue template configuration
│   ├── rfc.yml                   # RFC template
│   └── suggestion.yml            # Feature suggestion template
│
├── actions/                      # Reusable composite actions
│   ├── change-analysis/          # File change detection logic
│   ├── meshmc/
│   │   ├── package/              # MeshMC packaging action
│   │   └── setup-dependencies/   # MeshMC dependency setup action
│   └── mnv/
│       └── test_artefacts/       # MNV test artifacts action
│
├── codeql/                       # CodeQL analysis configuration
│
└── workflows/                    # 50+ GitHub Actions workflow files
    ├── ci.yml                    # Monolithic CI orchestrator
    ├── ci-lint.yml               # Commit/format linting
    ├── ci-schedule.yml           # Scheduled CI jobs
    ├── meshmc-*.yml              # MeshMC workflows
    ├── neozip-*.yml              # NeoZip workflows
    ├── json4cpp-*.yml            # json4cpp workflows
    ├── cmark-*.yml               # cmark workflows
    ├── tomlplusplus-*.yml        # toml++ workflows
    ├── mnv-*.yml                 # MNV workflows
    ├── cgit-ci.yml               # cgit CI
    ├── corebinutils-ci.yml       # CoreBinUtils CI
    ├── forgewrapper-build.yml    # ForgeWrapper CI
    ├── libnbtplusplus-ci.yml     # libnbt++ CI
    ├── genqrcode-ci.yml          # GenQRCode CI
    ├── images4docker-build.yml   # Docker image builds
    └── repo-*.yml                # Repository maintenance
```

---

## meshmc/ — MeshMC Launcher

The largest sub-project. A Qt 6 / C++23 custom Minecraft launcher.

```
meshmc/
├── .clang-format              # C++ formatting rules
├── .clang-tidy                # Static analysis configuration
├── .envrc                     # direnv configuration
├── .gitattributes             # File attribute rules
├── .gitignore                 # Ignore patterns
├── .markdownlint.yaml         # Markdown lint config
├── .markdownlintignore        # Markdown lint ignore
├── BUILD.md                   # Comprehensive build guide
├── CMakeLists.txt             # Root CMake configuration
├── CMakePresets.json          # Platform-specific CMake presets
├── CONTRIBUTING.md            # MeshMC-specific contribution guide
├── COPYING.md                 # License information
├── Containerfile              # Container build file
├── README.md                  # MeshMC overview
├── REUSE.toml                 # License annotations
├── changelog.md               # Version changelog
├── default.nix                # Nix build expression
├── flake.lock                 # Nix flake lock
├── flake.nix                  # Nix flake
├── shell.nix                  # Nix development shell
│
├── branding/                  # Icons, logos, splash screens
├── build/                     # Build output directory
├── buildconfig/               # Compile-time configuration generation
├── cmake/                     # Custom CMake modules
├── doc/                       # MeshMC-specific documentation
├── install/                   # Install output directory
├── LICENSES/                  # MeshMC-specific license copies
├── nix/                       # Nix packaging
├── scripts/                   # Build and maintenance scripts
├── updater/                   # Auto-update mechanism
│
├── launcher/                  # Main application source
│   ├── main.cpp               # Entry point
│   ├── Application.cpp/.h     # Application singleton
│   ├── CMakeLists.txt         # Launcher CMake
│   ├── icons/                 # Icon management
│   ├── java/                  # Java runtime management
│   ├── launch/                # Game launch logic
│   ├── meta/                  # Metadata handling
│   ├── minecraft/             # Minecraft-specific logic
│   ├── modplatform/           # Mod platform integration
│   ├── mojang/                # Mojang API integration
│   ├── net/                   # Networking
│   ├── news/                  # News feed
│   ├── notifications/         # User notifications
│   ├── resources/             # Qt resources
│   ├── screenshots/           # Screenshot management
│   ├── settings/              # Settings system
│   ├── tasks/                 # Async task framework
│   ├── testdata/              # Test fixtures
│   ├── tools/                 # Tool integrations
│   ├── translations/          # i18n
│   ├── ui/                    # Qt UI
│   └── updater/               # In-app updater
│
└── libraries/                 # Bundled library integrations
```

Key source files in `launcher/`:
- `Application.cpp` — Application lifecycle management
- `BaseInstance.cpp` — Minecraft instance abstraction
- `InstanceList.cpp` — Instance collection management
- `LaunchController.cpp` — Game launch orchestration
- `FileSystem.cpp` — Cross-platform file operations
- `Json.cpp` — JSON utilities (wrapping json4cpp)
- `GZip.cpp` — Compression utilities (wrapping zlib/neozip)

---

## mnv/ — MNV Text Editor

Vim fork with modern enhancements.

```
mnv/
├── CMakeLists.txt          # CMake build
├── CMakePresets.json        # CMake presets
├── configure               # Autotools configure script
├── CONTRIBUTING.md          # Contribution guide
├── COPYING.md               # License
├── LICENSE                  # Vim license text
├── Makefile                 # Root Makefile
├── README.md                # Overview
├── SECURITY.md              # Security policy
│
├── ci/                     # MNV-specific CI scripts
├── cmake/                  # CMake modules
├── lang/                   # Language support files
├── nsis/                   # Windows installer
├── pixmaps/                # Icons and graphics
├── runtime/                # Runtime files
├── src/                    # C source code
├── tools/                  # Development tools
└── build/                  # Build output
```

---

## cgit/ — Git Web Interface

```
cgit/
├── Makefile                # Build system
├── cgit.c                  # Main CGI entry point
├── cgit.h                  # Core data structures
├── cgit.css                # Default stylesheet
├── cgit.js                 # Client-side JavaScript
├── cgit.mk                 # Build configuration
├── cgitrc.5.txt            # Man page source
├── COPYING                 # GPL-2.0 license
├── README                  # Build instructions
├── robots.txt              # Default robots.txt
│
├── cache.c/.h              # Response caching
├── cmd.c/.h                # Command dispatching
├── configfile.c/.h         # Configuration parsing
├── filter.c                # Content filtering
├── html.c/.h               # HTML output generation
├── parsing.c               # Git object parsing
├── scan-tree.c/.h          # Repository scanning
├── shared.c                # Shared utilities
│
├── ui-*.c/.h                # UI modules:
│   ├── ui-atom              # Atom feed
│   ├── ui-blame             # File blame view
│   ├── ui-blob              # File content view
│   ├── ui-clone             # Clone URL display
│   ├── ui-commit            # Commit view
│   ├── ui-diff              # Diff view
│   ├── ui-log               # Commit log
│   ├── ui-patch             # Patch view
│   ├── ui-plain             # Plain text view
│   ├── ui-refs              # Reference listing
│   ├── ui-repolist          # Repository listing
│   ├── ui-shared            # Shared UI utilities
│   ├── ui-snapshot          # Tarball/zip snapshots
│   ├── ui-ssdiff            # Side-by-side diff
│   ├── ui-stats             # Statistics
│   ├── ui-summary           # Repository summary
│   ├── ui-tag               # Tag view
│   └── ui-tree              # Tree view
│
├── contrib/                # Third-party contributions
├── filters/                # Content filter scripts
├── git/                    # Bundled or Submodule Git source
└── tests/                  # Test suite
```

---

## neozip/ — Compression Library

zlib-ng fork with SIMD acceleration.

```
neozip/
├── CMakeLists.txt          # CMake build
├── configure               # Autotools-style configure
├── Makefile.in             # Make template
├── FAQ.zlib                # zlib FAQ
├── INDEX.md                # File index
├── LICENSE.md              # Zlib license
├── PORTING.md              # Porting guide
├── README.md               # Overview
│
├── adler32.c               # Adler-32 checksum
├── compress.c              # compression wrapper
├── crc32.c                 # CRC-32 checksum
├── deflate.c               # Deflate compression
├── deflate_fast.c          # Fast deflate strategy
├── deflate_huff.c          # Huffman-only strategy
├── deflate_medium.c        # Medium deflate strategy
├── deflate_quick.c         # Quick deflate strategy
├── deflate_rle.c           # RLE deflate strategy
├── deflate_slow.c          # Slow (best) deflate strategy
├── deflate_stored.c        # Stored (no compression)
├── inflate.c               # Inflate decompression
├── infback.c               # Inflate back-stream
├── trees.c                 # Huffman tree construction
├── uncompr.c               # Uncompress wrapper
├── gzlib.c                 # gzip file utilities
├── gzread.c                # gzip read
├── gzwrite.c               # gzip write
│
├── arch/                   # Architecture-specific SIMD code
│   ├── x86/                # SSE2, SSE4, AVX2, AVX512, PCLMULQDQ
│   ├── arm/                # NEON, ARMv8 CRC, PMULL
│   ├── power/              # VMX, VSX, Power8/9
│   ├── s390/               # DFLTCC (hardware deflate)
│   ├── riscv/              # RVV, ZBC
│   └── loongarch/          # LSX, LASX
│
├── cmake/                  # CMake modules
├── doc/                    # Documentation
├── test/                   # Test suite
├── tools/                  # Development tools
└── win32/                  # Windows-specific files
```

---

## Libraries (Other)

### json4cpp/

```
json4cpp/
├── CMakeLists.txt          # CMake build
├── meson.build             # Meson build
├── BUILD.bazel             # Bazel build
├── MODULE.bazel            # Bazel module
├── Package.swift           # Swift Package Manager
├── Makefile                # Convenience Makefile
├── LICENSE.MIT             # MIT license
├── README.md               # Comprehensive usage guide
│
├── include/nlohmann/       # Public headers
├── single_include/         # Amalgamated single header
├── src/                    # Implementation
├── docs/                   # MkDocs documentation
├── tests/                  # Catch2 test suite
├── cmake/                  # CMake modules
└── tools/                  # Development tools
```

### tomlplusplus/

```
tomlplusplus/
├── meson.build             # Primary build
├── meson_options.txt       # Meson options
├── CMakeLists.txt          # CMake build
├── LICENSE                 # License text
├── README.md               # Overview and usage
├── toml.hpp                # Single header include
│
├── include/toml++/         # Multi-header includes
├── src/                    # Implementation files
├── docs/                   # Documentation
├── examples/               # Usage examples
├── tests/                  # Test suite
├── toml-test/              # Official TOML test suite
├── fuzzing/                # Fuzz testing
└── tools/                  # Development tools
```

### libnbtplusplus/

```
libnbtplusplus/
├── CMakeLists.txt          # CMake build
├── COPYING                 # LGPL-3.0 full text
├── COPYING.LESSER          # LGPL-3.0 lesser clause
├── README.md               # Build guide
│
├── include/                # Public headers
├── src/                    # Implementation
└── test/                   # Test suite
```

### genqrcode/

```
genqrcode/
├── CMakeLists.txt          # CMake build
├── configure.ac            # Autotools configuration
├── autogen.sh              # Autotools bootstrap
├── Makefile.am             # Autotools Makefile
├── COPYING                 # LGPL-2.1 license
├── README.md               # Overview
│
├── qrencode.c/.h           # Main encoding API
├── qrinput.c/.h            # Input processing
├── qrspec.c/.h             # QR specification tables
├── bitstream.c/.h          # Bit stream utilities
├── mask.c/.h               # Masking patterns
├── rsecc.c/.h              # Reed-Solomon error correction
├── split.c/.h              # Data splitting
├── qrenc.c                 # CLI tool
│
├── cmake/                  # CMake modules
├── tests/                  # Test suite
└── use/                    # Usage examples
```

### forgewrapper/

```
forgewrapper/
├── build.gradle            # Gradle build script
├── settings.gradle         # Gradle settings
├── gradle.properties       # Build properties
├── gradlew                 # Unix Gradle wrapper
├── gradlew.bat             # Windows Gradle wrapper
├── LICENSE                 # MIT license
├── README.md               # Usage guide
│
├── gradle/                 # Gradle wrapper JAR
├── jigsaw/                 # JPMS module configuration
└── src/
    └── main/java/          # Java source
        └── io/github/zekerzhayard/forgewrapper/
            └── installer/
                └── detector/
                    └── IFileDetector.java  # SPI interface
```

---

## Infrastructure

### meta/ — Metadata Generator

```
meta/
├── pyproject.toml          # Poetry project configuration
├── poetry.lock             # Locked Python dependencies
├── requirements.txt        # pip requirements
├── README.md               # Deployment guide
├── COPYING / LICENSE        # MS-PL license
├── config.sh / config.example.sh  # Shell configuration
├── init.sh                 # Initialization script
├── update.sh               # Update script
├── flake.nix / flake.lock  # Nix flake
├── garnix.yaml             # Garnix CI configuration
├── renovate.json           # Renovate dependency updates
│
├── meta/                   # Python package
│   └── run/                # CLI entry points
│       ├── generate_fabric.py
│       ├── generate_forge.py
│       ├── generate_mojang.py
│       ├── generate_neoforge.py
│       ├── generate_quilt.py
│       ├── generate_java.py
│       └── ...
│
├── cache/ / caches/        # Cached upstream data
├── launcher/               # Launcher configuration
├── public/                 # Generated output
├── upstream/               # Upstream source data
├── fuzz/                   # Fuzz testing
├── nix/                    # Nix packaging
└── venv/                   # Python virtual environment
```

### ofborg/ — tickborg CI Bot

```
ofborg/
├── Cargo.toml              # Workspace root
├── Cargo.lock              # Locked Rust dependencies
├── Dockerfile              # Container build
├── docker-compose.yml      # Multi-container deployment
├── DEPLOY.md               # Deployment guide
├── README.md               # Overview and bot commands
├── LICENSE                 # MIT license
├── default.nix             # Nix build
├── flake.nix / flake.lock  # Nix flake
├── shell.nix               # Development shell
├── service.nix             # NixOS service module
├── config.production.json  # Production config
├── config.public.json      # Public config
├── example.config.json     # Example config
│
├── tickborg/               # Main CI bot
│   ├── Cargo.toml
│   └── src/
│
├── tickborg-simple-build/  # Simplified builder
│   ├── Cargo.toml
│   └── src/
│
├── ofborg-viewer/          # Build status viewer
│
├── deploy/                 # Deployment scripts
├── doc/                    # Documentation
└── target/                 # Cargo build output
```

### images4docker/

```
images4docker/
├── README.md               # Overview and workflow docs
├── LICENSE                 # GPL-3.0 license
│
├── dockerfiles/            # 40 Dockerfile-per-distro files
│   ├── debian-12.Dockerfile
│   ├── ubuntu-24.04.Dockerfile
│   ├── fedora-41.Dockerfile
│   ├── alpine-3.20.Dockerfile
│   └── ...
│
└── LICENSES/               # License copies
```

### ci/ — CI Infrastructure

```
ci/
├── OWNERS                  # CI code ownership
├── README.md               # CI documentation
├── default.nix             # Nix CI entry
├── pinned.json             # Pinned Nixpkgs revision + hash
├── supportedBranches.js    # Branch classification logic
├── update-pinned.sh        # Update pinned.json
│
├── codeowners-validator/   # CODEOWNERS validation tool
│   ├── default.nix
│   ├── owners-file-name.patch
│   └── permissions.patch
│
└── github-script/          # GitHub Actions JS helpers
    ├── run                 # CLI entry point
    ├── lint-commits.js     # Conventional Commits linter
    ├── prepare.js          # PR preparation
    ├── reviews.js          # Review state management
    ├── get-pr-commit-details.js
    ├── withRateLimit.js    # API rate limiting
    ├── package.json        # npm dependencies
    └── shell.nix           # Nix dev shell
```

---

## corebinutils/ — BSD Utilities

```
corebinutils/
├── config.mk                # Toolchain configuration
├── configure                # Toolchain detection script
├── GNUmakefile              # Top-level orchestrator
├── README.md                # Build instructions
├── build/                   # Shared build infrastructure
├── contrib/                 # Contributed utilities
├── cat/          
├── chmod/        
├── cp/
├── chflags/      
├── cpuset/       
├── csh/
├── date/         
├── dd/           
├── df/
├── domainname/   
├── echo/         
├── ed/
├── expr/         
├── freebsd-version/ 
├── getfacl/
├── hostname/     
├── kill/         
├── ln/
├── ls/           
├── mkdir/        
├── mv/
├── nproc/        
├── pax/          
├── pkill/
├── ps/           
├── pwait/        
├── pwd/
├── realpath/     
├── rm/           
├── rmail/
├── rmdir/        
├── setfacl/      
├── sh/
├── sleep/        
├── stty/         
├── sync/
├── test/         
├── timeout/      
└── uuidgen/
```

Each utility subdirectory contains its own `GNUmakefile` and source files.

---

## docs/ — Documentation

```
docs/
└── handbook/               # Developer handbook
    ├── Project-Tick/       # Organization-level docs
    └── [per-project]/      # Per-sub-project documentation
```

---

## archived/ — Deprecated Projects

```
archived/
├── projt-launcher/         # Original launcher (GPL-3.0-only)
├── projt-modpack/          # Modpack tooling (GPL-3.0-only)
├── projt-minicraft-modpack/ # Minicraft modpack (MIT)
└── ptlibzippy/             # ZIP library (Zlib)
```

These projects are kept for historical reference but are no longer actively
maintained. MeshMC supersedes projt-launcher, and neozip supersedes
ptlibzippy.
