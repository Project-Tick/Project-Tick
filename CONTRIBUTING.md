# Contributing to Project Tick

## Restrictions on Generative AI Usage (AI Policy)
> [!NOTE]
> The following is adapted from [matplotlib's contributing guide](https://matplotlib.org/devdocs/devel/contribute.html#generative-ai) and the [Linux Kernel policy guide](https://www.kernel.org/doc./html/next/process/coding-assistants.html)

We expect authentic engagement in our community.

- Do not post output from Large Language Models or similar generative AI as comments on Github or our discord server, as such comments tend to be formulaic and low-quality content.
- If you use generative AI tools as an aid in developing code or documentation changes, ensure that you fully understand the proposed changes and can explain why they are the correct approach.

Make sure you have added value based on your personal competency to your contributions. 
Just taking some input, feeding it to an AI and posting the result is not of value to the project. 
To preserve precious core developer capacity, we reserve the right to rigorously reject seemingly AI generated low-value contributions.

### Signed-off-by and Developer Certificate of Origin 

AI agents MUST NOT add Signed-off-by tags. Only humans can legally certify the Developer Certificate of Origin (DCO). The human submitter is responsible for:

  - Reviewing all AI-generated code
  - Ensuring compliance with licensing requirements
  - Adding their own Signed-off-by tag to certify the DCO
  - Taking full responsibility for the contribution
  
See [Signing your work](#signing-your-work) for more information.


### Attribution

When AI tools contribute to development, proper attribution helps track the evolving role of AI in the development process. Contributions should include an Assisted-by tag in the commit message with the following format:

```
Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]
```

Where:

  - `AGENT_NAME` is the name of the AI tool or framework
  - `MODEL_VERSION` is the specific model version used
  - `[TOOL1] [TOOL2]` are optional specialized analysis tools used (e.g., coccinelle, sparse, smatch, clang-tidy)

Basic development tools (git, gcc, make, editors) should not be listed.

Example:

```
Assisted-by: Claude:claude-3-opus coccinelle sparse
```

<!-- ## Contributor License Agreement (CLA) // FIXME: CLA cannot be used until a CLA signing model is agreed upon.

By submitting a contribution to this repository, you agree that your
contribution is made under the terms of the **Project Tick Contributor
License Agreement (CLA)**.

The CLA ensures that:

- you have the legal right to submit the contribution,
- the contribution does not knowingly infringe third-party rights,
- Project Tick may distribute the contribution under the applicable
  Project Tick license(s) governing the component,
- long-term governance and license consistency across the Project Tick
  ecosystem can be maintained.

The CLA applies to all intentional contributions, including but not
limited to source code, documentation, tests, data, media assets, and
configuration files.

The full text of the current CLA is available at:

- <https://projecttick.org/licenses/PT-CLA-2.0.txt>

If you do not agree to the CLA, please do not submit contributions.
 -->
---

## Quick Start

Please read the [handbook](docs/handbook/) for the section you are interested in. Example: ProjT Launcher

---

## Commit Messages

```text
subproject(component): short description

Optional explanation of what and why.
```

Examples:

```text
projtlauncher(fix): fix crash on startup with invalid config
```

---

## DCO Sign-off

Every commit must include a sign-off line and all files to commit:

```sh
git commit -s -a
```

This adds:

```text
Signed-off-by: Your Name <your.email@example.com>
```

The bot enforces DCO compliance and labels PRs missing sign-off.

---

## Merge Request Process

### Before Submitting

- Run clang-format on changed files
- Ensure code compiles without warnings
- Add tests for new functionality
- Sign off all commits
- Update documentation if needed

### PR Requirements

- Clear description of what and why
- Reference related issues
- Pass all CI checks
- One logical change per PR
- **Do not mix**: refactors, features, and third-party updates must be in separate PRs
- Third-party library updates require standalone PRs with documented rationale

### Review Process

1. Automated CI runs tests and linting
2. Maintainer reviews code
3. Address feedback
4. Merge when approved

---

## Documentation

See `docs/` for detailed documentation:

- [docs/contributing/](docs/contributing/) - Contribution guides
- [docs/handbook/](docs/handbook/) - Developer handbook
- [docs/](docs/) - General documentation

---

## Contact

- Issues: [GitHub Issues](https://github.com/Project-Tick/Project-Tick/issues)
- Email: [projecttick@projecttick.org](mailto:projecttick@projecttick.org)

---

## License

By contributing, you agree to license your work under the project's licenses.
See [LICENSES/](LICENSES/) folder.

## Code of Conduct

See [CODE_OF_CONDUCT](CODE_OF_CONDUCT).
