# Tickborg — Evaluation System

## Overview

The evaluation system determines **which sub-projects changed** in a pull
request and schedules builds accordingly. It replaces the original ofborg's
Nix expression evaluation with a monorepo-aware strategy that inspects changed
files, commit messages, and PR metadata.

---

## Key Source Files

| File | Purpose |
|------|---------|
| `tickborg/src/tasks/evaluate.rs` | `EvaluationWorker`, `OneEval` — orchestrates eval |
| `tickborg/src/tasks/eval/mod.rs` | `EvaluationStrategy` trait, `EvaluationComplete` |
| `tickborg/src/tasks/eval/monorepo.rs` | `MonorepoStrategy` — Project Tick specific |
| `tickborg/src/tasks/evaluationfilter.rs` | `EvaluationFilterWorker` — PR event gating |
| `tickborg/src/bin/evaluation-filter.rs` | Evaluation filter binary |
| `tickborg/src/bin/mass-rebuilder.rs` | Mass rebuilder binary (runs evaluations) |
| `tickborg/src/tagger.rs` | `ProjectTagger` — PR label generation |
| `tickborg/src/evalchecker.rs` | `EvalChecker` — generic command runner |
| `tickborg/src/buildtool.rs` | `detect_changed_projects()`, `find_project()` |

---

## Stage 1: Evaluation Filter

The evaluation filter is the gateway that decides whether a PR event warrants
full evaluation.

### `EvaluationFilterWorker`

```rust
// tasks/evaluationfilter.rs
pub struct EvaluationFilterWorker {
    acl: acl::Acl,
}

impl worker::SimpleWorker for EvaluationFilterWorker {
    type J = ghevent::PullRequestEvent;

    async fn consumer(&mut self, job: &ghevent::PullRequestEvent) -> worker::Actions {
        // Check 1: Is the repo eligible?
        if !self.acl.is_repo_eligible(&job.repository.full_name) {
            return vec![worker::Action::Ack];
        }

        // Check 2: Is the PR open?
        if job.pull_request.state != ghevent::PullRequestState::Open {
            return vec![worker::Action::Ack];
        }

        // Check 3: Is the action interesting?
        let interesting = match job.action {
            PullRequestAction::Opened => true,
            PullRequestAction::Synchronize => true,
            PullRequestAction::Reopened => true,
            PullRequestAction::Edited => {
                if let Some(ref changes) = job.changes {
                    changes.base.is_some()  // base branch changed
                } else {
                    false
                }
            }
            _ => false,
        };

        if !interesting {
            return vec![worker::Action::Ack];
        }

        // Produce an EvaluationJob
        let msg = evaluationjob::EvaluationJob {
            repo: Repo { /* ... */ },
            pr: Pr { /* ... */ },
        };

        vec![
            worker::publish_serde_action(
                None, Some("mass-rebuild-check-jobs".to_owned()), &msg
            ),
            worker::Action::Ack,
        ]
    }
}
```

### Filtering Rules

| PR Action | Result |
|-----------|--------|
| `Opened` | Evaluate |
| `Synchronize` (new commits pushed) | Evaluate |
| `Reopened` | Evaluate |
| `Edited` with base branch change | Evaluate |
| `Edited` without base change | Skip |
| `Closed` | Skip |
| Any unknown action | Skip |

### AMQP Flow

```
mass-rebuild-check-inputs (queue)
    ← github-events (exchange), routing: pull_request.*
    → EvaluationFilterWorker
    → mass-rebuild-check-jobs (queue, direct publish)
```

---

## Stage 2: The Evaluation Worker

### `EvaluationWorker`

```rust
// tasks/evaluate.rs
pub struct EvaluationWorker<E> {
    cloner: checkout::CachedCloner,
    github_vend: tokio::sync::RwLock<GithubAppVendingMachine>,
    acl: Acl,
    identity: String,
    events: E,
}
```

The `EvaluationWorker` implements `SimpleWorker` and orchestrates the full
evaluation pipeline.

### Message Decoding

