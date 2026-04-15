#!/usr/bin/env node

// Branch classification for the Project Tick monorepo.
// Used by CI scripts to determine branch types and skip/run certain jobs.

const typeConfig = {
  master: ['development', 'primary'],
  release: ['development', 'primary'],
  staging: ['development', 'secondary'],
  'staging-next': ['development', 'secondary'],
  feature: ['wip'],
  fix: ['wip'],
  backport: ['wip'],
  revert: ['wip'],
  wip: ['wip'],
  dependabot: ['wip'],
}

// "order" ranks the development branches by how likely they are the intended
// base branch when they are an otherwise equally good fit.
const orderConfig = {
  master: 0,
  release: 1,
  staging: 2,
  'staging-next': 3,
}

function split(branch) {
  return {
    ...branch.match(
      /(?<prefix>[a-zA-Z-]+?)(-(?<version>\d+\.\d+(?:\.\d+)?)(?:-(?<suffix>.*))?)?$/,
    ).groups,
  }
}

function classify(branch) {
  const { prefix, version } = split(branch)
  return {
    branch,
    order: orderConfig[prefix] ?? Infinity,
    stable: version != null,
    type: typeConfig[prefix] ?? ['wip'],
    version: version ?? 'dev',
  }
}

module.exports = { classify, split }

// If called directly via CLI, runs the following tests:
if (!module.parent) {
  console.log('split(branch)')
  function testSplit(branch) {
    console.log(branch, split(branch))
  }
  testSplit('master')
  testSplit('release-1.0')
  testSplit('release-2.5.1')
  testSplit('staging-1.0')
  testSplit('staging-next-1.0')
  testSplit('feature-new-ui')
  testSplit('fix-crash-on-start')
  testSplit('backport-123-to-release-1.0')
  testSplit('dependabot-npm')
  testSplit('wip-experiment')

  console.log('')

  console.log('classify(branch)')
  function testClassify(branch) {
    console.log(branch, classify(branch))
  }
  testClassify('master')
  testClassify('release-1.0')
  testClassify('release-2.5.1')
  testClassify('staging-1.0')
  testClassify('staging-next-1.0')
  testClassify('feature-new-ui')
  testClassify('fix-crash-on-start')
  testClassify('backport-123-to-release-1.0')
  testClassify('dependabot-npm')
  testClassify('wip-experiment')
}
