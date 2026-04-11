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

use App\Repository\SiteSettingRepository;

class ModuleService
{
    private SiteSettingRepository $settings;
    private string $projectDir;

    public function __construct(SiteSettingRepository $settings, string $projectDir)
    {
        $this->settings = $settings;
        $this->projectDir = $projectDir;
    }

    /**
     * Get detailed information about the current license status.
     */
    public function getLicenseInfo(): array
    {
        $info = [
            'status' => 'COMMUNITY_MODE',
            'payload' => null,
            'type' => 'COMMUNITY',
            'year' => null,
            'tier' => 'OPEN-SOURCE',
            'domain' => null,
        ];

        // 1. Local Dev Exemption
        if (isset($_SERVER['APP_ENV']) && $_SERVER['APP_ENV'] === 'dev') {
            $info['status'] = 'DEV_MODE';
            $info['type'] = 'ENTERPRISE';
            return $info;
        }

        // 2. SECURE License File Check (RSA Signed)
        // We check this first to let the admin dashboard display the actual license info if it's there.
        $licensePath = $this->projectDir . '/LICENSE_KEY';
        if (file_exists($licensePath)) {
            $info['status'] = 'INVALID_SIGNATURE';
            $rawKey = trim(file_get_contents($licensePath));
            
            if (str_contains($rawKey, '.')) {
                [$payload, $signatureBase64] = explode('.', $rawKey, 2);
                $signature = base64_decode($signatureBase64);
                
                // --- HARD CODED PUBLIC KEY ---
                $publicKey = "-----BEGIN PUBLIC KEY-----\n" .
                    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj1yMXn9G56PLnW4vrF0A\n" .
                    "1GXAZ6co3csElpLyFKGIqXmePen7OsD2neW5ljxNDI2o9s9sFotfXp/g+UVXcEYM\n" .
                    "Dcnx91WwXHsq3Fwep9NdaF8r+asfh/DNElo0RrrUKjIDC3d/bADklqJ1lsSUYTYP\n" .
                    "ZnfVcG/eC0A3HaP0ymztOefOJmIFEx1wluHFlJbx3uzGIrWLZTeJE3Tu+rCwhEj2\n" .
                    "UYxKbfmFHvhGu+Eln8KIYJI+IFRWFjNtBpU8LUyAva/lFedwLowgy7OLNkl2pjKd\n" .
                    "ovQyCMcxDZG+Vdj4EPkMFP7cHUT7CB3zohTTjZ8719/ojZGVUGDESqew0Nqr6olM\n" .
                    "zwIDAQAB\n" .
                    "-----END PUBLIC KEY-----";

                if (openssl_verify($payload, $signature, $publicKey, OPENSSL_ALGO_SHA256) === 1) {
                    if (str_starts_with($payload, 'PT-INTERNAL-PRO')) {
                        $parts = explode('-', $payload);
                        $info['type'] = $parts[2] ?? 'PRO';
                        $info['year'] = $parts[3] ?? '2026';
                        $info['tier'] = $parts[4] ?? 'OFFICIAL';
                        $info['domain'] = $parts[5] ?? null;
                        
                        // Domain Verification Check (Ignore CLI context if testing)
                        if ($info['domain'] !== null && php_sapi_name() !== 'cli') {
                            $currentHost = $_SERVER['HTTP_HOST'] ?? $_SERVER['SERVER_NAME'] ?? '';
                            // Strip port from host
                            $currentDomain = explode(':', $currentHost)[0];
                            
                            if (strtolower($currentDomain) !== strtolower($info['domain'])) {
                                $info['status'] = 'INVALID_DOMAIN';
                                return $info;
                            }
                        }

                        $info['status'] = 'VERIFIED';
                        $info['payload'] = $payload;
                    } else {
                        $info['status'] = 'INVALID_PREFIX';
                    }
                }
            }
        }

        // 3. Fallback to "Ping" Check: Verify if we are running on the official infra
        // If the license file is missing or invalid, we still allow features on the main server.
        // We verify this by checking if the server's actual IP matches the master IP.
        if (!in_array($info['status'], ['VERIFIED', 'DEV_MODE'])) {
            $masterIp = '152.53.231.231';
            $localIp = $_SERVER['SERVER_ADDR'] ?? '';
            
            // In some environments, SERVER_ADDR might not be set. Alternatively, we strictly rely on the Domain Check that happens earlier.
            if ($localIp === $masterIp || file_exists('/var/www/Project-Tick/website/projt-website/config/jwt/projt_private.pem')) {
                // We securely assume it's official infra ONLY if the local IP matches or the private key is physically present.
                $info['status'] = 'OFFICIAL_INFRA';
                $info['type'] = 'ENTERPRISE';
            }
        }

        return $info;
    }

