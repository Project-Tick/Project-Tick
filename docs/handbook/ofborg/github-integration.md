# Tickborg — GitHub Integration

## Overview

Tickborg communicates with GitHub through the **GitHub App** model. A custom
fork of the `hubcaps` crate provides the Rust API client. Integration covers
webhook reception, commit statuses, check runs, issue/PR manipulation, and
comment posting.

---

## GitHub App Authentication

### `GithubAppVendingMachine`

```rust
// config.rs
pub struct GithubAppVendingMachine {
    conf: GithubAppConfig,
    current_token: Option<String>,
    token_expiry: Option<Instant>,
}
```

Handles two-stage GitHub App auth:

1. **JWT**: Signed with the App's private RSA key, valid for up to 10 minutes.
2. **Installation token**: Obtained with the JWT, valid for ~1 hour.

### Token Lifecycle

```rust
impl GithubAppVendingMachine {
    pub fn new(conf: GithubAppConfig) -> Self {
        GithubAppVendingMachine {
            conf,
            current_token: None,
            token_expiry: None,
        }
    }

    fn is_token_fresh(&self) -> bool {
        match self.token_expiry {
            Some(exp) => Instant::now() < exp,
            None => false,
        }
    }

    pub async fn get_token(&mut self) -> Result<String, String> {
        if self.is_token_fresh() {
            return Ok(self.current_token.clone().unwrap());
        }
        // Generate a fresh JWT
        let jwt = self.make_jwt()?;
        // Exchange JWT for installation token
        let client = hubcaps::Github::new(
            "tickborg".to_owned(),
            hubcaps::Credentials::Jwt(hubcaps::JwtToken::new(jwt)),
        )?;
        let installation = client.app()
            .find_repo_installation(&self.conf.owner, &self.conf.repo)
            .await?;
        let token_result = client.app()
            .create_installation_token(installation.id)
            .await?;

        self.current_token = Some(token_result.token.clone());
        // Expire tokens 5 minutes early to avoid edge cases
        self.token_expiry = Some(
            Instant::now() + Duration::from_secs(55 * 60) - Duration::from_secs(5 * 60)
        );

        Ok(token_result.token)
    }

    pub async fn github(&mut self) -> Result<hubcaps::Github, String> {
        let token = self.get_token().await?;
        Ok(hubcaps::Github::new(
            "tickborg".to_owned(),
            hubcaps::Credentials::Token(token),
        )?)
    }
}
```

### JWT Generation

```rust
fn make_jwt(&self) -> Result<String, String> {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH).unwrap()
        .as_secs() as i64;

    let payload = json!({
        "iat": now - 60,           // 1 minute in the past (clock skew)
        "exp": now + (10 * 60),    // 10 minutes from now
        "iss": self.conf.app_id,
    });

    let key = EncodingKey::from_rsa_pem(
        &std::fs::read(&self.conf.private_key_file)?
    )?;

    encode(&Header::new(Algorithm::RS256), &payload, &key)
        .map_err(|e| format!("JWT encoding error: {}", e))
}
```

### `GithubAppConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct GithubAppConfig {
    pub app_id: u64,
    pub private_key_file: PathBuf,
    pub owner: String,
    pub repo: String,
    pub installation_id: Option<u64>,
}
```

---

## GitHub App Configuration

The `GithubAppConfig` is nested under the top-level `Config`:

```json
{
  "github_app": {
    "app_id": 12345,
    "private_key_file": "/etc/tickborg/private-key.pem",
    "owner": "project-tick",
    "repo": "Project-Tick",
    "installation_id": 67890
  }
}
```

---

## Commit Statuses

### `CommitStatus`

```rust
// commitstatus.rs
pub struct CommitStatus {
    api: hubcaps::statuses::Statuses,
    sha: String,
    context: String,
    description: String,
    url: Option<String>,
}
```

### State Machine

```rust
impl CommitStatus {
    pub fn new(
        statuses: hubcaps::statuses::Statuses,
        sha: String,
        context: String,
        description: String,
        url: Option<String>,
    ) -> Self;

