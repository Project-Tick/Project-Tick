# Project Tick Governance v3, 24 April 2026

<!--

Copyright (C) 2026 Project Tick

This Project Tick Management Document is an official governance document of Project Tick. It may be freely
distributed in its original, unmodified form. Modification of this document is not permitted for official
use. Only Project Tick may issue modified versions, which will be released under a new version identifier
and archived accordingly. Any third-party modifications must not be presented, distributed, or referenced
as an official Project Tick document. In such cases, the document must be renamed, and all references to
the Project Tick name, branding, and trademarks must be removed or clearly distinguished to avoid confusion.

-->

Welcome to Project Tick!

This document discusses the Project Tick ecosystem, repository management methods, and governance structure.

## Management

Project Tick's CI/CD requirements can be viewed via Github Actions; the repository itself is accessible through Gitlab SM at git.projecttick.org. While Project Tick hasn't completely abandoned the Pull Request model, the contribution threshold using Pull Requests is very low. In short, you create a patch and normally open a Pull Request, right? However, we don't use Merge, Squash, or Rebase Merge models.

### Pull Request Model

As mentioned above, you submit a PR, it's reviewed, CI/CD is run, and if your results are successful, your contributions are incorporated into the main feed by running the /merge command on the Github PR.

### Maintainer model

Maintainers are responsible for maintaining a project's folder located in .github/CODEOWNERS or ci/OWNERS. If you're wondering how to become a maintainer – how to manage a project's folder according to Project Tick standards and improve it – our model is very simple. You contribute over the long term, and if deemed suitable, your name will be given to the necessary files in that folder, making you a maintainer. Dear maintainers, we expect you to treat contributors well and respectfully under the Code of Conduct. Please do your best to do so.

### Repository Model

Our repositories are constantly and actively being cloned on GitHub and GitLab. Just because you develop and create a project on GitHub doesn't mean we operate entirely through GitHub. git.projecttick.org is always open for cloning and modifying the repository, and we continue to accept contributions from GitHub for CI/CD checks. However, thanks to the newly developing Foreman, we aim to run Proxy CI on our internal Git server, GitLab Self-Managed.

### Decision-Making Model

The decision-making mechanism is entirely under the control of the top-level person, [Mehmet Samet Duman][1]. He reviews the PULL requests approved by the maintainers and adds his own "Reviewed-by: Mehmet Samet Duman" tag. The Mehmet Samet Duman tag is added in the following format: Reviewed-by: Mehmet Samet Duman <yongdohyun@projecttick.org>. At the same time, the maintainer's "Reviewed-by: Maintainer name <email-address>" tag is also added, and the patches are added to the main feed as is using the /merge command. Then, the PR and any Issues are added to the necessary tags, and thanks are given before the PULL request and all Issues are closed. If the maintainer uses their veto power and this does not pass Mehmet Samet Duman's approval, that PR is closed unconditionally. However, if you think this is unfair, you can send a complaint to <projecttick@projecttick.org> with the tag [ISSUE].

### Patch Acceptance Model

Your PRs are subjected to rigorous testing via CI/CD. Afterwards, the maintainer of the relevant area reviews the patch, and if it's logical and doesn't contain bad code, the maintainer uses "approve" and calls BDFL (Mehmet Samet Duman). He approves, and the patch is included in the main workflow with /merge command. It's actually very simple. Maintainers can request fixes in patches, and if you approve the request and make the necessary changes, your chances of approval will increase. Conversely, if you refuse without a solid reason, the Maintainer will have veto power. As for bad code, it refers to going to great lengths to make things easier when they could be done more concisely. Don't bloat your codebases just to appear more visible or show off in Git Blame. If there are many maintainers in a region, for example, a folder and a top maintainer might be on the same project, a vote is held with +1 or -1 votes, and the one with the most votes takes effect. For example, in the MNV folder, you made a change to the tests and submitted a PR. The PR is reviewed by the maintainers, and if one requests the change and another agrees, all repository maintainers are called to the PR for a vote. However, each maintainer reserves the right not to vote. If the number of votes is equal, the final decision is made by the BDFL.

