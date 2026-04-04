// @ts-check
const { classify } = require('../supportedBranches.js')
const { getCommitDetailsForPR } = require('./get-pr-commit-details.js')

// Supported Conventional Commit types for Project Tick
const CONVENTIONAL_TYPES = [
  'build',
  'chore',
  'ci',
  'docs',
  'feat',
  'fix',
  'perf',
  'refactor',
  'revert',
  'style',
  'test',
]

// Known project scopes in the monorepo
const KNOWN_SCOPES = [
  'archived',
  'cgit',
  'ci',
  'cmark',
  'corebinutils',
  'forgewrapper',
  'genqrcode',
  'hooks',
  'images4docker',
  'json4cpp',
  'libnbtplusplus',
  'meshmc',
  'meta',
  'mnv',
  'neozip',
  'tomlplusplus',
  'repo',
  'deps',
]

/**
 * Validates commit messages against Project Tick Conventional Commits conventions.
 *
 * Format: type(scope): subject
 *   type  — one of: feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert
 *   scope — optional, should match a project directory or be a known scope
 *   subject — imperative, no trailing period, no uppercase first letter
 *
 * @param {{
 *  github: InstanceType<import('@actions/github/lib/utils').GitHub>,
 *  context: import('@actions/github/lib/context').Context,
 *  core: import('@actions/core'),
 *  repoPath?: string,
 * }} CheckCommitMessagesProps
 */
async function checkCommitMessages({ github, context, core, repoPath }) {
  const pull_number = context.payload.pull_request?.number
  if (!pull_number) {
    core.info('This is not a pull request. Skipping checks.')
    return
  }

  const pr = (
    await github.rest.pulls.get({
      ...context.repo,
      pull_number,
    })
  ).data

  const baseBranchType = classify(
    pr.base.ref.replace(/^refs\/heads\//, ''),
  ).type
  const headBranchType = classify(
    pr.head.ref.replace(/^refs\/heads\//, ''),
  ).type

  if (
    baseBranchType.includes('development') &&
    headBranchType.includes('development') &&
    pr.base.repo.id === pr.head.repo?.id
  ) {
    core.info(
      'This PR is from one development branch to another. Skipping checks.',
    )
    return
  }

  const commits = await getCommitDetailsForPR({ core, pr, repoPath })

  const failures = new Set()
  const warnings = new Set()

  const conventionalRegex = new RegExp(
    `^(${CONVENTIONAL_TYPES.join('|')})(\\(([^)]+)\\))?(!)?: .+$`,
  )

  for (const commit of commits) {
    const msg = commit.subject
    const logPrefix = `Commit ${commit.sha.slice(0, 12)}`

    // Check: fixup/squash commits
    const fixups = ['amend!', 'fixup!', 'squash!']
    if (fixups.some((s) => msg.startsWith(s))) {
      core.error(
        `${logPrefix}: starts with "${fixups.find((s) => msg.startsWith(s))}". ` +
          'Did you forget to run `git rebase -i --autosquash`?',
      )
      failures.add(commit.sha)
      continue
    }

    // Check: Conventional Commit format
    if (!conventionalRegex.test(msg)) {
      core.error(
        `${logPrefix}: "${msg}" does not follow Conventional Commits format. ` +
          'Expected: type(scope): subject  (e.g. "feat(mnv): add keybinding")',
      )
      failures.add(commit.sha)
      continue
    }

    // Extract parts
    const match = msg.match(conventionalRegex)
    const type = match[1]
    const scope = match[3] || null

    // Check: trailing period
    if (msg.endsWith('.')) {
      core.error(
        `${logPrefix}: subject should not end with a period.`,
      )
      failures.add(commit.sha)
    }

    // Warning: unknown scope
    if (scope && !KNOWN_SCOPES.includes(scope)) {
      core.warning(
        `${logPrefix}: scope "${scope}" is not a known project. ` +
          `Known scopes: ${KNOWN_SCOPES.join(', ')}`,
      )
      warnings.add(commit.sha)
    }

    // Check: subject should not start with uppercase (after type(scope): )
    const subjectStart = msg.indexOf(': ') + 2
    if (subjectStart < msg.length) {
      const firstChar = msg[subjectStart]
      if (firstChar === firstChar.toUpperCase() && firstChar !== firstChar.toLowerCase()) {
        core.warning(
          `${logPrefix}: subject should start with lowercase letter.`,
        )
        warnings.add(commit.sha)
      }
    }

    if (!failures.has(commit.sha)) {
      core.info(`${logPrefix}: "${msg}" — passed.`)
    }
  }

  if (failures.size !== 0) {
    core.error(
      'Please review the Conventional Commits guidelines at ' +
        '<https://www.conventionalcommits.org/> and the project CONTRIBUTING.md.',
    )
    core.setFailed(
      `${failures.size} commit(s) do not follow commit conventions.`,
    )
  } else if (warnings.size !== 0) {
    core.warning(
      `${warnings.size} commit(s) have minor issues (see warnings above).`,
    )
  }
}

module.exports = checkCommitMessages