```rust
impl<E: stats::SysEvents + 'static> worker::SimpleWorker for EvaluationWorker<E> {
    type J = evaluationjob::EvaluationJob;

    async fn msg_to_job(&mut self, _: &str, _: &Option<String>, body: &[u8])
        -> Result<Self::J, String>
    {
        self.events.notify(Event::JobReceived).await;
        match evaluationjob::from(body) {
            Ok(job) => {
                self.events.notify(Event::JobDecodeSuccess).await;
                Ok(job)
            }
            Err(err) => {
                self.events.notify(Event::JobDecodeFailure).await;
                Err("Failed to decode message".to_owned())
            }
        }
    }
}
```

### Per-Job Evaluation (`OneEval`)

```rust
struct OneEval<'a, E> {
    client_app: &'a hubcaps::Github,
    repo: hubcaps::repositories::Repository,
    acl: &'a Acl,
    events: &'a mut E,
    identity: &'a str,
    cloner: &'a checkout::CachedCloner,
    job: &'a evaluationjob::EvaluationJob,
}
```

### Evaluation Pipeline

The `evaluate_job` method executes these steps:

#### 1. Check if PR is closed

```rust
match issue_ref.get().await {
    Ok(iss) => {
        if iss.state == "closed" {
            self.events.notify(Event::IssueAlreadyClosed).await;
            return Ok(self.actions().skip(job));
        }
        // ...
    }
}
```

#### 2. Determine auto-schedule architectures

```rust
if issue_is_wip(&iss) {
    auto_schedule_build_archs = vec![];
} else {
    auto_schedule_build_archs = self.acl.build_job_architectures_for_user_repo(
        &iss.user.login, &job.repo.full_name,
    );
}
```

WIP PRs get no automatic builds. The architecture list depends on whether the
user is trusted (7 platforms) or not (3 primary platforms).

#### 3. Create the evaluation strategy

```rust
let mut evaluation_strategy = eval::MonorepoStrategy::new(job, &issue_ref);
```

#### 4. Set commit status

```rust
let mut overall_status = CommitStatus::new(
    repo.statuses(),
    job.pr.head_sha.clone(),
    format!("{prefix}-eval"),
    "Starting".to_owned(),
    None,
);
overall_status.set_with_description(
    "Starting", hubcaps::statuses::State::Pending
).await?;
```

#### 5. Pre-clone actions

```rust
evaluation_strategy.pre_clone().await?;
```

#### 6. Clone and checkout

```rust
let project = self.cloner.project(&job.repo.full_name, job.repo.clone_url.clone());
let co = project.clone_for("mr-est".to_string(), self.identity.to_string())?;
```

#### 7. Checkout target branch, fetch PR, merge

```rust
evaluation_strategy.on_target_branch(&co_path, &mut overall_status).await?;
co.fetch_pr(job.pr.number)?;
evaluation_strategy.after_fetch(&co)?;
co.merge_commit(OsStr::new("pr"))?;
evaluation_strategy.after_merge(&mut overall_status).await?;
```

#### 8. Run evaluation checks

```rust
let checks = evaluation_strategy.evaluation_checks();
// Execute each check and update commit status
```

#### 9. Complete evaluation

```rust
let eval_complete = evaluation_strategy.all_evaluations_passed(
    &mut overall_status
).await?;
```

### Error Handling

```rust
async fn worker_actions(&mut self) -> worker::Actions {
    let eval_result = match self.evaluate_job().await {
        Ok(v) => Ok(v),
        Err(eval_error) => match eval_error {
            EvalWorkerError::EvalError(eval::Error::Fail(msg)) =>
                Err(self.update_status(msg, None, State::Failure).await),
            EvalWorkerError::EvalError(eval::Error::CommitStatusWrite(e)) =>
                Err(Err(e)),
            EvalWorkerError::CommitStatusWrite(e) =>
                Err(Err(e)),
        },
    };

    match eval_result {
        Ok(eval_actions) => {
            // Remove tickborg-internal-error label
            update_labels(&issue_ref, &[], &["tickborg-internal-error".into()]).await;
            eval_actions
        }
        Err(Ok(())) => {
            // Error, but PR updated successfully
            self.actions().skip(self.job)
        }
        Err(Err(CommitStatusError::ExpiredCreds(_))) => {
            self.actions().retry_later(self.job)  // NackRequeue
        }
        Err(Err(CommitStatusError::MissingSha(_))) => {
            self.actions().skip(self.job)  // Ack (force pushed)
        }
        Err(Err(CommitStatusError::InternalError(_))) => {
            self.actions().retry_later(self.job)  // NackRequeue
        }
        Err(Err(CommitStatusError::Error(_))) => {
            // Add tickborg-internal-error label
            update_labels(&issue_ref, &["tickborg-internal-error".into()], &[]).await;
            self.actions().skip(self.job)
        }
    }
}
```

