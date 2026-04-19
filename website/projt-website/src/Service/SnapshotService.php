<?php

/*

SPDX-License-Identifier: MIT
SPDX-FileCopyrightText: 2026 Project Tick
SPDX-FileContributor: Project Tick

Copyright (c) 2026 Project Tick

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

namespace App\Service;

/**
 * Scans the FTP directory for snapshot component files and resolves the latest
 * snapshot release, producing a latest.json with download metadata.
 */
class SnapshotService
{
    private const FTP_ROOT = '/var/www/ftp/Project-Tick';
    private const DOWNLOAD_ROOT_URL = 'https://ftp.projecttick.org/Project-Tick';

    /**
     * Find all components-v*.json files and return the latest release tag.
     */
    public function findLatestSnapshotTag(): ?string
    {
        $pattern = self::FTP_ROOT . '/components-v*.json';
        $files = glob($pattern);

        if (empty($files)) {
            return null;
        }

        $tags = [];
        foreach ($files as $file) {
            $basename = basename($file);
            // Extract tag from "components-v202604191350.json"
            if (preg_match('/^components-(v\d+)\.json$/', $basename, $m)) {
                $tags[] = $m[1];
            }
        }

        if (empty($tags)) {
            return null;
        }

        // Sort descending by the numeric portion
        usort($tags, function (string $a, string $b): int {
            return strcmp($b, $a);
        });

        return $tags[0];
    }

    /**
     * Read and parse a components-v*.json file for a given release tag.
     *
     * @return array{schema_version: int, release_tag: string, release_date: string, components: array<string, array{version: string}>}|null
     */
    public function readComponentsJson(string $releaseTag): ?array
    {
        $path = self::FTP_ROOT . '/components-' . $releaseTag . '.json';
        if (!file_exists($path)) {
            return null;
        }

        $data = json_decode(file_get_contents($path), true);
        if (!is_array($data)) {
            return null;
        }

        return $data;
    }

    /**
     * List all component directories on the FTP that contain releases
     * for a given snapshot tag.
     *
     * @return array<string, array{name: string, has_release: bool, download_url: string|null}>
     */
    public function scanComponentDirectories(string $releaseTag): array
    {
        $result = [];
        $entries = scandir(self::FTP_ROOT);

        foreach ($entries as $entry) {
            if ($entry === '.' || $entry === '..') {
                continue;
            }

            $fullPath = self::FTP_ROOT . '/' . $entry;
            if (!is_dir($fullPath)) {
                continue;
            }

            $releaseDir = $fullPath . '/releases/download/' . $releaseTag;
            $hasRelease = is_dir($releaseDir);

            $result[$entry] = [
                'name' => $entry,
                'has_release' => $hasRelease,
                'download_url' => $hasRelease
                    ? self::DOWNLOAD_ROOT_URL . '/' . $entry . '/releases/download/' . $releaseTag . '/'
                    : null,
            ];
        }

        return $result;
    }

    /**
     * Build the full latest.json structure for the latest snapshot.
     *
     * @return array|null
     */
    public function buildLatestJson(): ?array
    {
        $tag = $this->findLatestSnapshotTag();
        if ($tag === null) {
            return null;
        }

        $components = $this->readComponentsJson($tag);
        if ($components === null) {
            return null;
        }

        $dirs = $this->scanComponentDirectories($tag);

        // Build per-component download info
        $componentDownloads = [];
        foreach ($dirs as $dirName => $info) {
            if (!$info['has_release']) {
                continue;
            }

            $entry = [
                'download_url' => $info['download_url'],
            ];

            // If this component has version info in components.json, include it
            if (isset($components['components'][$dirName]['version'])) {
                $entry['version'] = $components['components'][$dirName]['version'];
            }

            // List actual files in the release directory
            $releaseDir = self::FTP_ROOT . '/' . $dirName . '/releases/download/' . $tag;
            $files = $this->listReleaseFiles($releaseDir, $dirName, $tag);
            if (!empty($files)) {
                $entry['files'] = $files;
            }

            $componentDownloads[$dirName] = $entry;
        }

        return [
            'schema_version' => 1,
            'release_tag' => $tag,
            'release_date' => $components['release_date'] ?? date('Y-m-d'),
            'components_json_url' => self::DOWNLOAD_ROOT_URL . '/components-' . $tag . '.json',
            'components' => $components['components'] ?? [],
            'downloads' => $componentDownloads,
        ];
    }

    /**
     * List release files (source archives only, skipping checksums/signatures)
     * for a component release directory.
     *
     * @return array<array{name: string, url: string, size: int}>
     */
    private function listReleaseFiles(string $releaseDir, string $componentName, string $tag): array
    {
        if (!is_dir($releaseDir)) {
            return [];
        }

        $files = [];
        $entries = scandir($releaseDir);

        foreach ($entries as $entry) {
            if ($entry === '.' || $entry === '..') {
                continue;
            }

            // Skip checksum, signature and zsync files
            if (str_ends_with($entry, '.sha256')
                || str_ends_with($entry, '.asc')
                || str_ends_with($entry, '.zsync')
            ) {
                continue;
            }

            $filePath = $releaseDir . '/' . $entry;
            if (!is_file($filePath)) {
                continue;
            }

            $files[] = [
                'name' => $entry,
                'url' => self::DOWNLOAD_ROOT_URL . '/' . $componentName . '/releases/download/' . $tag . '/' . $entry,
                'size' => filesize($filePath),
            ];
        }

        return $files;
    }

    /**
     * Write latest.json to the FTP root directory.
     */
    public function writeLatestJson(array $data): string
    {
        $path = self::FTP_ROOT . '/latest.json';
        $json = json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
        file_put_contents($path, $json . "\n");

        return $path;
    }
}