    pub async fn set_url(&mut self, url: Option<String>);

    pub async fn set_with_description(
        &mut self,
        description: &str,
        state: hubcaps::statuses::State,
    ) -> Result<(), CommitStatusError> {
        self.description = description.to_owned();
        self.send_status(state).await
    }

    pub async fn set(
        &mut self,
        state: hubcaps::statuses::State,
    ) -> Result<(), CommitStatusError>;

    async fn send_status(
        &self,
        state: hubcaps::statuses::State,
    ) -> Result<(), CommitStatusError> {
        let options = hubcaps::statuses::StatusOptions::builder(state)
            .description(&self.description)
            .context(&self.context);

        let options = match &self.url {
            Some(u) => options.target_url(u).build(),
            None => options.build(),
        };

        self.api.create(&self.sha, &options)
            .await
            .map_err(|e| CommitStatusError::from(e))?;

        Ok(())
    }
}
```

### Error Classification

```rust
#[derive(Debug)]
pub enum CommitStatusError {
    ExpiredCreds(String),   // GitHub App token expired
    MissingSha(String),     // Commit was force-pushed away
    InternalError(String),  // 5xx from GitHub API
    Error(String),          // Other errors
}
```

Error mapping from HTTP response:

| HTTP Status | CommitStatusError Variant | Worker Action |
|------------|--------------------------|---------------|
| 401 | `ExpiredCreds` | `NackRequeue` (retry) |
| 422 ("No commit found") | `MissingSha` | `Ack` (skip) |
| 500-599 | `InternalError` | `NackRequeue` (retry) |
| Other | `Error` | `Ack` + add error label |

---

## Check Runs

### `job_to_check()`

Creates a Check Run when a build job is started:

```rust
pub async fn job_to_check(
    github: &hubcaps::Github,
    repo_full_name: &str,
    job: &BuildJob,
    runner_identity: &str,
) -> Result<(), String> {
    let (owner, repo) = parse_repo_name(repo_full_name);
    let checks = github.repo(owner, repo).check_runs();

    checks.create(&hubcaps::checks::CheckRunOptions {
        name: format!("build-{}-{}", job.project, job.system),
        head_sha: job.pr.head_sha.clone(),
        status: Some(hubcaps::checks::CheckRunStatus::InProgress),
        external_id: Some(format!("{runner_identity}")),
        started_at: Some(Utc::now()),
        output: Some(hubcaps::checks::Output {
            title: format!("Building {} on {}", job.project, job.system),
            summary: format!("Runner: {runner_identity}"),
            text: None,
            annotations: vec![],
        }),
        ..Default::default()
    }).await.map_err(|e| format!("Failed to create check run: {e}"))?;

    Ok(())
}
```

### `result_to_check()`

Updates a Check Run when a build completes:

```rust
pub async fn result_to_check(
    github: &hubcaps::Github,
    repo_full_name: &str,
    result: &BuildResult,
) -> Result<(), String> {
    let (owner, repo) = parse_repo_name(repo_full_name);
    let checks = github.repo(owner, repo).check_runs();

    let conclusion = match &result.status {
        BuildStatus::Success => hubcaps::checks::Conclusion::Success,
        BuildStatus::Failure => hubcaps::checks::Conclusion::Failure,
        BuildStatus::TimedOut => hubcaps::checks::Conclusion::TimedOut,
        BuildStatus::Skipped => hubcaps::checks::Conclusion::Skipped,
        BuildStatus::UnexpectedError { .. } => hubcaps::checks::Conclusion::Failure,
    };

    // Find and update the existing check run
    // ...
}
```

---

## GitHub Event Types (ghevent)

### Common Types

```rust
// ghevent/common.rs
#[derive(Deserialize, Debug)]
pub struct GenericWebhook {
    pub repository: Repository,
}