---

## The `EvaluationStrategy` Trait

```rust
// tasks/eval/mod.rs
pub trait EvaluationStrategy {
    fn pre_clone(&mut self)
        -> impl Future<Output = StepResult<()>>;

    fn on_target_branch(&mut self, co: &Path, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<()>>;

    fn after_fetch(&mut self, co: &CachedProjectCo)
        -> StepResult<()>;

    fn after_merge(&mut self, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<()>>;

    fn evaluation_checks(&self) -> Vec<EvalChecker>;

    fn all_evaluations_passed(&mut self, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<EvaluationComplete>>;
}

pub type StepResult<T> = Result<T, Error>;

#[derive(Default)]
pub struct EvaluationComplete {
    pub builds: Vec<BuildJob>,
}

#[derive(Debug)]
pub enum Error {
    CommitStatusWrite(CommitStatusError),
    Fail(String),
}
```

---

## The `MonorepoStrategy`

### Title-Based Label Detection

```rust
// tasks/eval/monorepo.rs
const TITLE_LABELS: [(&str, &str); 12] = [
    ("meshmc", "project: meshmc"),
    ("mnv", "project: mnv"),
    ("neozip", "project: neozip"),
    ("cmark", "project: cmark"),
    ("cgit", "project: cgit"),
    ("json4cpp", "project: json4cpp"),
    ("tomlplusplus", "project: tomlplusplus"),
    ("corebinutils", "project: corebinutils"),
    ("forgewrapper", "project: forgewrapper"),
    ("genqrcode", "project: genqrcode"),
    ("darwin", "platform: macos"),
    ("windows", "platform: windows"),
];

fn label_from_title(title: &str) -> Vec<String> {
    let title_lower = title.to_lowercase();
    TITLE_LABELS.iter()
        .filter(|(word, _)| {
            let re = Regex::new(&format!("\\b{word}\\b")).unwrap();
            re.is_match(&title_lower)
        })
        .map(|(_, label)| (*label).into())
        .collect()
}
```

