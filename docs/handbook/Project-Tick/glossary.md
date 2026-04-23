# Project Tick — Glossary

## A

### Adler-32
A checksum algorithm designed by Mark Adler, used in the zlib data format.
neozip provides SIMD-accelerated implementations of Adler-32 via architecture-
specific intrinsics (SSE4.2, AVX2, NEON, VMX).

### AMQP (Advanced Message Queuing Protocol)
A wire-level protocol for message-oriented middleware. tickborg uses AMQP
(via RabbitMQ) to receive build requests from GitHub webhooks and dispatch
them to worker nodes.

### AppImage
A portable Linux application format that bundles all dependencies into a
single executable file. MeshMC distributes Linux releases as AppImages.

### Autotools
A build system suite (autoconf, automake, libtool) used by several
sub-projects including genqrcode. The `./autogen.sh` script bootstraps
the build, producing `configure` and `Makefile.in`.

### AUR (Arch User Repository)
A community-driven repository for Arch Linux packages. MeshMC publishes a
PKGBUILD to the AUR for Arch users.

### AVX2 / AVX-512
Advanced Vector Extensions — x86 SIMD instruction sets providing 256-bit and
512-bit vector operations. neozip uses AVX2 for accelerated CRC-32, Adler-32,
and deflate hash chain insertion.

---

## B

### Bazel
An open-source build system from Google. json4cpp provides `BUILD.bazel` and
`MODULE.bazel` files for Bazel-based builds as an alternative to CMake.

### BSD License
A permissive open-source license family. Project Tick uses BSD-1-Clause,
BSD-2-Clause, BSD-3-Clause, and BSD-4-Clause across various components,
primarily in corebinutils (FreeBSD-derived code).

### BSL-1.0 (Boost Software License)
A permissive license used by some utility code in the monorepo.

---

## C

### Cargo
The Rust package manager and build system. tickborg uses a Cargo workspace
with two crates: `tickborg` and `tickborg-simple-build`.

### CC-BY-SA-4.0
Creative Commons Attribution-ShareAlike 4.0 International. Used for
documentation content within the Project Tick monorepo.

### CC0-1.0
Creative Commons Zero. A public domain dedication used for trivial
configuration files and metadata.

### CGI (Common Gateway Interface)
A protocol for web servers to execute programs and return their output. cgit
is a CGI application that generates HTML from Git repositories.

### cgit
A fast, lightweight web interface for Git repositories, written in C. The
Project Tick fork includes UI customizations and is linked against a bundled
Git source tree.

### CLA (Contributor License Agreement)
A legal agreement between a contributor and the project. Project Tick uses
PT-CLA-2.0, which must be signed before contributions are accepted.

### Clang
The C/C++ compiler from the LLVM project. MeshMC requires Clang 18+ or
equivalent. The Nix development shell provides LLVM 22 (Clang 22).

### clang-format
An automatic C/C++ code formatter. MeshMC's `.clang-format` defines the
project's formatting rules, enforced by CI.

### clang-tidy
A C/C++ static analysis tool. MeshMC's `.clang-tidy` configures enabled
checks. CI runs clang-tidy as part of the lint stage.

### CMake
A cross-platform build system generator. The primary build system for MeshMC,
neozip, json4cpp, libnbtplusplus, genqrcode, cmark, and MNV. Project Tick
requires CMake 3.20+ for MeshMC.

### CMake Presets
A JSON-based configuration file (`CMakePresets.json`) that defines named sets
of CMake configure, build, and test options. MeshMC uses presets for each
target platform.

### CODEOWNERS
A GitHub feature that maps file paths to responsible reviewers. Project Tick's
`.github/CODEOWNERS` routes all reviews to `@YongDo-Hyun`.

### CodeQL
GitHub's semantic code analysis engine for finding security vulnerabilities.
Configured in `.github/codeql/` for C, C++, and Java scanning.

### cmark
A standard-compliant CommonMark Markdown parser written in C. Licensed under
BSD-2-Clause. Provides both a library and a CLI tool.

### CommonMark
A strongly-defined specification for Markdown syntax. cmark is the reference
C implementation of the CommonMark spec.

### Conventional Commits
A commit message convention: `type(scope): description`. The CI lint stage
(`ci/github-script/lint-commits.js`) enforces this convention.

### corebinutils
A collection of core Unix utilities ported from FreeBSD. Provides minimal
implementations of commands like `cat`, `ls`, `cp`, `mv`, `rm`, `mkdir`,
`chmod`, `echo`, `kill`, `ps`, and 30+ others.