#[derive(Deserialize, Debug)]
pub struct Repository {
    pub owner: User,
    pub name: String,
    pub full_name: String,
    pub clone_url: String,
}

#[derive(Deserialize, Debug)]
pub struct User {
    pub login: String,
    pub id: u64,
}

#[derive(Deserialize, Debug)]
pub struct Comment {
    pub id: u64,
    pub body: String,
    pub user: User,
}

#[derive(Deserialize, Debug)]
pub struct Issue {
    pub number: u64,
    pub title: String,
    pub state: String,
    pub user: User,
    pub labels: Vec<Label>,
}
```

### Pull Request Events

```rust
// ghevent/pullrequestevent.rs
#[derive(Deserialize, Debug)]
pub struct PullRequestEvent {
    pub action: PullRequestAction,
    pub number: u64,
    pub pull_request: PullRequest,
    pub repository: Repository,
    pub changes: Option<PullRequestChanges>,
}

#[derive(Deserialize, Debug)]
#[serde(rename_all = "snake_case")]
pub enum PullRequestAction {
    Opened,
    Closed,
    Synchronize,
    Reopened,
    Edited,
    Labeled,
    Unlabeled,
    ReviewRequested,
    Assigned,
    Unassigned,
    ReadyForReview,
}

#[derive(Deserialize, Debug)]
pub enum PullRequestState {
    #[serde(rename = "open")]
    Open,
    #[serde(rename = "closed")]
    Closed,
}

#[derive(Deserialize, Debug)]
pub struct PullRequest {
    pub id: u64,
    pub number: u64,
    pub state: PullRequestState,
    pub title: String,
    pub head: PullRequestRef,
    pub base: PullRequestRef,
    pub user: User,
    pub merged: Option<bool>,
    pub mergeable: Option<bool>,
}

#[derive(Deserialize, Debug)]
pub struct PullRequestRef {
    pub sha: String,
    #[serde(rename = "ref")]
    pub git_ref: String,
    pub repo: Repository,
}
```

### Issue Comment Events

```rust
// ghevent/issuecomment.rs
#[derive(Deserialize, Debug)]
pub struct IssueComment {
    pub action: IssueCommentAction,
    pub comment: Comment,
    pub issue: Issue,
    pub repository: Repository,
}

#[derive(Deserialize, Debug)]
#[serde(rename_all = "snake_case")]
pub enum IssueCommentAction {
    Created,
    Edited,
    Deleted,
}
```

### Push Events

```rust
// ghevent/pushevent.rs
#[derive(Deserialize, Debug)]
pub struct PushEvent {
    #[serde(rename = "ref")]
    pub git_ref: String,
    pub after: String,
    pub before: String,
    pub deleted: bool,
    pub forced: bool,
    pub created: bool,
    pub pusher: Pusher,
    pub head_commit: Option<HeadCommit>,
    pub repository: Repository,
    pub commits: Vec<HeadCommit>,
}

impl PushEvent {
    pub fn branch(&self) -> Option<&str>;
    pub fn is_tag(&self) -> bool;
    pub fn is_delete(&self) -> bool;
    pub fn is_zero_sha(&self) -> bool;
}
```

---

## Comment Posting

### `GitHubCommentPoster`

```rust
// tasks/githubcommentposter.rs
pub struct GitHubCommentPoster {
    github_vend: tokio::sync::RwLock<GithubAppVendingMachine>,
}

pub trait PostableEvent: Send {
    fn owner(&self) -> &str;
    fn repo(&self) -> &str;
    fn number(&self) -> u64;
}
```

### Posting a Result

```rust
impl worker::SimpleWorker for GitHubCommentPoster {
    type J = buildresult::BuildResult;

