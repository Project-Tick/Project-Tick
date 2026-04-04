# Project Tick

[![CI](https://github.com/Project-Tick/Project-Tick/actions/workflows/meshmc-build.yml/badge.svg)](https://github.com/Project-Tick/Project-Tick/actions)
[![License](https://img.shields.io/badge/license-mixed-blue)](LICENSES/)
[![CLA assistant](https://cla-assistant.io/readme/badge/Project-Tick/Project-Tick)](https://cla-assistant.io/Project-Tick/Project-Tick)

Welcome to Project Tick.

This repository contains the source tree for Project Tick, a modular
ecosystem for building, packaging, and running software across multiple
platforms.

The project is organized as a unified monorepo with clearly separated
components. Each directory represents an independent module, library,
tool, or application that can be built and used standalone or as part
of the larger system.

Core areas include:
- System tools and utilities (`corebinutils`, `systeminfo`, `uvim`)
- Libraries (`ptlibzippy`, `json4cpp`, `tomlplusplus`, `libnbtplusplus`)
- Applications (`projt-launcher`, modpacks, tools)
- Infrastructure and integration (`meta`, `tools`, `dockerimages`)

Project Tick focuses on reproducible builds, minimal dependencies,
and full control over the software stack.

## Getting Started

Each component is self-contained. Refer to the individual directories
for build and usage instructions.

## Contributing

Contributions are welcome. Please follow project conventions and
component-level documentation when submitting changes.

## License

See the `LICENSES/` directory for detailed licensing information.