### Coverity
A commercial static analysis tool. Some sub-projects include Coverity scan
integration in their CI workflows.

### CRC-32
Cyclic Redundancy Check with a 32-bit output. Used in gzip, PNG, and other
formats. neozip provides SIMD-accelerated CRC-32 using PCLMULQDQ (x86),
PMULL (ARM), and hardware instructions on s390x.

### Crowdin
A localization management platform. MeshMC uses Crowdin for translation
management (`launcher/translations/`).

### CurseForge
A Minecraft mod hosting platform. MeshMC integrates with CurseForge for mod
discovery and installation via `launcher/modplatform/`.

---

## D

### DCO (Developer Certificate of Origin)
A lightweight alternative to a CLA. Contributors certify their right to
submit code by adding `Signed-off-by:` to commit messages (`git commit -s`).
Enforced by `.github/dco.yml`.

### Deflate
A lossless compression algorithm combining LZ77 and Huffman coding. neozip
provides multiple deflate strategies: fast, medium, slow (best), quick,
huffman-only, RLE, and stored.

### DFLTCC (Deflate Conversion Call)
A hardware instruction on IBM z15+ mainframes (s390x) that performs deflate
compression/decompression in hardware. neozip supports DFLTCC via
`arch/s390/`.

### direnv
A shell extension that loads environment variables from `.envrc` files.
Project Tick uses direnv with Nix (`use flake`) for automatic development
environment activation.

### Docker / Containerfile
Container image build specifications. images4docker provides 40 Dockerfiles
for CI build environments. MeshMC includes a `Containerfile` for container
builds.

---

## E

### ECM (Extra CMake Modules)
A set of additional CMake modules provided by the KDE project. Required as a
build dependency for MeshMC.

---

## F

### Fabric
A lightweight Minecraft mod loader. MeshMC supports Fabric modding via
`launcher/modplatform/`. The meta generator produces Fabric version metadata
(`generate_fabric.py`).

### Flake (Nix)
A Nix feature that provides reproducible, hermetic project definitions. The
root `flake.nix` defines the development shell with LLVM 22, Qt 6, and all
build dependencies.

### Flatpak / Flathub
A Linux application sandboxing and distribution system. MeshMC is published
on Flathub after release.

### Forge (Minecraft Forge)
A Minecraft mod loader for modifying the game. forgewrapper provides a Java
shim using SPI (Service Provider Interface) for Forge's boot process.

### ForgeWrapper
A Java library that uses JPMS (Java Platform Module System) and SPI to
integrate with Minecraft Forge's installer/detector mechanism. Located at
`forgewrapper/`.

### FreeBSD
A Unix-like operating system. corebinutils contains utilities ported from
FreeBSD's coreutils.

### Fuzz Testing
A testing technique that provides random/malformed inputs to find crashes and
vulnerabilities. neozip, cmark, meta, and tomlplusplus include fuzz testing
targets.

---

## G

### Garnix
A CI platform for Nix projects. The meta sub-project uses Garnix
(`meta/garnix.yaml`).

### genqrcode
A C library for generating QR codes. Supports all QR code versions (1–40),
multiple error correction levels (L/M/Q/H), and various encoding modes
(numeric, alphanumeric, byte, kanji, ECI).

### GHCR (GitHub Container Registry)
GitHub's container image registry. images4docker images are pushed to GHCR.

### GPL (GNU General Public License)
A copyleft license family. Project Tick uses GPL-2.0-only (cgit),
GPL-3.0-only (archived projects), and GPL-3.0-or-later (MeshMC,
images4docker).

### Gradle
A build automation tool for JVM projects. forgewrapper uses Gradle with the
Gradle Wrapper (`gradlew`).

---

## H

### Huffman Coding
A lossless data compression algorithm using variable-length codes. neozip's
`trees.c` implements Huffman tree construction for the deflate algorithm.

---

## I

### images4docker
A collection of 40 Dockerfiles providing CI build environments for every
supported Linux distribution. Qt 6 is a mandatory dependency in all images.
Images are rebuilt daily at 03:17 UTC.

---

## J

### JPMS (Java Platform Module System)
Introduced in Java 9 (Project Jigsaw), JPMS provides a module system for
Java. forgewrapper uses JPMS configuration (`jigsaw/`) for proper module
encapsulation.

### json4cpp
A fork of nlohmann/json — a header-only JSON library for C++. Licensed under
MIT. Provides SAX and DOM parsing, serialization, JSON Pointer, JSON Patch,
JSON Merge Patch, and CBOR/MessagePack/UBJSON/BSON support.