### Commit & Sign-off Model

Commits are made using the DCO standard, meaning Signed-off-by. If DCO is not used, the PR bot will give an error, and if it is not corrected, the patch will be rejected. Please describe what you did in detail in your commits using Header, Body, and Footer.

### Inactive Maintainers Model

If a maintainer is unavailable, their position will be vacated. Maintainers, please let us know if you are leaving your post, are tired, or going on vacation. A maintainer can be offline for a maximum of 75 days if they don't have a valid reason or health issue. If a maintainer resigns or is dismissed, BDFL will replace the maintainer.

### Bug Control Model

Your bug reports will be received [here][4]. If you have any questions or issues related to development, you can contact us via [Github Issue][7] or [Project Tick Gitlab Issue][8]. bugs.projecttick.org is Bugzilla-based. If you find a bug or encounter an issue and want to report it, go to bugs.projecttick.org and log in with SSO. You can create your SSO account via id.projecttick.net. Then, click the "Report a bug" button and select a product. Let's take MNV as an example. After selecting MNV, select the version from the version counter, then specify your system and operating system. Immediately after, select the impact caused by the bug. Then, describe in detail how you can reproduce the bug. That's it! Your bug has been logged. The Maintenance Personnel or BDFL, who is an expert in the field, will investigate the issue and resolve the bug as soon as possible. If you know programming and are interested in these topics, you can also contribute via [Github][2].

### Project Tick SSO Model

Project Tick SSO uses the Keycloak infrastructure and helps you manage all your operations on projecttick.org or projecttick.net. We have prepared 4 OAuth mechanisms; Github, Gitlab, Microsoft and Google. You can create an account with these infrastructures or via email. Our website now has an SSO infrastructure instead of its own form. This will save you from account confusion, don't worry. Go ahead and create an account [Here][3]. You can manage your account from there.

### Release & Versioning Model

In our versioning model, each project has its own version, and when a project is updated, a snapshot tag is created. Its format is vYYYYMMDDHHMM. For example, when MNV version 10.0.4 is released, a snapshot tag is created. And a components.json file is added to the asset in the Github release. This file is read by MeshMC, and if there is no change in the version, the update is not visible. If the version is updated in that snapshot, MeshMC is upgraded to that version, and in this way, a fully-fledged (rolling release in code) model is achieved. This model creates continuity by constantly updating the code without breaking it, and if you are experiencing this issue, you can try the LTS versions we will release in the future. Or if you find a bug, you can report it [here][4].

### Security Model

#### How to report

If you discover a security vulnerability, please report it via email:

- [`projecttick@projecttick.org`](mailto:projecttick@projecttick.org)

#### What to include

When submitting a report, please include:

- Steps to reproduce the issue
- Expected and actual behavior
- Affected versions
- Logs or crash reports if available

### Licensing & REUSE Compliance Model

We place great importance on SPDX. We actively define the REUSE system in both the lefthook of our Bootstrap scripts and perform full scans with CI/CD. Currently, there are no issues with the REUSE lint, but we require that any new files you create include SPDX-FileCopyrightText, SPDX-FileContributor, and SPDX-License-Identifier because, as mentioned at the beginning, SPDX is one of the most important components of the license identifier, and many distros currently prioritize the presence of SPDX headers in their source code.

### AI Usage Model

Don't worry! We're not jealous of you using AI, but we don't support it, nor are we against it. Use AI, but know its limits.

- AI should never, ever use the `Signed-off-by` tag.
- Don't send AI code without reviewing it and making necessary adjustments.
- Use the `Assisted-by` tag, though optional.
- Manage AI with contextually appropriate and well-written prompts. For example, a prompt should be at least 150 words long and contain detailed logic. Otherwise, AI, or LLMs, won't fit your context and will babble. - Use a memory bank system, but never put these memory banks in a repository. If necessary, temporarily add them to `.gitignore`, and if the Assistant gets frustrated because it can't see them, politely explain it to them.

