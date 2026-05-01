# Project Tick Governance v4, 29 April 2026

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

Project Tick's CI/CD requirements can be viewed via Github Actions; the repository can be accessed via Gitlab SM at git.projecttick.org. Project Tick completely abandoned Pull Requests as of April 29th. Even without Pull Requests, Project Tick can easily use your right to two repositories via SSO from Gitlab or with a personal account. And you can easily open a Merge Request. All you need to do is open an MR immediately after pushing a commit, even if it's in draft. Because this way you can test your commit from every point with CI. In short, we are now more free. And our workflows are now repository-independent.

### ~~Pull~~ Merge Request Model

As mentioned above, you submit an MR, it's reviewed, CI/CD is run, and if your results are successful, your commits are merged directly into the MR in their pure form with a merge commit.

### Maintainer model

Maintainers are responsible for maintaining the project folder located in the .github/CODEOWNERS or ci/OWNERS directory. How do you become a Maintainer? If you're wondering how to manage and improve your project folder according to Project Tick standards, our model is very simple. You contribute long-term, and if deemed appropriate, your name will be assigned to the necessary files in this folder, thus making you an administrator. Dear Maintainers, we expect you to treat contributors well and respectfully according to the Code of Conduct. Please try your best to do so. With Project Tick Gitlab, you have even more freedom. You can do your work comfortably and openly.

### Repository Model

Our repositories are constantly and actively cloned to GitHub and GitLab SaaS. Developing and creating a project on GitHub doesn't mean you have to work entirely through GitHub. git.projecttick.org is always open for cloning and modifying the repository, and we continued to accept contributions from GitHub for CI/CD checks. However, with our new Foreman system, we can easily test Merge Requests created in our own GitLab. And while doing this, we've also made workflows as independent of the repository as possible, except for secrets. Tons of projects like MeshMC, MNV, and Json++ can now be tested thanks to Foreman. Of course, we also need to acknowledge Flathub. We forked Flathub's vorarbeiter, so we're happy to give them credit. "Copyright (C) 2025 Flathub". If I may add a TLDr... Go to git.projecttick.org, create an account, fork the repository, commit, open a merge request, run CI/CD, make sure it's successful, let's check and merge.

### Decision-Making Model

The decision-making mechanism is entirely under the control of the top-level person, [Mehmet Samet Duman][1]. He reviews and merges Merge Requests approved by maintainers. If a maintainer uses their veto power and this does not pass Mehmet Samet Duman's approval, this PR is unconditionally closed. However, if you think this is unfair, you can send a complaint to <projecttick@projecttick.org> with the tag [ISSUE].

### Patch Acceptance Model

Your MRs undergo rigorous testing via CI/CD. Then, the maintainer of the relevant domain reviews the patch, and if it makes sense and doesn't contain bad code, the administrator bot calls BDFL (Mehmet Samet Duman) using ping admins. He approves it, and the patch is merged, which is very simple. It's actually quite simple. Maintainers can request fixes in patches, and if you approve the request and make the necessary changes, your chances of approval increase. Conversely, if you reject it without a solid reason, the administrator has veto power. Bad code refers to putting excessive effort into making things easier that could be done more quickly. Don't bloat your codebases to be more visible or to show off on GitBlame. For example, if there are multiple administrators in a domain, a folder and the top-level administrator might be in the same project; a vote is held with +1 or -1 votes, and the one with the most votes prevails. For example, you made a change in the tests in the MNV folder and submitted a Merge Request (MR). The MR is reviewed by the managers, and if one manager requests a change and the other agrees, all warehouse managers are called to vote on the MR. However, each manager reserves the right not to vote. In the event of a tie, the final decision is made by the BDFL.

### Commit & Sign-off Model

Commits are made according to the DCO standard, meaning they are Signed-off-by. If DCO is not used, the MR bot will throw an error, and if this error is not corrected, the patch will be rejected. Please describe the actions you take in your commits in detail using the Header, Body, and Footer sections. We are trying to approximate the Conventional commits standard. Please try to adapt your commits accordingly.

### Inactive Maintainers Model

If a maintainer is unavailable, their position will be vacated. Maintainers, please let us know if you are leaving your post, are tired, or going on vacation. A maintainer can be offline for a maximum of 75 days if they don't have a valid reason or health issue. If a maintainer resigns or is dismissed, BDFL will replace the maintainer.

### Bug Control Model

Your bug reports will be received [here][4]. If you have any questions or issues regarding development, you can contact us via [Project Tick Gitlab Issue][4]. Your bug reports will now be handled here.

### Project Tick SSO Model

Project Tick SSO uses the Keycloak infrastructure and helps you manage all your operations on projecttick.org or projecttick.net. We have prepared 4 OAuth mechanisms; Github, Gitlab SaaS, Microsoft and Google. You can create an account with these infrastructures or via email. Our website now has an SSO infrastructure instead of its own form. This will save you from account confusion, don't worry. Go ahead and create an account [Here][3]. You can manage your account from there.

### Release & Versioning Model

In our versioning model, each project has its own version, and a snapshot tag is created when a project is updated. Its format is vYYYYMMDDHHMM. For example, when MNV version 10.0.4 is released, a snapshot tag is created. And a file similar to components.json is added to ftp.projecttick.org. This file is read by MeshMC, and if there is no change in the version, the update is not visible. If the version is updated in that snapshot, MeshMC is upgraded to that version, thus achieving a fully-fledged (continuous code update) model. This model creates continuity by constantly updating the code without breaking it, and if you are experiencing this issue, you can use the latest LTS snapshot we released. Or if you find a bug, you can report it [here][4]. Our beta versions are now available. However, our beta versions only provide source code instead of a binary.

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

Project Tick's main repository can be accessed at git.projecttick.org. Remember, we have no connection with Github (except for CI/CD). However, if these conditions are met, i.e., if we have independent CI/CD machines, full independence will be achieved and this problem will be solved to some extent. We are thinking of you and you can be sure that we will act accordingly. We returned to Github due to CI/CD restrictions and CI/CD configurations and decided that Gitlab SaaS was not suitable for us. Gitlab is a platform with a high contribution threshold and it is difficult for contributors to reach us, but let's give them credit. But you might ask, "Didn't the contribution surface increase even more when you left Gitlab and moved to your own infrastructure?" Don't worry. Thanks to Foreman, we can solve the CI/CD problem in Merge Requests. There is also a [Gitlab mirror][6] that you can check. We also upgraded our server system from Gitolite to Gitlab, eliminating service duplication. We've completely abandoned Mailman, Gitolite, CGit, Gitweb, Forgejo, and Bugzilla. We're now all in one place. We're also completely leaving Github, except for CI/CD, because the problems it's been causing us lately have been frustrating. You can open your PRs as MRs and your Issues as Work Items in Project Tick Gitlab.

### Subproject Boundaries

Each project makes its own decisions. Because we use the Snapshot tag system in this monorepo, our projects are constantly updated. However, there are differences between folder and project maintainer. Project Maintainer:

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
[4]: https://git.projecttick.org/Project-Tick/Project-Tick/-/issues
[5]: https://github.com/Project-Tick/Project-Tick/blob/master/GOVERNANCE.md#merge-request-model
[6]: https://gitlab.com/Project-Tick/Project-Tick
[7]: https://github.com/Project-Tick/Project-Tick/issues
