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

namespace App\EventListener;

use App\Entity\Visit;
use App\Entity\User;
use Doctrine\ORM\EntityManagerInterface;
use Symfony\Component\EventDispatcher\Attribute\AsEventListener;
use Symfony\Component\Security\Core\Authentication\Token\Storage\TokenStorageInterface;

#[AsEventListener(event: 'kernel.terminate')]
class VisitListener
{
    public function __construct(
        private EntityManagerInterface $entityManager,
        private TokenStorageInterface $tokenStorage
    ) {}

    public function onKernelTerminate(\Symfony\Component\HttpKernel\Event\TerminateEvent $event): void
    {
        $request = $event->getRequest();
        $userAgent = $request->headers->get('User-Agent');
        $path = $request->getPathInfo();

        // Skip internal/debug requests and feeds
        if (str_starts_with($path, '/_') || str_starts_with($path, '/admin') || str_starts_with($path, '/api') || str_ends_with($path, '.xml')) {
            return;
        }

        $visit = new Visit();
        $visit->setUrl($path);
        $visit->setIp($request->getClientIp());
        
        // Simple User Agent Parsing
        $visit->setBrowser($this->parseBrowser($userAgent));
        $visit->setOs($this->parseOs($userAgent));

        // Safely check for user only if authentication already happened
        // Avoid triggering lazy-auth/session init during terminate (headers already sent)
        try {
            $token = $this->tokenStorage->getToken();
            if ($token && $token->getUser() instanceof User) {
                $visit->setUser($token->getUser());
            }
        } catch (\Throwable $e) {
            // Session/auth not available (e.g. headers already sent for static responses)
        }

        $this->entityManager->persist($visit);
        $this->entityManager->flush();
    }

    private function parseBrowser(?string $ua): string
    {
        if (!$ua) return 'Unknown';
        if (str_contains($ua, 'Firefox')) return 'Firefox';
        if (str_contains($ua, 'Chrome')) return 'Chrome';
        if (str_contains($ua, 'Safari')) return 'Safari';
        if (str_contains($ua, 'Edge')) return 'Edge';
        return 'Other';
    }

    private function parseOs(?string $ua): string
    {
        if (!$ua) return 'Unknown';
        if (str_contains($ua, 'Windows')) return 'Windows';
        if (str_contains($ua, 'Macintosh')) return 'macOS';
        if (str_contains($ua, 'Linux')) return 'Linux';
        if (str_contains($ua, 'Android')) return 'Android';
        if (str_contains($ua, 'iPhone')) return 'iOS';
        return 'Other';
    }
}