### Source of Truth Model

Project Tick's main repository can be accessed at git.projecttick.org. Remember, we have no connection with Github (except for CI/CD). However, if these conditions are met, i.e., if we have independent CI/CD machines, full independence will be achieved and this issue will be resolved to some extent. We are thinking of you and you can be sure that we will act accordingly. We returned to Github due to CI/CD restrictions and CI/CD configurations and decided that Gitlab was not suitable for us. Gitlab is a platform with a high contribution threshold and it is difficult for contributors to reach us, but let's give them credit. But you might ask, "Didn't the contribution surface increase even more when you left Gitlab and moved to your own infrastructure?" Don't worry. We accept your contributions via Github as described in the [Pull Request Model][5] section, but our process is slightly different. Don't let this scare you. There is a [Gitlab mirror][6] that you can check. We also upgraded our server from Gitolite to Gitlab, eliminating service duplication. We completely abandoned Mailman, Gitolite, CGit, Gitweb, and Forgejo. We are now in one place. Soon we will start accepting contributions entirely through our own Gitlab. Get ready.

### Subproject Boundaries

Each project makes its own decisions. Because we use the SnapShot tag system in this monorepository, our projects are constantly updated. However, there are differences between folder and project maintainer. Project Maintainer:

```json
Project Tick
├── Branding
│   ├── Current
│   └── Deprecated
├── cgit
├── ci
│   ├── codeowners-validator
│   └── github-script
├── classparser
├── cmark
├── corebinutils
│   ├── cat
│   ├── chflags
│   ├── chmod
│   ├── contrib
│   ├── cp
│   ├── cpuset
│   ├── csh
│   ├── date
│   ├── dd
│   ├── df
│   ├── domainname
│   ├── echo
│   ├── ed
│   ├── expr
│   ├── freebsd-version
│   ├── getfacl
│   ├── hostname
│   ├── kill
│   ├── ln
│   ├── ls
│   ├── mkdir
│   ├── mv
│   ├── nix
│   ├── nproc
│   ├── pax
│   ├── pkill
│   ├── ps
│   ├── pwait
│   ├── pwd
│   ├── realpath
│   ├── rm
│   ├── rmail
│   ├── rmdir
│   ├── setfacl
│   ├── sh
│   ├── sleep
│   ├── stty
│   ├── sync
│   ├── test
│   ├── timeout
│   └── uuidgen
├── docs
│   ├── CheckLists
│   ├── handbook
│   └── RelNotes
├── forgewrapper
├── ganalytics
├── genqrcode
├── hooks
├── iconfix
├── images4docker
├── infra
│   ├── actions-images
│   ├── foreman
│   └── merge-action
├── javacheck
├── javalauncher
├── json4cpp
├── katabasis
├── Legal
│   ├── Governance
│   └── Licenses
├── LICENSES
├── libnbtplusplus
├── LocalPeer
├── meshmc
├── meta
├── mnv
├── neozip
├── ofborg
├── optional-bare
├── Packaging
├── Patches
├── rainbow
├── systeminfo
├── tomlplusplus
├── website
│   └── projt-website
└── xz-embedded
```

As seen above, it generally manages one of the 16+ projects. However, the Folder Maintainer is only responsible for one folder within the 16+ projects. If there are interdependent projects among these 16+ projects, technical decisions must be made in a way that does not disrupt other products or in agreement with other project maintainers. Nothing should be broken here. Maintainers, we hope you know your place.

[1]: https://github.com/YongDo-Hyun
[2]: https://github.com/Project-Tick/Project-Tick.git
[3]: https://id.projecttick.net/realms/projecttick/account
[4]: https://bugs.projecttick.org
[5]: https://github.com/Project-Tick/Project-Tick/blob/master/GOVERNANCE.md#pull-request-model
[6]: https://gitlab.com/Project-Tick/Project-Tick
[7]: https://github.com/Project-Tick/Project-Tick/issues
[8]: https://git.projecttick.org/Project-Tick/Project-Tick/-/issues
