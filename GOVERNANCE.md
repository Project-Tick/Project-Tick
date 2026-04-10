# Project Tick Governance v1, 10 April 2026

Welcome to Project Tick!

This document discusses the Project Tick ecosystem, repository management methods, and governance structure.

## Management

Project Tick's CI/CD requirements can be viewed via Github Actions, or through the Git repositories: git.projecttick.org, git.projecttick.net, or gitweb.projecttick.net. Even though Project Tick hasn't completely abandoned the Pull Request model, the contribution threshold is very low with Pull Request. To briefly explain; you create a patch and normally open a PR, right? However, we don't use the Merge, Squash, or Rebase Merge model.

### Pull Request Model

As mentioned above, you submit a PR, it's reviewed, CI/CD runs, and if your results are successful, your contributions are added to the main feed via the .patch file provided by Github with the caption "Reviewed-by: <Reviewer name> <<EMAIL>>", signed, and included. It's that simple! Isn't it?

### Maintainer model

Maintainers are responsible for maintaining a project's folder located in .github/CODEOWNERS or ci/OWNERS. If you're wondering how to become a maintainer – how to manage a project's folder according to Project Tick standards and improve it – our model is very simple. You contribute over the long term, and if deemed suitable, your name will be given to the necessary files in that folder, making you a maintainer. Dear maintainers, we expect you to treat contributors well and respectfully under the Code of Conduct. Please do your best to do so.

### Repository Model

Our repositories are continuously and actively mirrored on GitHub and GitLab. Just because you develop and create a project on GitHub doesn't mean we operate entirely through GitHub. git.projecttick.org is always open for you to clone and modify the repository, but since our internal infrastructure doesn't host Gerrit or SM GitLab, code review isn't done through those platforms and is entirely managed through GitHub.

### Decision-Making Model

The decision-making mechanism is entirely under the control of the top person, [Mehmet Samet Duman](https://github.com/YongDo-Hyun). He evaluates the PULL requests approved by the maintainers and adds his own Reviewed-by tag. Mehmet Samet Duman will add the tag in the format: Reviewed-by: Mehmet Samet Duman <yongdohyun@projecttick.org>. At the same time, the Maintainer's Reviewed-by tag is added, and patches are added to the main feed as is. Then, the PR and, if applicable, the Issue are given the necessary tags, and a thank you is given before closing the Pull and any Issues. If the maintainer exercises their veto right and it doesn't pass Mehmet Samet Duman's approval, that PR will be unconditionally closed. However, if you think this is unfair, you can send a complaint to <projecttick@projecttick.org> with the [ISSUE] tag.

### Patch Acceptance Model

Your PRs are subjected to rigorous testing via CI/CD. Afterwards, the maintainer of the relevant area reviews the patch, and if it's logical and doesn't contain bad code, the maintainer uses "approve" and calls BDFL (Mehmet Samet Duman). He approves, and the patch is included in the main workflow. It's actually very simple. Maintainers can request fixes in patches, and if you approve the request and make the necessary changes, your chances of approval will increase. Conversely, if you refuse without a solid reason, the Maintainer will have veto power. As for bad code, it refers to going to great lengths to make things easier when they could be done more concisely. Don't bloat your codebases just to appear more visible or show off in Git Blame. If there are many maintainers in a region, for example, a folder and a top maintainer might be on the same project, a vote is held with +1 or -1 votes, and the one with the most votes takes effect. For example, in the MNV folder, you made a change to the tests and submitted a PR. The PR is reviewed by the maintainers, and if one requests the change and another agrees, all repository maintainers are called to the PR for a vote. However, each maintainer reserves the right not to vote. If the number of votes is equal, the final decision is made by the BDFL.

### Commit & Sign-off Model

Commits are made using the DCO standard, meaning signed-off-by. If DCO is not used, the PR bot will give an error, and if it is not corrected, the patch will be rejected. Please describe what you did in detail in your commits using Header, Body, and Footer.

### Inactive Maintainers Model

If a maintainer is unavailable, their position will be vacated. Maintainers, please let us know if you are leaving your post, are tired, or going on vacation. A maintainer can be offline for a maximum of 30 days if they don't have a valid reason or health issue. If a maintainer resigns or is dismissed, BDFL will replace the maintainer.