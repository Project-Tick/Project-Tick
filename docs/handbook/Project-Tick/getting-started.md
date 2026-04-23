# Project Tick — Getting Started

## Prerequisites

Before working with Project Tick, ensure you have the following base tools
installed on your system:

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| Git | 2.30+ | Source control, submodule management |
| CMake | 3.20+ | Build system for C/C++ projects |
| Ninja | 1.10+ | Fast build backend for CMake |
| C++ compiler | GCC 13+ / Clang 17+ / MSVC 19.36+ | C++23 compilation |
| C compiler | GCC 13+ / Clang 17+ / MSVC 19.36+ | C11/C23 compilation |
| pkg-config | any | Library discovery |
| Python | 3.10+ | meta/ component |
| Rust | stable | tickborg CI bot |
| JDK | 17+ | ForgeWrapper, Minecraft runtime |
| Node.js | 22+ | CI scripts |

### Optional but Recommended

| Tool | Purpose |
|------|---------|
| Nix | Reproducible builds, development shells |
| Go | Installing lefthook |
| lefthook | Git hooks manager |
| reuse | REUSE license compliance checking |
| clang-format | Code formatting |
| clang-tidy | Static analysis |
| npm | CI script dependencies |
| Docker/Podman | Container-based builds |
| scdoc | Man page generation |

---

## Cloning the Repository

Project Tick uses Git submodules. Always clone recursively:

```bash
git clone --recursive https://github.com/Project-Tick/Project-Tick.git
cd Project-Tick
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

The repository is large. If you only need a specific sub-project, you can
do a sparse checkout:

```bash
git clone --filter=blob:none --sparse https://github.com/Project-Tick/Project-Tick.git
cd Project-Tick
git sparse-checkout set meshmc json4cpp tomlplusplus libnbtplusplus neozip
git submodule update --init --recursive
```

---

## Bootstrap (Recommended First Step)

The fastest way to get a working development environment is to use the
bootstrap script. It detects your platform, installs missing dependencies,
initializes submodules, and sets up lefthook.

### Linux / macOS

```bash
./bootstrap.sh
```

The script supports the following distributions:

| Distribution | Package Manager | Detection |
|-------------|-----------------|-----------|
| Debian | apt | `/etc/os-release` ID |
| Ubuntu, Linux Mint, Pop!_OS | apt | `/etc/os-release` ID |
| Fedora | dnf | `/etc/os-release` ID |
| RHEL, CentOS, Rocky, Alma | dnf/yum | `/etc/os-release` ID |
| openSUSE, SLES | zypper | `/etc/os-release` ID |
| Arch, Manjaro, EndeavourOS | pacman | `/etc/os-release` ID |
| macOS | Homebrew | `uname -s` = Darwin |

The bootstrap script checks for:

- **Build tools:** npm, Go, lefthook, reuse
- **Libraries:** Qt6Core, quazip1-qt6, zlib, ECM (via pkg-config)

If any dependencies are missing, it installs them using the appropriate
package manager with `sudo`.

### Windows

```cmd
bootstrap.cmd
```

Uses [Scoop](https://scoop.sh) for CLI tools and
[vcpkg](https://github.com/microsoft/vcpkg) for C/C++ libraries.

### Nix

If you have Nix installed with flakes support:

```bash
nix develop
```

This drops you into a development shell with LLVM 22, clang-tidy, and all
necessary tooling. The shell hook automatically initializes submodules.

---

## Building MeshMC (Primary Application)

MeshMC is the main application in the Project Tick ecosystem. Here's how to
build it from source.

### Step 1: Install Dependencies

#### Debian / Ubuntu

```bash
sudo apt-get install \
    cmake ninja-build extra-cmake-modules pkg-config \
    qt6-base-dev libquazip1-qt6-dev zlib1g-dev \
    libcmark-dev libarchive-dev libqrencode-dev libtomlplusplus-dev \
    scdoc
```

#### Fedora

```bash
sudo dnf install \
    cmake ninja-build extra-cmake-modules pkgconf \
    qt6-qtbase-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

#### Arch Linux