This uses word boundary regex (`\b`) to prevent false matches (e.g., "cmake"
won't match "cmark").

### Commit Scope Parsing

```rust
fn parse_commit_scopes(messages: &[String]) -> Vec<String> {
    let scope_re = Regex::new(r"^[a-z]+\(([^)]+)\)").unwrap();
    let colon_re = Regex::new(r"^([a-z0-9_-]+):").unwrap();

    let mut projects: Vec<String> = messages.iter()
        .filter_map(|line| {
            let trimmed = line.trim();
            // Conventional Commits: "feat(meshmc): add block renderer"
            if let Some(caps) = scope_re.captures(trimmed) {
                Some(caps[1].to_string())
            }
            // Simple: "meshmc: fix crash"
            else if let Some(caps) = colon_re.captures(trimmed) {
                let candidate = caps[1].to_string();
                if crate::buildtool::find_project(&candidate).is_some() {
                    Some(candidate)
                } else {
                    None
                }
            } else {
                None
            }
        })
        .collect();

    projects.sort();
    projects.dedup();
    projects
}
```

This recognises both Conventional Commits (`feat(meshmc): ...`) and simple
scope prefixes (`meshmc: ...`).

### File Change Detection

The strategy uses `CachedProjectCo::files_changed_from_head()` to get the
list of changed files, then passes them through
`buildtool::detect_changed_projects()` which maps each file to its top-level
directory and matches against known projects.

---

## The `EvalChecker`

```rust
// evalchecker.rs
pub struct EvalChecker {
    name: String,
    command: String,
    args: Vec<String>,
}

impl EvalChecker {
    pub fn new(name: &str, command: &str, args: Vec<String>) -> EvalChecker;
    pub fn name(&self) -> &str;
    pub fn execute(&self, path: &Path) -> Result<File, File>;
    pub fn cli_cmd(&self) -> String;
}
```

`EvalChecker` is a generic command execution wrapper. It runs a command in the
checkout directory and returns `Ok(File)` on success, `Err(File)` on failure.
The `File` contains captured stdout + stderr.

```rust
pub fn execute(&self, path: &Path) -> Result<File, File> {
    let output = Command::new(&self.command)
        .args(&self.args)
        .current_dir(path)
        .output();

    match output {
        Ok(result) => {
            // Write stdout + stderr to temp file
            if result.status.success() {
                Ok(file)
            } else {
                Err(file)
            }
        }
        Err(e) => {
            // Write error message to temp file
            Err(file)
        }
    }
}
```

---

## The `ProjectTagger`

```rust
// tagger.rs
pub struct ProjectTagger {
    selected: Vec<String>,
}

impl ProjectTagger {
    pub fn new() -> Self;

    pub fn analyze_changes(&mut self, changed_files: &[String]) {
        let projects = detect_changed_projects(changed_files);
        for project in projects {
            self.selected.push(format!("project: {project}"));
        }

        // Cross-cutting labels
        let has_ci = changed_files.iter().any(|f|
            f.starts_with(".github/") || f.starts_with("ci/")
        );
        let has_docs = changed_files.iter().any(|f|
            f.starts_with("docs/") || f.ends_with(".md")
        );
        let has_root = changed_files.iter().any(|f|
            !f.contains('/') && !f.ends_with(".md")
        );

        if has_ci   { self.selected.push("scope: ci".into()); }
        if has_docs { self.selected.push("scope: docs".into()); }
        if has_root { self.selected.push("scope: root".into()); }
    }

    pub fn tags_to_add(&self) -> Vec<String>;
    pub fn tags_to_remove(&self) -> Vec<String>;
}
```

### Label Examples

| Changed Files | Generated Labels |
|--------------|------------------|
| `meshmc/CMakeLists.txt` | `project: meshmc` |
| `mnv/src/main.c` | `project: mnv` |
| `.github/workflows/ci.yml` | `scope: ci` |
| `README.md` | `scope: docs` |
| `flake.nix` | `scope: root` |

---

## Commit Status Updates

Throughout evaluation, the commit status is updated to reflect progress:

```
Starting → Cloning project → Checking out target → Fetching PR →
Merging → Running checks → Evaluation complete
```

Or on failure:

```
Starting → ... → Merge failed (Failure)
Starting → ... → Check 'xyz' failed (Failure)
```

The commit status context includes a prefix determined dynamically:

```rust
let prefix = get_prefix(repo.statuses(), &job.pr.head_sha).await?;
let context = format!("{prefix}-eval");
```

---

## Auto-Scheduled vs. Manual Builds

### Auto-Scheduled (from PR evaluation)

When a PR is evaluated, builds are automatically scheduled for the detected
changed projects. The set of architectures depends on the ACL:

- **Trusted users**: All 7 platforms
- **Untrusted users**: 3 primary platforms (x86_64 Linux/macOS/Windows)
- **WIP PRs**: No automatic builds

### Manual (from `@tickbot` commands)

Users can manually trigger builds or re-evaluations:

```
@tickbot build meshmc          → Build meshmc on all eligible platforms
@tickbot eval                  → Re-run evaluation
@tickbot test mnv              → Run tests for mnv
@tickbot build meshmc neozip   → Build multiple projects
```

These are handled by the `github-comment-filter`, not the evaluation system.

---

## Label Management

The evaluation system manages PR labels via the GitHub API:

```rust
async fn update_labels(
    issue_ref: &IssueRef,
    add: &[String],
    remove: &[String],
) {
    // Add labels
    for label in add {
        issue_ref.labels().add(vec![label.clone()]).await;
    }
    // Remove labels
    for label in remove {
        issue_ref.labels().remove(label).await;
    }
}
```

Labels managed:
- `project: <name>` — Which sub-projects are affected
- `scope: ci` / `scope: docs` / `scope: root` — Cross-cutting changes
- `platform: macos` / `platform: windows` — Platform-specific changes
- `tickborg-internal-error` — Added when tickborg encounters an internal error
