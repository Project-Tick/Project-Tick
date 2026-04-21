## Flathub merge action

Internal Flathub merge action to merge submission pull requests.

This action used to live at [flathub/actions](https://github.com/flathub/actions/blob/master/merge/entrypoint.py)
and then at [flathub/flathub](https://github.com/flathub/flathub/tree/4b4880054ef9a5d0769ec81618784ef41c243b19/.github/actions/merge).
Please refer to the git log there for the contribution history which
has unfortunately been lost due to multiple migrations.

### Usage

```yaml
name: Submission pull request merge

on:
  issue_comment:
    types:
      - created

jobs:
  merge:
    runs-on: ubuntu-latest
    timeout-minutes: 30
    if: ${{ github.event.issue.pull_request && contains(github.event.comment.body, '/merge') }}
    steps:
      - uses: flathub-infra/merge-action@<sha>
        with:
          token: ${{ secrets.MY_TOKEN }}
```

The token `MY_TOKEN` must have the capabilities to create a repository
in the Flathub organisation; edit and push to repositories; add and
modify repository colloborators and set branch protections.

Commenting on a PR with the `/merge` command will trigger the action.
The commenter needs to be in the `reviewer` team or an admin of Flathub.

The format of the command is:

```
/merge:<optional target repo default branch, default:master> head=<pr head commit sha, 40 chars> <additional colloborators @foo @baz, default: PR author>

# Examples

/merge head=SHA
/merge:beta head=SHA
/merge:24.08 head=SHA
/merge:24.08 head=SHA @foo @baz
```

### Development

```sh
uv run ruff format
uv run ruff check --fix --exit-non-zero-on-fix
uv run mypy .
```