```bash
sudo pacman -S --needed \
    cmake ninja extra-cmake-modules pkgconf \
    qt6-base quazip-qt6 zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

#### openSUSE

```bash
sudo zypper install \
    cmake ninja extra-cmake-modules pkg-config \
    qt6-base-devel quazip-qt6-devel zlib-devel \
    cmark-devel libarchive-devel qrencode-devel tomlplusplus-devel \
    scdoc
```

#### macOS (Homebrew)

```bash
brew install \
    cmake ninja extra-cmake-modules \
    qt@6 quazip zlib \
    cmark libarchive qrencode tomlplusplus \
    scdoc
```

### Step 2: Configure with CMake Presets

MeshMC ships `CMakePresets.json` with platform-specific presets:

```bash
cd meshmc

# Linux
cmake --preset linux

# macOS
cmake --preset macos

# macOS Universal Binary (x86_64 + arm64)
cmake --preset macos_universal

# Windows (MinGW)
cmake --preset windows_mingw

# Windows (MSVC)
cmake --preset windows_msvc
```

All presets use Ninja Multi-Config, output to `build/`, and install to
`install/`.

### Step 3: Build

```bash
# Using preset (matches the configure preset name)
cmake --build --preset linux

# Or manually with Ninja
cmake --build build --config Release
```

### Step 4: Install (Optional)

```bash
cmake --install build --config Release --prefix install
```

The built binary appears at `install/bin/meshmc`.

### Step 5: Run Tests

```bash
cd build
ctest --output-on-failure
```

### Building with Nix

```bash
cd meshmc
nix build
```

Or enter a development shell:

```bash
nix develop
cmake --preset linux
cmake --build --preset linux
```

### Building with Container (Podman/Docker)

MeshMC provides a `Containerfile`:

```bash
cd meshmc
podman build -t meshmc .
```

---

## Building Other Sub-Projects

### NeoZip (Compression Library)

```bash
cd neozip

# CMake build
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest --output-on-failure

# Or Autotools
./configure
make -j$(nproc)
make test
```

### cmark (Markdown Library)

```bash
cd cmark
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest --output-on-failure
```

### json4cpp (JSON Library)

```bash
cd json4cpp
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest --output-on-failure
```

json4cpp is header-only. For most uses, just include
`<nlohmann/json.hpp>` or `<nlohmann/json_fwd.hpp>` and point your
include path at `json4cpp/include/` or `json4cpp/single_include/`.

### tomlplusplus (TOML Library)

```bash
cd tomlplusplus

# Meson (primary)
meson setup build
ninja -C build
ninja -C build test

# Or CMake
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest --output-on-failure
```

toml++ is header-only. Include `<toml++/toml.hpp>` or use the single
header `toml.hpp`.

### libnbt++ (NBT Library)

```bash
cd libnbtplusplus
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

CMake options:
- `NBT_BUILD_SHARED=OFF` — Build static library (default)
- `NBT_USE_ZLIB=ON` — Enable zlib support for compressed NBT (default)
- `NBT_BUILD_TESTS=ON` — Build tests (default)

### GenQRCode (QR Code Library)

```bash
cd genqrcode

# Autotools
./autogen.sh
./configure
make -j$(nproc)
make check

# Or CMake
mkdir build && cd build
cmake .. -G Ninja
ninja
ctest --output-on-failure
```

### ForgeWrapper (Java)

```bash
cd forgewrapper
./gradlew build
```

The JAR is produced in `build/libs/`.

### CoreBinUtils (BSD Utilities)

```bash
cd corebinutils
./configure
make -f GNUmakefile -j$(nproc) all

# Run tests
make -f GNUmakefile test
```

Uses musl-first toolchain selection by default.

### MNV (Text Editor)

```bash
cd mnv

# CMake
mkdir build && cd build
cmake .. -G Ninja
ninja

# Or Autotools
./configure
make -j$(nproc)
```

### cgit (Git Web Interface)

```bash
cd cgit

# Initialize Git submodule (cgit bundles its own git)
git submodule init
git submodule update

make
sudo make install
```

Installs to `/var/www/htdocs/cgit` by default. Provide a `cgit.conf`
file to customize.

### Meta (Metadata Generator)

```bash
cd meta

# Install dependencies with Poetry
pip install poetry
poetry install

# Update Mojang versions
poetry run updateMojang

# Generate all metadata
poetry run generateMojang
poetry run generateForge
poetry run generateNeoForge
poetry run generateFabric
poetry run generateQuilt
poetry run generateJava
```