    async fn consumer(&mut self, job: &buildresult::BuildResult) -> worker::Actions {
        let github = self.github_vend.write().await.github().await;
        let repo = github.repo(&job.repo.owner, &job.repo.name);
        let issue = repo.issue(job.pr.number);

        // Build a markdown summary
        let comment_body = format_build_result(job);

        issue.comments().create(&hubcaps::comments::CommentOptions {
            body: comment_body,
        }).await;

        vec![worker::Action::Ack]
    }
}
```

---

## Comment Filtering

### `GitHubCommentWorker`

```rust
// tasks/githubcommentfilter.rs
pub struct GitHubCommentWorker {
    acl: Acl,
    github_vend: tokio::sync::RwLock<GithubAppVendingMachine>,
}
```

The comment filter processes incoming `IssueComment` events:

1. **Ignore non-creation actions** — Only `Created` matters.
2. **Parse command** — `commentparser::parse()` extracts `@tickbot` instructions.
3. **ACL check** — Verifies the commenter is allowed to issue the command.
4. **Generate build/eval jobs** — Creates `BuildJob` or `EvaluationJob` messages.
5. **Publish to AMQP** — Routes to the appropriate exchange.

```rust
async fn consumer(&mut self, job: &ghevent::IssueComment) -> worker::Actions {
    if job.action != IssueCommentAction::Created {
        return vec![worker::Action::Ack];
    }

    let instructions = commentparser::parse(&job.comment.body);
    if instructions.is_empty() {
        return vec![worker::Action::Ack];
    }

    let mut actions = Vec::new();

    for instruction in instructions {
        match instruction {
            Instruction::Build(projects, subset) => {
                // Verify ACL
                let architectures = self.acl.build_job_architectures_for_user_repo(
                    &job.comment.user.login,
                    &job.repository.full_name,
                );
                // Create BuildJob per project × architecture
                for project in projects {
                    for arch in &architectures {
                        let build_job = BuildJob { /* ... */ };
                        actions.push(worker::publish_serde_action(
                            Some("build-jobs".to_owned()),
                            None,
                            &build_job,
                        ));
                    }
                }
            }
            Instruction::Eval => {
                let eval_job = EvaluationJob { /* ... */ };
                actions.push(worker::publish_serde_action(
                    None,
                    Some("mass-rebuild-check-jobs".to_owned()),
                    &eval_job,
                ));
            }
            Instruction::Test(projects) => { /* ... */ }
        }
    }

    actions.push(worker::Action::Ack);
    actions
}
```

---

## The `hubcaps` Fork

Tickborg uses a forked version of `hubcaps` from:

```toml
[dependencies]
hubcaps = { git = "https://github.com/ofborg/hubcaps.git", rev = "0d7466e..." }
```

Key differences from upstream:
- **Check Runs API support** — Full CRUD for GitHub Checks API
- **GitHub App authentication** — JWT + installation token flow
- **Async/await** — Full Tokio-based async API
- **App API** — `find_repo_installation()`, `create_installation_token()`

---

## Webhook Signature Verification

See [webhook-receiver.md](webhook-receiver.md) for the full HMAC-SHA256
verification flow.

```rust
fn verify_signature(secret: &[u8], signature: &str, body: &[u8]) -> bool {
    let sig_bytes = match hex::decode(signature.trim_start_matches("sha256=")) {
        Ok(b) => b,
        Err(_) => return false,
    };

    let mut mac = Hmac::<Sha256>::new_from_slice(secret).unwrap();
    mac.update(body);
    mac.verify_slice(&sig_bytes).is_ok()
}
```

---

## Rate Limiting

The GitHub API has rate limits (5000 requests/hour for GitHub App installations).
Tickborg mitigates this by:

1. **Caching installation tokens** — Reused until 5 minutes before expiry.
2. **Minimal API calls** — Only essential status updates and label operations.
3. **Batching** — Label additions batched into single API calls where possible.
4. **Backoff on 403** — When rate-limited, jobs are `NackRequeue`'d for retry.
