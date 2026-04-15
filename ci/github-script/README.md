# GitHub CI Scripts — Project Tick

JavaScript-based CI scripts using [`actions/github-script`](https://github.com/actions/github-script).

## Local Testing

```bash
cd ci/github-script
npm ci        # install dependencies
gh auth login # ensure GitHub CLI is authenticated
```

### Available Commands

#### Lint Commits
Validates commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) format.

```bash
./run lint-commits YongDo-Hyun Project-Tick 123
```

#### Prepare
Checks PR mergeability and validates branch targeting.

```bash
./run prepare YongDo-Hyun Project-Tick 123
```
