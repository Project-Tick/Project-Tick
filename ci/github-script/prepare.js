// @ts-check
const { classify } = require('../supportedBranches.js')
const { postReview, dismissReviews } = require('./reviews.js')
const reviewKey = 'prepare'

/**
 * Prepares a PR for CI by checking mergeability and branch targeting.
 *
 * Outputs:
 *   - base: base branch classification
 *   - head: head branch classification
 *   - mergedSha: the merge commit SHA (or head SHA if conflict)
 *   - targetSha: the target comparison SHA
 *
 * @param {{
 *  github: InstanceType<import('@actions/github/lib/utils').GitHub>,
 *  context: import('@actions/github/lib/context').Context,
 *  core: import('@actions/core'),
 *  dry?: boolean,
 * }} PrepareProps
 */
module.exports = async ({ github, context, core, dry }) => {
  const pull_number = context.payload.pull_request.number

  for (const retryInterval of [5, 10, 20, 40, 80]) {
    core.info('Checking whether the pull request can be merged...')
    const prInfo = (
      await github.rest.pulls.get({
        ...context.repo,
        pull_number,
      })
    ).data

    if (prInfo.state !== 'open') throw new Error('PR is not open anymore.')

    if (prInfo.mergeable == null) {
      core.info(
        `GitHub is still computing mergeability, waiting ${retryInterval}s...`,
      )
      await new Promise((resolve) => setTimeout(resolve, retryInterval * 1000))
      continue
    }

    const { base, head } = prInfo

    const baseClassification = classify(base.ref)
    core.setOutput('base', baseClassification)
    console.log('base classification:', baseClassification)

    const headClassification =
      base.repo.full_name === head.repo.full_name
        ? classify(head.ref)
        : { type: ['wip'] }
    core.setOutput('head', headClassification)
    console.log('head classification:', headClassification)

    // Warn if targeting a release branch with a non-backport/fix branch
    if (
      baseClassification.stable &&
      baseClassification.type.includes('primary')
    ) {
      const headPrefix = head.ref.split('-')[0]
      if (!['backport', 'fix', 'revert'].includes(headPrefix)) {
        core.warning(
          `This PR targets release branch \`${base.ref}\`. ` +
            'New features should typically target \`master\`.',
        )
      }
    }

    // Check base branch targeting
    if (headClassification.type.includes('wip')) {
      // Determine the best base branch candidate
      const branches = (
        await github.paginate(github.rest.repos.listBranches, {
          ...context.repo,
          per_page: 100,
        })
      ).map(({ name }) => classify(name))

      const releases = branches
        .filter(({ stable, type }) => type.includes('primary') && stable)
        .sort((a, b) => b.version.localeCompare(a.version))

      async function mergeBase({ branch, order, version }) {
        const { data } = await github.rest.repos.compareCommitsWithBasehead({
          ...context.repo,
          basehead: `${branch}...${head.sha}`,
          per_page: 1,
          page: 2,
        })
        return {
          branch,
          order,
          version,
          commits: data.total_commits,
          sha: data.merge_base_commit.sha,
        }
      }

      let candidates = [await mergeBase(classify('master'))]
      for (const release of releases) {
        const nextCandidate = await mergeBase(release)
        if (candidates[0].commits === nextCandidate.commits)
          candidates.push(nextCandidate)
        if (candidates[0].commits > nextCandidate.commits)
          candidates = [nextCandidate]
        if (candidates[0].commits < 10000) break
      }

      const best = candidates.sort((a, b) => a.order - b.order).at(0)

      core.info(`Best base branch candidate: ${best.branch}`)

      if (best.branch !== base.ref) {
        const current = await mergeBase(classify(base.ref))
        const body = [
          `This PR targets \`${current.branch}\`, but based on the commit history ` +
            `\`${best.branch}\` appears to be a better fit ` +
            `(${current.commits - best.commits} fewer commits ahead).`,
          '',
          `If this is intentional, you can ignore this message. Otherwise:`,
          `- [Change the base branch](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/changing-the-base-branch-of-a-pull-request) to \`${best.branch}\`.`,
        ].join('\n')

        await postReview({ github, context, core, dry, body, reviewKey })
        core.warning(`PR may target the wrong base branch.`)
      } else {
        await dismissReviews({ github, context, core, dry, reviewKey })
      }
    } else {
      await dismissReviews({ github, context, core, dry, reviewKey })
    }

    let mergedSha, targetSha

    if (prInfo.mergeable) {
      core.info('The PR can be merged.')
      mergedSha = prInfo.merge_commit_sha
      targetSha = (
        await github.rest.repos.getCommit({
          ...context.repo,
          ref: prInfo.merge_commit_sha,
        })
      ).data.parents[0].sha
    } else {
      core.warning('The PR has a merge conflict.')
      mergedSha = head.sha
      targetSha = (
        await github.rest.repos.compareCommitsWithBasehead({
          ...context.repo,
          basehead: `${base.sha}...${head.sha}`,
        })
      ).data.merge_base_commit.sha
    }

    core.info(`merged: ${mergedSha}\ntarget: ${targetSha}`)
    core.setOutput('mergedSha', mergedSha)
    core.setOutput('targetSha', targetSha)

    // Detect touched CI-relevant files
    const files = (
      await github.paginate(github.rest.pulls.listFiles, {
        ...context.repo,
        pull_number,
        per_page: 100,
      })
    ).map((file) => file.filename)

    const touched = []
    if (files.some((f) => f.startsWith('ci/'))) touched.push('ci')
    if (files.includes('ci/pinned.json')) touched.push('pinned')
    if (files.some((f) => f.startsWith('.github/'))) touched.push('github')
    core.setOutput('touched', touched)

    return
  }
  throw new Error(
    'Timed out waiting for GitHub to compute mergeability. Check https://www.githubstatus.com.',
  )
}
