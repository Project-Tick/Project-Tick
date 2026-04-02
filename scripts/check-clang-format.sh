#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
file_list="$repo_root/.clang-format-files"

discover_files() {
	if git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
		git -C "$repo_root" ls-files '*.c' '*.h' '*.cpp'
	else
		rg --files "$repo_root" -g '*.c' -g '*.h' -g '*.cpp'
	fi
}

if [[ -f "$file_list" ]]; then
	mapfile -t files < <(grep -Ev '^(#|$)' "$file_list")
	if [[ ${#files[@]} -eq 0 ]]; then
		mapfile -t files < <(discover_files)
	fi
else
	mapfile -t files < <(discover_files)
fi

if [[ ${#files[@]} -eq 0 ]]; then
	echo "No C/C++ source files found for clang-format checks" >&2
	exit 1
fi

for file in "${files[@]}"; do
	if [[ "$file" = "$repo_root"/* ]]; then
		candidate="$file"
	else
		candidate="$repo_root/$file"
	fi
	if [[ ! -f "$candidate" ]]; then
		echo "Configured clang-format file not found: $file" >&2
		exit 1
	fi
done

cd "$repo_root"
clang-format --dry-run --Werror --style=file "${files[@]}"