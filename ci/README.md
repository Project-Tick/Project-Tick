# CI Infrastructure — Project Tick

This directory contains CI support files for the Project Tick monorepo.

## Structure

```
ci/
├── OWNERS                    # CI code ownership (CODEOWNERS format)
├── README.md                 # This file
├── supportedBranches.js      # Branch classification for CI decisions
├── codeowners-validator/     # Builds codeowners-validator from source
│   ├── owners-file-name.patch
│   └── permissions.patch
└── github-script/            # GitHub Actions JavaScript helpers
    ├── run                   # CLI entry point for local testing
    ├── lint-commits.js       # Commit message linter (Conventional Commits)
    ├── prepare.js            # PR preparation & validation
    ├── reviews.js            # GitHub review state management
    ├── get-pr-commit-details.js  # Extract commit details from PRs
    ├── withRateLimit.js      # GitHub API rate limit helper
    └── package.json          # Node.js dependencies
```

## GitHub Script

JavaScript-based CI scripts using [`actions/github-script`](https://github.com/actions/github-script).

### Local Testing

```bash
cd ci/github-script
gh auth login # ensure GitHub CLI is authenticated
./run lint-commits <owner> <repo> <pr-number>
./run prepare <owner> <repo> <pr-number>
```

## Branch Classification

`ci/supportedBranches.js` classifies repository branches for CI decisions:

| Prefix       | Type                     | Description                         |
|--------------|--------------------------|-------------------------------------|
| `master`     | development / primary    | Main development branch             |
| `release-*`  | development / primary    | Release branches (e.g. release-1.0) |
| `staging-*`  | development / secondary  | Pre-release staging                 |
| `feature-*`  | wip                      | Feature branches                    |
| `fix-*`      | wip                      | Bug fix branches                    |
| `backport-*` | wip                      | Backport branches                   |

## Commit Conventions

Project Tick uses [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): subject

feat(mnv): add new keybinding support
fix(meshmc): resolve crash on startup
ci(neozip): update build matrix
docs(cmark): fix API reference
```

The commit linter (`ci/github-script/lint-commits.js`) validates this format in PRs.