---

## L

### lefthook
A fast, cross-platform Git hooks manager. Configured in `lefthook.yml` to
run REUSE lint and checkpatch on pre-commit.

### LGPL (GNU Lesser General Public License)
A copyleft license that permits linking from proprietary code.
libnbtplusplus uses LGPL-3.0, genqrcode uses LGPL-2.1.

### libnbtplusplus
A C++ library for reading and writing Minecraft's NBT (Named Binary Tag)
format. Used by MeshMC to parse and modify Minecraft world data.

### LLVM
A compiler infrastructure providing Clang, LLD, and other tools. Project
Tick's Nix development shell provides LLVM 22.

### Lua
A lightweight scripting language. cgit uses Lua for content filtering
(`filter.c`).

### LZ77
A lossless compression algorithm that replaces repeated occurrences with
references (length, distance pairs). The foundation of the deflate algorithm
implemented in neozip.

---

## M

### Make (GNU Make)
A build automation tool. Used by cgit, corebinutils, and cmark. cgit uses
a plain Makefile, while corebinutils uses GNUmakefile.

### MeshMC
The primary application in the Project Tick ecosystem. A custom Minecraft
launcher written in C++23 with Qt 6, supporting multiple mod loaders,
instance management, and cross-platform deployment.

### Meson
A build system focused on speed and simplicity. tomlplusplus uses Meson as
its primary build system.

### MIT License
A permissive open-source license. Used by json4cpp, tomlplusplus,
forgewrapper, tickborg, and archived/projt-minicraft-modpack.

### MNV
A fork of the Vim text editor with modern enhancements. Written in C, built
with CMake or Autotools.

### Modrinth
A Minecraft mod hosting platform. MeshMC integrates with Modrinth for mod
discovery and installation.

### Mojang
The developer of Minecraft. meta generates Mojang version metadata
(`generate_mojang.py`).

### MS-PL (Microsoft Public License)
An open-source license used by the meta sub-project.

### MSVC (Microsoft Visual C++)
Microsoft's C/C++ compiler. MeshMC requires MSVC 17.10+ (Visual Studio 2022)
for Windows builds.

### musl
A lightweight C standard library implementation for Linux. Some neozip CI
builds test against musl for static linking compatibility.

---

## N

### NBT (Named Binary Tag)
A binary format used by Minecraft for storing structured data (worlds,
entities, items). libnbtplusplus provides C++ types for all NBT tag types:
`tag_byte`, `tag_short`, `tag_int`, `tag_long`, `tag_float`, `tag_double`,
`tag_string`, `tag_byte_array`, `tag_list`, `tag_compound`,
`tag_int_array`, `tag_long_array`.

### NEON
ARM's SIMD instruction set for 128-bit vector operations. neozip uses NEON
for accelerated CRC-32, Adler-32, and slide hash on AArch64.

### NeoForge
A community fork of Minecraft Forge. MeshMC supports NeoForge modding. The
meta generator produces NeoForge version metadata (`generate_neoforge.py`).

### neozip
Project Tick's fork of zlib-ng, a high-performance zlib replacement with
SIMD acceleration across x86, ARM, Power, s390x, RISC-V, and LoongArch
architectures. Licensed under the Zlib license.

### Nix
A purely functional package manager. Project Tick uses Nix flakes for
reproducible development environments, CI tooling, and package builds.

### nixpkgs
The Nix package collection. CI pins a specific nixpkgs revision in
`ci/pinned.json` for reproducible builds.

### NSIS (Nullsoft Scriptable Install System)
A Windows installer creation tool. MNV uses NSIS (`mnv/nsis/`) for Windows
distribution.

---

## O

### ofborg
The upstream project from which tickborg is forked. A CI system for the
Nixpkgs package repository that processes GitHub events via AMQP.

---

## P

### PCLMULQDQ
An x86 instruction for carry-less multiplication used to accelerate CRC-32
computation. neozip uses PCLMULQDQ via `arch/x86/`.

### PKGBUILD
An Arch Linux package build script. MeshMC maintains a PKGBUILD for AUR
distribution.

### PMULL
An ARM instruction for polynomial multiplication, used for CRC-32
acceleration on AArch64. neozip's ARM CRC implementation uses PMULL.

### Poetry
A Python dependency management and packaging tool. The meta sub-project uses
Poetry (`meta/pyproject.toml`, `meta/poetry.lock`).

### PR (Pull Request)
A GitHub mechanism for proposing code changes. All changes to protected
branches must go through PRs with passing CI and review approval.

