/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: Project Tick
 *
 * mangohud_detect.h — Compile-time / runtime detection helpers for MangoHud.
 *
 * Vendored / authored for MeshMC LinuxPerf plugin.
 *
 * MangoHud (https://github.com/flightlessmango/MangoHud) does not expose a
 * public C API header for launchers. Integration is exclusively via:
 *
 *   1. Wrapper command:   mangohud <executable> [args...]
 *      MangoHud uses LD_PRELOAD internally to hook into OpenGL / Vulkan.
 *
 *   2. Environment variables (set before the child process starts):
 *        MANGOHUD=1              — enable the overlay (Vulkan; implied by wrapper)
 *        MANGOHUD_DLSYM=1        — enable dlsym hooking (needed for some OpenGL games;
 *                                  default-enabled in MangoHud ≥ 1.3)
 *        MANGOHUD_CONFIG=<csv>   — comma-separated config key=value pairs
 *        MANGOHUD_CONFIGFILE=<p> — path to a MangoHud config file
 *                                  (useful for Java apps whose process name is "java")
 *
 * For Minecraft / LWJGL (OpenGL or Vulkan backend) the recommended invocation
 * is to prepend "mangohud" as a wrapper command.  MangoHud will then inject
 * itself via LD_PRELOAD and hook into whichever graphics API LWJGL opens.
 *
 * This header provides a lightweight runtime availability check that the
 * LinuxPerf plugin uses to disable the MangoHud checkbox when the binary is
 * not found on PATH, and to detect the Flatpak sandboxed variant.
 */

#pragma once

#ifdef __cplusplus
#include <cstdlib>

namespace mangohud_detect {

/*
 * Returns true when `mangohud` is findable on PATH.
 * Uses a fast execvp-safe PATH walk via QStandardPaths on the Qt side, or
 * falls back to popen("command -v mangohud") — callers should prefer the Qt
 * QStandardPaths overload provided in LinuxPerfPlugin.cpp.
 */
inline bool is_available_on_path()
{
	/* Implemented in LinuxPerfPlugin.cpp via QStandardPaths::findExecutable. */
	return false; /* placeholder — never call this directly */
}

/*
 * Returns true when running inside a Flatpak sandbox (/.flatpak-info exists).
 * In that case the correct MangoHud layer is
 *   org.freedesktop.Platform.VulkanLayer.MangoHud
 * and it is already pre-loaded by the Flatpak runtime; no wrapper is needed.
 */
inline bool is_flatpak()
{
#if defined(__linux__)
	struct stat st;
	return (stat("/.flatpak-info", &st) == 0);
#else
	return false;
#endif
}

/*
 * Key environment variables injected by the LinuxPerf plugin when MangoHud
 * is enabled via the wrapper command path.  Defined here so they have a
 * single canonical source.
 */
static constexpr const char *ENV_MANGOHUD        = "MANGOHUD";
static constexpr const char *ENV_MANGOHUD_DLSYM  = "MANGOHUD_DLSYM";

/*
 * Value "1" used for the boolean env vars above.
 */
static constexpr const char *ENV_VALUE_ON  = "1";
static constexpr const char *ENV_VALUE_OFF = "0";

} /* namespace mangohud_detect */

#endif /* __cplusplus */