### tickborg (CI Bot)

```bash
cd ofborg/tickborg
cargo build
cargo test
cargo check
```

---

## Setting Up the Development Environment

### Git Hooks with Lefthook

After cloning, install lefthook to enable pre-commit and pre-push hooks:

```bash
# Install lefthook (if not already installed)
go install github.com/evilmartians/lefthook@latest

# Or via npm
npm i -g lefthook

# Install hooks in the repository
lefthook install
```

The hooks perform:

1. **Pre-commit:**
   - REUSE license compliance check (auto-downloads missing licenses)
   - checkpatch.pl on staged C/C++/CMake changes

2. **Pre-push:**
   - Final REUSE compliance check

### REUSE Compliance

Ensure every file has proper SPDX headers:

```bash
# Check compliance
reuse lint

# Download missing license texts
reuse download --all
```

### Code Formatting

MeshMC uses clang-format for C/C++ formatting:

```bash
# Format a file
clang-format -i path/to/file.cpp

# Check formatting (CI style)
clang-format --dry-run --Werror path/to/file.cpp
```

The CI system uses `treefmt` with biome (JavaScript), nixfmt (Nix), and
yamlfmt (YAML) for other file types.

### IDE Setup

#### VS Code

Recommended extensions:
- C/C++ (ms-vscode.cpptools)
- CMake Tools (ms-vscode.cmake-tools)
- clangd (llvm-vs-code-extensions.vscode-clangd)

MeshMC generates `compile_commands.json` via
`CMAKE_EXPORT_COMPILE_COMMANDS ON` for full IDE support.

#### CLion

Open the `meshmc/CMakeLists.txt` directly. CLion natively supports CMake
presets — select the appropriate platform preset.

#### Vim/MNV

Use the `compile_commands.json` with a language server like `clangd` or
`ccls`.

---

## First Contribution Workflow

1. **Fork** the repository on GitHub
2. **Clone** your fork:
   ```bash
   git clone --recursive https://github.com/YOUR_USERNAME/Project-Tick.git
   ```
3. **Create a branch:**
   ```bash
   git checkout -b feature/my-change
   ```
4. **Make your changes**
5. **Format and lint:**
   ```bash
   clang-format -i changed_files.cpp
   reuse lint
   ```
6. **Commit with sign-off and conventional format:**
   ```bash
   git commit -s -a -m "feat(meshmc): add new feature description"
   ```
7. **Push and create a PR:**
   ```bash
   git push origin feature/my-change
   ```
8. Open a pull request against the `master` branch

See [contributing.md](contributing.md) for detailed contribution guidelines.

---

## Troubleshooting

### CMake can't find Qt 6

Ensure Qt 6 is installed and discoverable:

```bash
# Check if Qt6Core is available
pkg-config --modversion Qt6Core

# If using a custom Qt installation, set CMAKE_PREFIX_PATH
cmake --preset linux -DCMAKE_PREFIX_PATH=/path/to/qt6
```

### Submodules are empty

```bash
git submodule update --init --recursive --force
```

### Build fails on WSL

MeshMC explicitly blocks WSL builds in its CMakeLists.txt:

```
Building MeshMC is not supported in Linux-on-Windows distributions.
```

Build natively on Windows using the `windows_msvc` or `windows_mingw` preset
instead.

### In-source build error

MeshMC enforces out-of-source builds. If you see this error:

```
You are building MeshMC in-source. Please separate the build tree from the source tree.
```

Create a separate build directory:

```bash
cd meshmc
cmake --preset linux   # Uses build/ automatically
```

### Missing ECM (Extra CMake Modules)

Install the ECM package for your distribution:

```bash
# Debian/Ubuntu
sudo apt-get install extra-cmake-modules

# Fedora
sudo dnf install extra-cmake-modules

# Arch
sudo pacman -S extra-cmake-modules
```

### Nix build fails

Ensure you have flakes enabled:

```bash
# Check Nix version
nix --version

# Enable flakes (if not already)
echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf
```

### Poetry not found

```bash
pip install poetry
# Or
pipx install poetry
```

### Rust/Cargo not found

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env
```

### cgit build fails (missing Git submodule)

```bash
cd cgit
git submodule init
git submodule update
# Or download manually:
make get-git
```