---

## Q

### QR Code (Quick Response Code)
A two-dimensional barcode format. genqrcode generates QR codes supporting
versions 1–40, four error correction levels (L/M/Q/H), and multiple encoding
modes.

### Qt 6
A cross-platform application framework. MeshMC uses Qt 6 for its GUI
(widgets, dialogs, themes). Qt 6 is a mandatory dependency across all
images4docker build environments.

### Quilt
A Minecraft mod loader forked from Fabric. MeshMC supports Quilt modding. The
meta generator produces Quilt version metadata (`generate_quilt.py`).

---

## R

### RabbitMQ
An AMQP message broker. tickborg connects to RabbitMQ to receive build
requests dispatched from GitHub webhooks.

### Reed-Solomon
An error-correcting code used in QR codes. genqrcode implements Reed-Solomon
error correction in `rsecc.c`.

### Renovate
An automated dependency update bot. Configured in `meta/renovate.json`.

### REUSE
A specification from the FSFE (Free Software Foundation Europe) for
expressing license and copyright information. Project Tick's `REUSE.toml`
maps every file path to its SPDX license identifier.

### RISC-V
An open-source instruction set architecture. neozip includes RISC-V SIMD
optimizations via the RVV (Vector) and ZBC (Carry-less Multiply) extensions
in `arch/riscv/`.

---

## S

### Scoop
A Windows package manager. `bootstrap.cmd` uses Scoop to install
dependencies on Windows.

### Semgrep
A pattern-based static analysis tool for security scanning. Some CI workflows
include Semgrep scans.

### SemVer (Semantic Versioning)
A versioning scheme: `MAJOR.MINOR.PATCH`. MAJOR for breaking changes, MINOR
for backwards-compatible features, PATCH for bug fixes.

### SIMD (Single Instruction, Multiple Data)
A parallel processing technique. neozip heavily uses SIMD for performance-
critical operations: SSE2/SSE4.2/AVX2/AVX-512 (x86), NEON (ARM), VMX/VSX
(Power), DFLTCC (s390x), RVV (RISC-V), LSX/LASX (LoongArch).

### SPI (Service Provider Interface)
A Java API pattern for extensibility. forgewrapper uses SPI via
`IFileDetector.java` to integrate with Forge's installer mechanism.

### SPDX (Software Package Data Exchange)
A standard for communicating software license information. All Project Tick
licenses use SPDX identifiers. The `LICENSES/` directory contains full SPDX-
named license text files.

### SSE (Streaming SIMD Extensions)
x86 SIMD instruction sets (SSE2, SSE4.2). neozip uses SSE for baseline
SIMD acceleration on x86 platforms.

---

## T

### tickborg
Project Tick's CI bot, forked from ofborg. A Rust application that listens on
AMQP for build requests and executes them. Bot commands: `@tickbot build`,
`@tickbot test`, `@tickbot eval`.

### TOML (Tom's Obvious Minimal Language)
A configuration file format. tomlplusplus is a C++17 header-only TOML parser
and serializer supporting TOML v1.0.0.

### tomlplusplus
A header-only C++17 TOML library. Licensed under MIT. Provides parsing,
serialization, and manipulation of TOML documents. Built with Meson or CMake.

### treefmt
A universal code formatter dispatcher. Configured in `ci/default.nix` to run
all language-specific formatters in a single pass.

---

## U

### Unlicense
A public domain dedication license. Used for some trivial files in the
monorepo.

---

## V

### vcpkg
Microsoft's C/C++ package manager. MeshMC uses vcpkg for Windows dependency
management (`meshmc/vcpkg.json`, `meshmc/vcpkg-configuration.json`).

### Vim
A highly configurable text editor. MNV is a fork of Vim with additional
features. Licensed under the Vim license + GPL-3.0.

### VMX / VSX
IBM Power architecture SIMD instruction sets (Vector Multimedia Extension /
Vector Scalar Extension). neozip uses VMX/VSX for Power8/9 acceleration.

---

## W

### WSL (Windows Subsystem for Linux)
A compatibility layer for running Linux on Windows. Project Tick does **not**
support building under WSL; native Windows builds via MSVC are required.

---

## Z

### zlib
The original compression library implementing the deflate algorithm. neozip
is a high-performance fork of zlib-ng, which itself is a modernized fork of
zlib.

### zlib-ng
A modernized fork of zlib with SIMD optimizations. neozip is Project Tick's
fork of zlib-ng with additional modifications.

### Zlib License
A permissive open-source license. Used by neozip and archived/ptlibzippy.
