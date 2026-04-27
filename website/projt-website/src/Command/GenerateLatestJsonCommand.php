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

namespace App\Command;

use App\Service\SnapshotService;
use Symfony\Component\Console\Attribute\AsCommand;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;
use Symfony\Component\Console\Style\SymfonyStyle;

#[AsCommand(
    name: 'app:generate-latest-json',
    description: 'Scan the FTP directory and generate a single latest.json containing all release channels (stable, beta, lts).',
)]
class GenerateLatestJsonCommand extends Command
{
    public function __construct(
        private readonly SnapshotService $snapshotService,
    ) {
        parent::__construct();
    }

    protected function execute(InputInterface $input, OutputInterface $output): int
    {
        $io = new SymfonyStyle($input, $output);

        $io->title('Generating latest.json');

        $data = $this->snapshotService->buildLatestJson();

        if (empty(array_intersect_key($data, array_flip(SnapshotService::CHANNELS)))) {
            $io->error('No snapshot tags found for any channel. latest.json not written.');
            return Command::FAILURE;
        }

        $path = $this->snapshotService->writeLatestJson($data);
        $io->success('latest.json written to: ' . $path);

        foreach (SnapshotService::CHANNELS as $channel) {
            if (!isset($data[$channel])) {
                $io->warning(sprintf('Channel "%s": no snapshot tag found — omitted.', $channel));
                continue;
            }

            $ch = $data[$channel];
            $io->section(strtoupper($channel));
            $io->table(
                ['Field', 'Value'],
                [
                    ['Release Tag', $ch['release_tag']],
                    ['Release Date', $ch['release_date']],
                    ['Components JSON', $ch['components_json_url']],
                ]
            );

            if (!empty($ch['components'])) {
                $rows = [];
                foreach ($ch['components'] as $name => $info) {
                    $rows[] = [$name, $info['version'] ?? 'N/A'];
                }
                $io->table(['Component', 'Version'], $rows);
            }

            if (!empty($ch['downloads'])) {
                $downloadCount = 0;
                foreach ($ch['downloads'] as $name => $info) {
                    $fileCount = count($info['files'] ?? []);
                    $downloadCount += $fileCount;
                    $io->writeln(sprintf('  <info>%s</info>: %d file(s)', $name, $fileCount));
                }
                $io->newLine();
                $io->writeln(sprintf('Total: <info>%d</info> component(s), <info>%d</info> file(s)',
                    count($ch['downloads']),
                    $downloadCount,
                ));
            }
        }

        return Command::SUCCESS;
    }
}