    /**
     * Deprecated: Use getLicenseInfo instead.
     */
    public function isAuthorized(): bool
    {
        $info = $this->getLicenseInfo();
        return in_array($info['type'], ['PRO', 'ENTERPRISE', 'SAAS', 'OFFICIAL']);
    }

    public function getVersion(): string
    {
        $versionPath = $this->projectDir . '/VERSION';
        if (file_exists($versionPath)) {
            return trim(file_get_contents($versionPath));
        }
        return '0.0.0-UNKNOWN';
    }

    public function isEnabled(string $module): bool
    {
        $module = strtolower($module);

        // 1. OS Integrity Check: ONLY RHEL-BASED SYSTEMS ALLOWED
        if (!$this->checkOsIntegrity()) {
            return false;
        }

        // 2. Code Integrity Check: If the core files for the module are missing, it's HARD DISABLED.
        if (!$this->checkFileIntegrity($module)) {
            return false;
        }

        // 3. Enforce Authorization: Enterprise Modules vs Community Modules
        $info = $this->getLicenseInfo();
        $isEnterprise = in_array($info['type'], ['PRO', 'ENTERPRISE', 'SAAS']);
        
        $enterpriseOnlyModules = ['monitor', 'license_keys'];
        
        if (in_array($module, $enterpriseOnlyModules) && !$isEnterprise) {
            return false;
        }

        // 4. User Toggle Check (Database)
        $key = 'module_' . $module;
        $value = $this->settings->getValue($key, 'true');

        return filter_var($value, FILTER_VALIDATE_BOOLEAN);
    }

    /**
     * Physical check: ONLY RHEL-based systems (RHEL, Alma, Rocky, Centos, Fedora) are permitted.
     */
    private function checkOsIntegrity(): bool
    {
        // Local Dev Exemption
        if (isset($_SERVER['APP_ENV']) && $_SERVER['APP_ENV'] === 'dev') {
            return true;
        }

        if (!file_exists('/etc/os-release')) {
            return false;
        }

        $osInfo = file_get_contents('/etc/os-release');
        $isRhel = false;
        
        $rhelIdentifiers = ['rhel', 'centos', 'rocky', 'almalinux', 'fedora'];
        foreach ($rhelIdentifiers as $id) {
            if (preg_match("/^ID(_LIKE)?=.*$id/m", $osInfo)) {
                $isRhel = true;
                break;
            }
        }

        // Additional check for legendary /etc/redhat-release
        if (!$isRhel && file_exists('/etc/redhat-release')) {
            $isRhel = true;
        }

        return $isRhel;
    }

    /**
     * Physical check: Does the code even exist for this module?
     * Supports both legacy single-file checks and NEW Submodule-based check in src/Module/
     */
    private function checkFileIntegrity(string $module): bool
    {
        // 1. New Submodule Structure Check
        $submoduleDir = $this->projectDir . '/src/Module/' . $this->normalizeModuleName($module);
        if (is_dir($submoduleDir)) {
            // Check if directory is not empty (contains more than just . and ..)
            $files = scandir($submoduleDir);
            if (count($files) > 2) {
                return true;
            }
        }

        // 2. Legacy File Check (Fallback)
        $map = [
            'monitor'       => 'src/Module/Monitor/UptimeService.php',
            'community'     => 'src/Controller/DiscussionController.php',
        ];

        if (!isset($map[$module])) {
            return true;
        }

        return file_exists($this->projectDir . '/' . $map[$module]);
    }

    private function normalizeModuleName(string $module): string
    {
        // converts github_bot to GithubBot
        return str_replace(' ', '', ucwords(str_replace('_', ' ', $module)));
    }

    public function isMonitorEnabled(): bool
    {
        return $this->isEnabled('monitor');
    }

    public function isCommunityEnabled(): bool
    {
        return $this->isEnabled('community');
    }
}
