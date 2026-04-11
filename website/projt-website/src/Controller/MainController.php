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

namespace App\Controller;

use App\Entity\HandbookEntry;
use App\Entity\License;
use App\Entity\Product;
use App\Entity\News;
use App\Entity\LicenseCategory;
use Doctrine\ORM\EntityManagerInterface;
use Symfony\Bundle\FrameworkBundle\Controller\AbstractController;
use Symfony\Component\HttpFoundation\Response;
use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\Routing\Attribute\Route;

use App\Service\GithubService;

final class MainController extends AbstractController
{
    #[Route('/', name: 'app_home')]
    public function index(EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(Product::class)->findOneBy(['slug' => 'projt-launcher']);

        return $this->render('main/index.html.twig', [
            'product' => $product
        ]);
    }

    #[Route('/projtlauncher', name: 'app_launcher')]
    public function launcher(EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => 'projt-launcher']);
        
        $user = $this->getUser();
        $isLicensed = false;
        
        if ($product) {
            if ($product->isRequiresLicense()) {
                if ($user) {
                    $licenseCount = $em->getRepository(\App\Entity\UserLicense::class)->count([
                        'user' => $user,
                        'assignedProduct' => $product
                    ]);
                    $isLicensed = ($licenseCount > 0);
                }
            } else {
                $isLicensed = true;
            }
        }

        return $this->render('main/launcher.html.twig', [
            'product' => $product,
            'is_licensed' => $isLicensed
        ]);
    }

    #[Route('/p/meshmc', name: 'app_meshmc_launcher')]
    public function launcher(EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => 'meshmc']);
        
        $user = $this->getUser();
        $isLicensed = false;
        
        if ($product) {
            if ($product->isRequiresLicense()) {
                if ($user) {
                    $licenseCount = $em->getRepository(\App\Entity\UserLicense::class)->count([
                        'user' => $user,
                        'assignedProduct' => $product
                    ]);
                    $isLicensed = ($licenseCount > 0);
                }
            } else {
                $isLicensed = true;
            }
        }

        return $this->render('main/launcher.html.twig', [
            'product' => $product,
            'is_licensed' => $isLicensed
        ]);
    }

    #[Route('/p/{slug}', name: 'app_product_show')]
    public function productShow(string $slug, EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        
        if (!$product) {
            throw $this->createNotFoundException('Product not found');
        }

        $user = $this->getUser();
        $isLicensed = false;
        
        if ($product->isRequiresLicense()) {
            if ($user) {
                // Check if user has an assigned license for this specific product
                $licenseCount = $em->getRepository(\App\Entity\UserLicense::class)->count([
                    'user' => $user,
                    'assignedProduct' => $product
                ]);
                $isLicensed = ($licenseCount > 0);
            }
        } else {
            $isLicensed = true; // Free product
        }

        if ($product->isUseCustomLandingPage() && $product->getLandingPageContent()) {
            return $this->render('main/product_landing_custom.html.twig', [
                'product' => $product,
                'is_licensed' => $isLicensed
            ]);
        }

        return $this->render('main/launcher.html.twig', [
            'product' => $product,
            'is_licensed' => $isLicensed
        ]);
    }

    #[Route('/p/{slug}/download', name: 'app_product_download')]
    public function download(string $slug, EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        
        if (!$product || $product->getProductType() !== 'software') {
            throw $this->createNotFoundException('Download page not available for this product.');
        }

        $user = $this->getUser();
        if ($product->isRequiresLicense()) {
            if (!$user) {
                $this->addFlash('info', 'Please login to verify your license.');
                return $this->redirectToRoute('app_login');
            }
            
            $licenseCount = $em->getRepository(\App\Entity\UserLicense::class)->count([
                'user' => $user,
                'assignedProduct' => $product
            ]);
            
            if ($licenseCount === 0) {
                $this->addFlash('error', 'You do not have a valid license for this product.');
                return $this->redirectToRoute('app_product_show', ['slug' => $slug]);
            }
        }

        return $this->render('main/download.html.twig', [
            'product' => $product
        ]);
    }

    #[Route('/p/{slug}/download/{os}', name: 'app_product_download_os', requirements: ['os' => 'windows|macos|linux'])]
    public function downloadOs(string $slug, string $os, EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        
        if (!$product || $product->getProductType() !== 'software') {
            throw $this->createNotFoundException('Product not found');
        }

        // License check again for safety
        if ($product->isRequiresLicense()) {
            $user = $this->getUser();
            if (!$user) return $this->redirectToRoute('app_login');
            
            $licenseCount = $em->getRepository(\App\Entity\UserLicense::class)->count([
                'user' => $user,
                'assignedProduct' => $product
            ]);
            
            if ($licenseCount === 0) return $this->redirectToRoute('app_product_show', ['slug' => $slug]);
        }

        // Check if OS is enabled
        $isAllowed = false;
        if ($os === 'windows' && $product->isAllowWindows()) $isAllowed = true;
        if ($os === 'macos' && $product->isAllowMacos()) $isAllowed = true;
        if ($os === 'linux' && $product->isAllowLinux()) $isAllowed = true;

        if (!$isAllowed) {
            throw $this->createNotFoundException('Download for this platform is not available.');
        }

        return $this->render('main/download_os.html.twig', [
            'product' => $product,
            'os' => $os
        ]);
    }

    #[Route('/news', name: 'app_project_news')]
    public function projectNews(Request $request, EntityManagerInterface $em): Response
    {
        $query = $request->query->get('q');
        $repo = $em->getRepository(News::class);
        
        if ($query) {
            $news = $repo->createQueryBuilder('n')
                ->leftJoin('n.product', 'p')
                ->where('(n.title LIKE :query OR n.content LIKE :query)')
                ->andWhere('p.id IS NULL')
                ->andWhere('n.status = :status')
                ->setParameter('query', '%' . $query . '%')
                ->setParameter('status', News::STATUS_PUBLISHED)
                ->orderBy('n.date', 'DESC')
                ->getQuery()
                ->getResult();
        } else {
            $news = $repo->createQueryBuilder('n')
                ->leftJoin('n.product', 'p')
                ->where('p.id IS NULL')
                ->andWhere('n.status = :status')
                ->setParameter('status', News::STATUS_PUBLISHED)
                ->orderBy('n.date', 'DESC')
                ->getQuery()
                ->getResult();
        }

        return $this->render('main/project_news.html.twig', [
            'news_list' => $news,
            'search_query' => $query
        ]);
    }

    #[Route('/news/{slug}', name: 'app_project_news_show')]
    public function projectNewsShow(string $slug, EntityManagerInterface $em): Response
    {
        $post = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->leftJoin('n.product', 'p')
            ->where('n.slug = :slug')
            ->andWhere('p.id IS NULL')
            ->setParameter('slug', $slug)
            ->getQuery()
            ->getOneOrNullResult();

        if (!$post) {
            throw $this->createNotFoundException('News post not found');
        }
        return $this->render('main/project_news_show.html.twig', ['post' => $post]);
    }

    #[Route('/projtlauncher/news', name: 'app_news')]
    public function news(EntityManagerInterface $em): Response
    {
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->andWhere('n.status = :status')
            ->setParameter('slug', 'projt-launcher')
            ->setParameter('status', News::STATUS_PUBLISHED)
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        return $this->render('main/news.html.twig', [
            'news_list' => $news
        ]);
    }

    #[Route('/projtlauncher/news/{slug}', name: 'app_news_show')]
    public function newsShow(string $slug, EntityManagerInterface $em): Response
    {
        $post = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('n.slug = :slug')
            ->andWhere('p.slug = :productSlug')
            ->setParameter('slug', $slug)
            ->setParameter('productSlug', 'projt-launcher')
            ->getQuery()
            ->getOneOrNullResult();

        if (!$post) {
            throw $this->createNotFoundException('News post not found');
        }

        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => 'projt-launcher']);

        return $this->render('main/product_news_show.html.twig', [
            'post' => $post,
            'product' => $product
        ]);
    }

    private function mergeContributors(array $githubContribs, array $gitlabContribs): array
    {
        $merged = [];
        $normalize = function($str) {
            return preg_replace('/[^a-z0-9]/', '', strtolower(trim($str ?? '')));
        };

        // First process GitHub contribs
        foreach ($githubContribs as $contrib) {
            $key = $normalize($contrib['login'] ?? '');
            if (!$key) continue;

            $contrib['github_url'] = $contrib['html_url'] ?? '#';
            $merged[$key] = $contrib;
            if (!isset($merged[$key]['contributions'])) {
                $merged[$key]['contributions'] = 0;
            }
        }

        // Now process GitLab, check if they overlap with GitHub accounts
        foreach ($gitlabContribs as $contrib) {
            $nameKey = $normalize($contrib['name'] ?? '');
            $emailKey = $normalize($contrib['email_prefix'] ?? '');
            $loginKey = $normalize($contrib['login'] ?? '');
            
            $matchedKey = null;
            if ($nameKey && isset($merged[$nameKey])) $matchedKey = $nameKey;
            elseif ($emailKey && isset($merged[$emailKey])) $matchedKey = $emailKey;
            elseif ($loginKey && isset($merged[$loginKey])) $matchedKey = $loginKey;

            if ($matchedKey) {
                // It's a merged account (either GitHub+GitLab or duplicate GitLab)
                $merged[$matchedKey]['contributions'] += ($contrib['contributions'] ?? 0);
                if (!isset($merged[$matchedKey]['github_url'])) {
                    $merged[$matchedKey]['github_url'] = '#'; // In case it was a GitLab+GitLab merge
                } else {
                    $merged[$matchedKey]['is_merged'] = true; // It has a github_url, so it's a cross-platform merge
                }
                $merged[$matchedKey]['gitlab_url'] = $contrib['html_url'] ?? '#';
            } else {
                $newKey = $loginKey ?: ($nameKey ?: uniqid());
                if (isset($merged[$newKey])) {
                    $merged[$newKey]['contributions'] += ($contrib['contributions'] ?? 0);
                } else {
                    $contrib['gitlab_url'] = $contrib['html_url'] ?? '#';
                    $contrib['github_url'] = '#';
                    $merged[$newKey] = $contrib;
                }
            }
        }

        $result = array_values($merged);
        usort($result, function($a, $b) {
            return ($b['contributions'] ?? 0) <=> ($a['contributions'] ?? 0);
        });

        return $result;
    }

    #[Route('/product/{slug}/about', name: 'app_product_about')]
    public function productAbout(string $slug, EntityManagerInterface $em, GithubService $githubService, \App\Service\GitlabService $gitlabService): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        
        if (!$product) {
            throw $this->createNotFoundException('Product not found');
        }

        $ghContribs = [];
        $glContribs = [];
        
        if ($product->getGithubUrl()) {
            $ghContribs = $githubService->getContributors($product->getGithubUrl());
        }
        if ($product->getGitlabUrl()) {
            $glContribs = $gitlabService->getContributors($product->getGitlabUrl());
        }

        $contributors = $this->mergeContributors($ghContribs, $glContribs);

        return $this->render('main/about.html.twig', [
            'product' => $product,
            'contributors' => $contributors
        ]);
    }

    #[Route('/projtlauncher/about', name: 'app_about')]
    public function about(EntityManagerInterface $em, GithubService $githubService, \App\Service\GitlabService $gitlabService): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => 'projt-launcher']);
        
        $ghContribs = [];
        $glContribs = [];
        
        if ($product) {
            if ($product->getGithubUrl()) {
                $ghContribs = $githubService->getContributors($product->getGithubUrl());
            }
            if ($product->getGitlabUrl()) {
                $glContribs = $gitlabService->getContributors($product->getGitlabUrl());
            }
        }
        
        $contributors = $this->mergeContributors($ghContribs, $glContribs);
        
        return $this->render('main/about.html.twig', [
            'product' => $product,
            'contributors' => $contributors
        ]);
    }



    #[Route('/handbook', name: 'app_handbook')]
    public function handbook(EntityManagerInterface $em): Response
    {
        $allEntries = $em->getRepository(HandbookEntry::class)->findAll();
        $tree = $this->buildHandbookTree($allEntries);

        return $this->render('main/handbook.html.twig', [
            'entries' => $allEntries,
            'tree' => $tree
        ]);
    }

    #[Route('/handbook/search', name: 'app_handbook_search', methods: ['GET'])]
    public function handbookSearch(Request $request, EntityManagerInterface $em): Response
    {
        $query = $request->query->get('q');
        $allEntries = $em->getRepository(HandbookEntry::class)->findAll();
        $tree = $this->buildHandbookTree($allEntries);

        if (!$query) {
            return $this->render('main/handbook.html.twig', [
                'entries' => $allEntries,
                'tree' => $tree
            ]);
        }

        $results = $em->getRepository(HandbookEntry::class)->search($query);

        return $this->render('main/handbook.html.twig', [
            'entries' => $allEntries,
            'tree' => $tree,
            'search_query' => $query,
            'search_results' => $results
        ]);
    }

    #[Route('/handbook/entry/{id}/feedback', name: 'app_handbook_feedback', methods: ['POST'])]
    public function handbookFeedback(\App\Entity\HandbookEntry $entry, Request $request, EntityManagerInterface $em): Response
    {
        $helpful = $request->request->get('helpful') === '1';
        $user = $this->getUser();
        $ip = $request->getClientIp();

        $repo = $em->getRepository(\App\Entity\HandbookFeedback::class);
        
        // Try to find existing feedback by User or IP
        $criteria = ['entry' => $entry];
        if ($user) {
            $criteria['user'] = $user;
        } else {
            $criteria['ip'] = $ip;
            $criteria['user'] = null; // Ensure we don't accidentally match an IP that now has a user
        }

        $feedback = $repo->findOneBy($criteria);

        if (!$feedback) {
            $feedback = new \App\Entity\HandbookFeedback();
            $feedback->setEntry($entry);
            $feedback->setIp($ip);
            if ($user) {
                $feedback->setUser($user);
            }
            $em->persist($feedback);
        }

        $feedback->setHelpful($helpful);
        $feedback->setCreatedAt(new \DateTimeImmutable()); // Update timestamp to show when they last voted
        
        $em->flush();
        
        $this->addFlash('success', 'Thank you for your feedback!');
        
        return $this->redirectToRoute('app_handbook_show', ['slug' => $entry->getSlug()]);
    }

    #[Route('/handbook/{slug}', name: 'app_handbook_show', requirements: ['slug' => '.+'])]

    public function handbookShow(string $slug, EntityManagerInterface $em): Response
    {
        $slug = rtrim($slug, '/');
        if (empty($slug)) {
            return $this->redirectToRoute('app_handbook');
        }

        $repo = $em->getRepository(HandbookEntry::class);
        $entry = $repo->findOneBy(['slug' => $slug]);
        
        // Try fallback to /index if not found
        if (!$entry) {
            $entry = $repo->findOneBy(['slug' => $slug . '/index']);
        }

        if (!$entry) {
            throw $this->createNotFoundException('The entry does not exist: ' . $slug);
        }

        $allEntries = $repo->findAll();
        $tree = $this->buildHandbookTree($allEntries);
        
        return $this->render('main/handbook_show.html.twig', [
            'entry' => $entry,
            'tree' => $tree
        ]);
    }

    private function buildHandbookTree(array $entries): array
    {
        $tree = [];
        foreach ($entries as $entry) {
            $parts = explode('/', $entry->getSlug());
            $current = &$tree;
            foreach ($parts as $part) {
                if (!isset($current[$part])) {
                    $current[$part] = ['_children' => [], '_entry' => null];
                }
                $current = &$current[$part]['_children'];
            }
            // Re-traverse to set the entry on the correct node
            $current = &$tree;
            foreach ($parts as $part) {
                if ($part === end($parts)) {
                    $current[$part]['_entry'] = $entry;
                }
                $current = &$current[$part]['_children'];
            }
        }
        return $tree;
    }

    #[Route('/licenses', name: 'app_licenses')]
    public function licenses(EntityManagerInterface $em): Response
    {
        $categories = $em->getRepository(LicenseCategory::class)->findBy([], ['displayOrder' => 'ASC']);
        
        // Filter out categories that are explicitly for SPDX/Global (keep the list clean)
        $filteredCategories = array_filter($categories, function($cat) {
            return !in_array($cat->getName(), ['OSI Approved', 'Other Open Source']);
        });

        $comparisons = $em->getRepository(\App\Entity\LicenseComparison::class)->findBy([], ['displayOrder' => 'ASC']);

        return $this->render('main/licenses.html.twig', [
            'categories' => $filteredCategories,
            'comparisons' => $comparisons,
        ]);
    }

    #[Route('/licenses/{slug}.txt', name: 'app_licenses_show_txt', requirements: ['slug' => '.+'])]
    public function licensesShowTxt(string $slug, EntityManagerInterface $em): Response
    {
        $license = $em->getRepository(License::class)->findOneBy(['slug' => $slug]);
        if (!$license) {
            throw $this->createNotFoundException('The license (' . $slug . ') does not exist');
        }
        return new Response($license->getContent(), 200, [
            'Content-Type' => 'text/plain; charset=utf-8'
        ]);
    }

    #[Route('/licenses/{slug}', name: 'app_licenses_show', requirements: ['slug' => '.+'])]
    public function licensesShow(string $slug, EntityManagerInterface $em): Response
    {
        $license = $em->getRepository(License::class)->findOneBy(['slug' => $slug]);
        if (!$license) {
            throw $this->createNotFoundException('The license does not exist');
        }
        return $this->render('main/licenses_show.html.twig', [
            'license' => $license
        ]);
    }

    #[Route('/feed.xml', name: 'app_feed_main', format: 'xml')]
    public function feedMain(EntityManagerInterface $em): Response
    {
        // Main feed contains only Root news.
        $news = $em->getRepository(News::class)->findBy(
            ['product' => null, 'status' => News::STATUS_PUBLISHED],
            ['date' => 'DESC']
        );

        $xml = $this->renderView('feed/main.xml.twig', [
            'news' => $news
        ]);

        return new Response($xml, 200, ['Content-Type' => 'application/xml']);
    }

    #[Route('/projtlauncher/feed/feed.xml', name: 'app_feed_launcher', format: 'xml')]
    public function feedLauncher(EntityManagerInterface $em): Response
    {
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->andWhere('n.status = :status')
            ->setParameter('slug', 'projt-launcher')
            ->setParameter('status', News::STATUS_PUBLISHED)
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $xml = $this->renderView('feed/launcher.xml.twig', [
            'news' => $news
        ]);

        return new Response($xml, 200, ['Content-Type' => 'application/xml']);
    }

    #[Route('/projtlauncher/feed/', name: 'app_feed_launcher_dir')]
    public function feedLauncherDir(EntityManagerInterface $em): Response
    {
        return $this->feedLauncher($em);
    }

    #[Route('/projtlauncher/feed/appcast.xml', name: 'app_feed_launcher_appcast', format: 'xml')]
    public function feedLauncherAppcast(EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => 'projt-launcher']);
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->setParameter('slug', 'projt-launcher')
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $xml = $this->renderView('feed/appcast.xml.twig', [
            'news' => $news,
            'product' => $product
        ]);
        return new Response($xml, 200, ['Content-Type' => 'application/xml']);
    }

    #[Route('/projtlauncher/feed/feed.json', name: 'app_feed_launcher_json', format: 'json')]
    public function feedLauncherJson(EntityManagerInterface $em): Response
    {
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->setParameter('slug', 'projt-launcher')
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $json = $this->renderView('feed/feed.json.twig', ['news' => $news]);
        return new Response($json, 200, ['Content-Type' => 'application/json']);
    }

    #[Route('/product/{slug}/news', name: 'app_product_news_list')]
    public function productNewsList(string $slug, Request $request, EntityManagerInterface $em): Response
    {
        $query = $request->query->get('q');
        $repo = $em->getRepository(News::class);

        if ($query) {
             $news = $repo->createQueryBuilder('n')
                ->join('n.product', 'p')
                ->where('(n.title LIKE :query OR n.content LIKE :query)')
                ->andWhere('p.slug = :slug')
                ->andWhere('n.status = :status')
                ->setParameter('query', '%' . $query . '%')
                ->setParameter('slug', $slug)
                ->setParameter('status', News::STATUS_PUBLISHED)
                ->orderBy('n.date', 'DESC')
                ->getQuery()
                ->getResult();
        } else {
            $news = $repo->createQueryBuilder('n')
                ->join('n.product', 'p')
                ->where('p.slug = :slug')
                ->andWhere('n.status = :status')
                ->setParameter('slug', $slug)
                ->setParameter('status', News::STATUS_PUBLISHED)
                ->orderBy('n.date', 'DESC')
                ->getQuery()
                ->getResult();
        }

        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);

        return $this->render('main/product_news.html.twig', [
            'news_list' => $news,
            'product' => $product,
            'slug' => $slug,
            'search_query' => $query
        ]);
    }

    #[Route('/product/{slug}/news/{newsSlug}', name: 'app_product_news_show')]
    public function productNewsDetail(string $slug, string $newsSlug, EntityManagerInterface $em): Response
    {
        $post = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('n.slug = :newsSlug')
            ->andWhere('p.slug = :productSlug')
            ->setParameter('newsSlug', $newsSlug)
            ->setParameter('productSlug', $slug)
            ->getQuery()
            ->getOneOrNullResult();

        if (!$post) {
            throw $this->createNotFoundException('News post not found');
        }

        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);

        return $this->render('main/product_news_show.html.twig', [
            'post' => $post,
            'product' => $product
        ]);
    }

    #[Route('/product/{slug}/feed.xml', name: 'app_product_feed', format: 'xml')]
    public function productFeed(string $slug, EntityManagerInterface $em, \App\Service\LauncherFeedService $launcherFeed): Response
    {
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->andWhere('n.status = :status')
            ->setParameter('slug', $slug)
            ->setParameter('status', News::STATUS_PUBLISHED)
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        $title = $product ? $product->getTitle() . ' News' : ucwords(str_replace('-', ' ', $slug)) . ' News';

        // ProjT Launcher gets the updater-compatible appcast feed
        if ($slug === 'projt-launcher') {
            // Build asset map keyed by release_tag (falls back to release_version)
            $assetMap = [];
            $ftpFolder = ($product ? $product->getFtpFolderName() : null) ?? 'ProjT-Launcher';
            foreach ($news as $post) {
                $meta = $post->getMetadata() ?? [];
                $version = $meta['release_version'] ?? null;
                $releaseTag = $meta['release_tag'] ?? $version;

                if ($releaseTag !== null && !isset($assetMap[$releaseTag])) {
                    $assetMap[$releaseTag] = $launcherFeed->getAssetsForVersion($releaseTag, $ftpFolder);
                }
            }

            $xml = $this->renderView('feed/product_launcher.xml.twig', [
                'news' => $news,
                'title' => $title,
                'product' => $product,
                'asset_map' => $assetMap,
                'feed_url' => $this->generateUrl('app_product_feed', ['slug' => $slug], \Symfony\Component\Routing\Generator\UrlGeneratorInterface::ABSOLUTE_URL),
                'site_url' => $this->generateUrl('app_home', [], \Symfony\Component\Routing\Generator\UrlGeneratorInterface::ABSOLUTE_URL)
            ]);

            return new Response($xml, 200, ['Content-Type' => 'application/xml']);
        }

        // Generic product feed
        $xml = $this->renderView('feed/product.xml.twig', [
            'news' => $news,
            'title' => $title,
            'feed_url' => $this->generateUrl('app_product_feed', ['slug' => $slug], \Symfony\Component\Routing\Generator\UrlGeneratorInterface::ABSOLUTE_URL),
            'site_url' => $this->generateUrl('app_home', [], \Symfony\Component\Routing\Generator\UrlGeneratorInterface::ABSOLUTE_URL)
        ]);

        return new Response($xml, 200, ['Content-Type' => 'application/xml']);
    }

    #[Route('/product/{slug}/feed.json', name: 'app_product_feed_json', format: 'json')]
    public function productFeedJson(string $slug, EntityManagerInterface $em): Response
    {
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->setParameter('slug', $slug)
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $json = $this->renderView('feed/feed.json.twig', ['news' => $news]);
        return new Response($json, 200, ['Content-Type' => 'application/json']);
    }

    #[Route('/product/{slug}/appcast.xml', name: 'app_product_feed_appcast', format: 'xml')]
    public function productFeedAppcast(string $slug, EntityManagerInterface $em): Response
    {
        $product = $em->getRepository(\App\Entity\Product::class)->findOneBy(['slug' => $slug]);
        $news = $em->getRepository(News::class)
            ->createQueryBuilder('n')
            ->join('n.product', 'p')
            ->where('p.slug = :slug')
            ->setParameter('slug', $slug)
            ->orderBy('n.date', 'DESC')
            ->getQuery()
            ->getResult();

        $xml = $this->renderView('feed/appcast.xml.twig', [
            'news' => $news,
            'product' => $product
        ]);
        return new Response($xml, 200, ['Content-Type' => 'application/xml']);
    }
    #[Route('/u/{username}', name: 'app_public_profile')]
    public function publicProfile(string $username, EntityManagerInterface $em): Response
    {
        $user = $em->getRepository(\App\Entity\User::class)->findOneBy(['username' => $username]);
        if (!$user) {
            // Fallback: check github username
            $user = $em->getRepository(\App\Entity\User::class)->findOneBy(['githubUsername' => $username]);
        }

        if (!$user) {
            throw $this->createNotFoundException('User not found');
        }

        $gravatarHash = md5(strtolower(trim($user->getEmail())));

        return $this->render('user/public_profile.html.twig', [
            'user' => $user,
            'gravatarHash' => $gravatarHash
        ]);
    }
    #[Route('/roadmap', name: 'app_roadmap')]
    public function roadmap(EntityManagerInterface $em): Response
    {
        $items = $em->getRepository(\App\Entity\RoadmapItem::class)->findBy([], ['priority' => 'DESC']);
        
        return $this->render('main/roadmap.html.twig', [
            'items' => $items
        ]);
    }

    #[Route('/products', name: 'app_products')]
    public function products(EntityManagerInterface $em): Response
    {
        $products = $em->getRepository(\App\Entity\Product::class)->findBy(['productVisibility' => 'public'], ['updatedAt' => 'DESC']);
        return $this->render('main/products.html.twig', [
            'products' => $products
        ]);
    }

    #[Route('/branding', name: 'app_branding')]
    public function branding(): Response
    {
        return $this->render('main/branding.html.twig');
    }

    #[Route('/spdx-licenses', name: 'app_spdx_licenses')]
    public function spdxLicenses(Request $request, EntityManagerInterface $em): Response
    {
        $q = $request->query->get('q');
        $qb = $em->getRepository(\App\Entity\SpdxLicense::class)->createQueryBuilder('s')
            ->orderBy('s.identifier', 'ASC');

        if ($q) {
            $qb->andWhere('s.identifier LIKE :q OR s.name LIKE :q')
               ->setParameter('q', '%' . $q . '%');
        }

        $licenses = $qb->getQuery()->getResult();

        return $this->render('main/spdx_licenses.html.twig', [
            'licenses' => $licenses,
            'q' => $q
        ]);
    }

    #[Route('/spdx-licenses/{identifier}', name: 'app_spdx_license_show', requirements: ['identifier' => '^(?!.*\.txt$).+'])]
    public function spdxLicenseShow(string $identifier, EntityManagerInterface $em): Response
    {
        $license = $em->getRepository(\App\Entity\SpdxLicense::class)->findOneBy(['identifier' => $identifier]);
        if (!$license) throw $this->createNotFoundException('License not found');

        return $this->render('main/spdx_license_show.html.twig', [
            'license' => $license
        ]);
    }

    #[Route('/spdx-licenses/{identifier}.txt', name: 'app_spdx_license_raw', requirements: ['identifier' => '.+'])]
    public function spdxLicenseRaw(string $identifier, EntityManagerInterface $em): Response
    {
        $license = $em->getRepository(\App\Entity\SpdxLicense::class)->findOneBy(['identifier' => $identifier]);
        if (!$license || !$license->getContent()) throw $this->createNotFoundException('Content not available');

        return new Response($license->getContent(), 200, ['Content-Type' => 'text/plain']);
    }

    #[Route('/search', name: 'app_global_search')]
    public function globalSearch(Request $request, EntityManagerInterface $em): Response
    {
        $q = $request->query->get('q');
        if (!$q) {
            return $this->redirectToRoute('app_home');
        }

        $handbookResults = $em->getRepository(\App\Entity\HandbookEntry::class)->createQueryBuilder('h')
            ->where('h.title LIKE :q OR h.content LIKE :q')
            ->setParameter('q', '%'.$q.'%')
            ->setMaxResults(10)
            ->getQuery()->getResult();

        $newsResults = $em->getRepository(\App\Entity\News::class)->createQueryBuilder('n')
            ->where('n.title LIKE :q OR n.content LIKE :q OR n.description LIKE :q')
            ->setParameter('q', '%'.$q.'%')
            ->setMaxResults(10)
            ->getQuery()->getResult();

        $userResults = $em->getRepository(\App\Entity\User::class)->createQueryBuilder('u')
            ->where('u.username LIKE :q OR u.githubUsername LIKE :q')
            ->setParameter('q', '%'.$q.'%')
            ->setMaxResults(10)
            ->getQuery()->getResult();

        $productResults = $em->getRepository(\App\Entity\Product::class)->createQueryBuilder('p')
        ->where('p.title LIKE :q OR p.description LIKE :q OR p.summary LIKE :q')
        ->setParameter('q', '%'.$q.'%')
        ->setMaxResults(5)
        ->getQuery()->getResult();

        return $this->render('main/search_results.html.twig', [
            'query' => $q,
            'handbook' => $handbookResults,
            'news' => $newsResults,
            'users' => $userResults,
            'products' => $productResults
        ]);
    }

    #[Route('/roadmap/{id}/vote', name: 'app_roadmap_vote', methods: ['POST'])]
    #[IsGranted('IS_AUTHENTICATED_FULLY')]
    public function roadmapVote(\App\Entity\RoadmapItem $item, EntityManagerInterface $em): Response
    {
        /** @var \App\Entity\User $user */
        $user = $this->getUser();
        
        if ($item->getVotes()->contains($user)) {
            $item->removeVote($user);
        } else {
            $item->addVote($user);
        }
        
        $em->flush();
        
        return $this->redirectToRoute('app_roadmap');
    }

    #[Route('/forge', name: 'app_forge')]
    public function forge(): Response
    {
        return $this->render('main/forge.html.twig');
    }

    #[Route('/trademark', name: 'app_trademark')]
    public function trademark(): Response
    {
        return $this->render('main/trademark.html.twig');
    }
}
