# Tickborg — Data Flow

## Overview

This document traces the complete path of messages through the tickborg system
for the three primary event types: **pull request**, **comment command**, and
**push event**.

---

## Pull Request Flow

A PR opened against the monorepo triggers evaluation and automatic builds.

### Step-by-Step

```
GitHub                    Webhook Receiver         RabbitMQ
───────                   ─────────────────        ────────
POST /github-webhook ───► HMAC verify ──────────►  github-events exchange
  X-Hub-Signature-256       route by event type       routing_key: pull_request.opened
  X-GitHub-Event: pull_request
```

```
RabbitMQ                  Evaluation Filter        RabbitMQ
────────                  ─────────────────        ────────
mass-rebuild-check-inputs  PR filter logic ───────► mass-rebuild-check-jobs
  ◄── github-events          - Repo eligible?           (direct queue publish)
       pull_request.*         - Action interesting?
                              - PR open?
```

```
RabbitMQ                  Mass Rebuilder            RabbitMQ / GitHub
────────                  ──────────────            ─────────────────
mass-rebuild-check-jobs    EvaluationWorker          - Commit status: pending
                           OneEval:                  - Clone + merge PR
                             1. Check PR state       - Detect changed projects
                             2. Clone repo           - Generate labels
                             3. Fetch PR             - Commit status: success
                             4. Merge                - Publish BuildJob(s)
                             5. Detect changes  ──►  build-jobs exchange (fanout)
                             6. Run eval checks
                             7. Tag PR labels   ──►  GitHub API: add labels
```

```
RabbitMQ                  Builder                   RabbitMQ / GitHub
────────                  ───────                   ─────────────────
build-inputs-{id}          BuildWorker               - Check Run: in_progress
  ◄── build-jobs            1. Clone repo            - Publish log lines ──► logs exchange
                            2. Checkout PR           - Check Run: completed
                            3. Detect build system   - Publish BuildResult ──► build-results
                            4. Build
                            5. Test (if requested)
```

```
RabbitMQ                  Comment Poster            GitHub
────────                  ──────────────            ──────
build-results              Format result    ───────► PR comment with build summary
  ◄── build-results         as markdown
```

```
RabbitMQ                  Log Collector             Disk
────────                  ─────────────             ────
build-logs                 LogMessageCollector ────► /var/log/tickborg/builds/{id}.log
  ◄── logs exchange
       logs.*
```

### Sequence Diagram

```
GitHub ──► Webhook Receiver ──► [github-events]
                                     │
                          pull_request.*
                                     ▼
                              Evaluation Filter
                                     │
                                     ▼
                         [mass-rebuild-check-jobs]
                                     │
                                     ▼
                             Mass Rebuilder ──► GitHub (status + labels)
                                     │
                              BuildJob × N
                                     ▼
                              [build-jobs]
                                     │
                                     ▼
                                Builder ──► GitHub (check run)
                               /       \
                          [logs]    [build-results]
                            │           │
                            ▼           ▼
                      Log Collector  Comment Poster ──► GitHub (PR comment)
```

---

## Comment Command Flow

A user posts `@tickbot build meshmc` on a PR.

### Step-by-Step

```
GitHub                    Webhook Receiver         RabbitMQ
───────                   ─────────────────        ────────
POST /github-webhook ───► HMAC verify ──────────►  github-events exchange
  X-GitHub-Event:            route: issue_comment     routing_key: issue_comment.created
    issue_comment
```

```
RabbitMQ                  Comment Filter           RabbitMQ
────────                  ──────────────           ────────
comment-jobs               GitHubCommentWorker      build-jobs exchange
  ◄── github-events          1. Ignore !Created
       issue_comment.*        2. Parse @tickbot
                              3. Extract instruction
                              4. ACL check
                              5. Produce BuildJob(s) ──►  build-jobs (fanout)
```

The rest of the flow (builder → log collector → comment poster) is identical
to the PR flow.

### Comment Parser Detail

```
Input:  "@tickbot build meshmc neozip"

commentparser::parse()
    ┌──────────────────────────────────────────┐
    │  nom parser pipeline:                     │
    │  1. tag("@tickbot")                       │
    │  2. space1                                │
    │  3. alt((tag("build"), tag("test"),        │
    │         tag("eval")))                      │
    │  4. space1                                │
    │  5. separated_list1(space1, alphanumeric1) │
    └──────────────────────────────────────────┘

Output: [Instruction::Build(["meshmc", "neozip"], Subset::Project)]
```

### Message Expansion

A single comment can generate multiple AMQP messages:

```
@tickbot build meshmc
    │
    ▼
ACL: user allowed on [x86_64-linux, aarch64-linux, x86_64-darwin]
    │
    ▼
3 BuildJob messages:
  ├── BuildJob { project: "meshmc", system: "x86_64-linux", ... }
  ├── BuildJob { project: "meshmc", system: "aarch64-linux", ... }
  └── BuildJob { project: "meshmc", system: "x86_64-darwin", ... }
```

---

## Push Event Flow

A push to a tracked branch (e.g., `main`).

### Step-by-Step

```
GitHub                    Webhook Receiver         RabbitMQ
───────                   ─────────────────        ────────
POST /github-webhook ───► HMAC verify ──────────►  github-events exchange
  X-GitHub-Event: push       route: push              routing_key: push.push
```

```
RabbitMQ                  Push Filter              RabbitMQ / External
────────                  ───────────              ─────────────────
push-jobs                  PushFilterWorker
  ◄── github-events          1. Skip tags
       push.*                 2. Skip deletes
                              3. Skip zero-SHA
                              4. Check branch name
                              5. Trigger rebuild  ──►  (future: deployment hooks)
```

### Push Event Guards

```rust
impl worker::SimpleWorker for PushFilterWorker {
    async fn consumer(&mut self, job: &ghevent::PushEvent) -> worker::Actions {
        // Skip tags
        if job.is_tag() {
            return vec![worker::Action::Ack];
        }

        // Skip branch deletions
        if job.is_delete() {
            return vec![worker::Action::Ack];
        }

        // Skip zero-SHA (orphan push)
        if job.is_zero_sha() {
            return vec![worker::Action::Ack];
        }

        // Only process main branch
        if job.branch() != Some("main") {
            return vec![worker::Action::Ack];
        }

        // Process the push event...
    }
}
```

---

## Statistics Flow

All services emit `EventMessage` events to the stats exchange.

```
Any Service
    │
    ├── worker::Action::Publish ──► [stats] exchange (fanout)
    │                                    │
    │                                    ▼
    │                              stats-events queue
    │                                    │
    │                                    ▼
    │                             StatCollectorWorker
    │                                    │
    └── Metrics:                         ▼
        - JobReceived              MetricCollector
        - JobDecodeSuccess            │
        - JobDecodeFailure            ▼
        - BuildStarted            HTTP endpoint (:9090)
        - BuildCompleted             /metrics
        - EvalStarted
        - EvalCompleted
```

### `SysEvents` Trait

```rust
// stats.rs
pub trait SysEvents: Send {
    fn notify(&mut self, event: Event)
        -> impl Future<Output = ()>;
}
```

Every worker is generic over `E: SysEvents`, allowing stats collection
to be plugged in or replaced with a no-op.

---

## Log Collection Flow

Build logs are streamed in real-time via the `logs` exchange.

```
Builder (BuildWorker)
    │
    │  During build execution, for each output line:
    │
    ├── BuildLogStart { /* ... */ }     ──► [logs] routing_key: logs.{attempt_id}
    ├── BuildLogMsg { line: "..." }     ──► [logs] routing_key: logs.{attempt_id}
    ├── BuildLogMsg { line: "..." }     ──► [logs] routing_key: logs.{attempt_id}
    └── BuildLogMsg { line: "..." }     ──► [logs] routing_key: logs.{attempt_id}
```

```
RabbitMQ                  Log Collector            Disk
────────                  ─────────────            ────
build-logs                 LogMessageCollector
  ◄── logs                   matches by attempt_id
       logs.*                writes to file:
                             {log_storage_path}/{attempt_id}.log
```

### `LogFrom` Enum

```rust
pub enum LogFrom {
    Worker(BuildLogMsg),
    Start(BuildLogStart),
}
```

The collector distinguishes between log start (creates the file with metadata
header) and log lines (appends to the file).

---

## Message Format Summary

All messages are JSON-serialized via `serde_json`. Key message types and their
flows:

| Message Type | Producer | Consumer | Exchange |
|-------------|----------|----------|----------|
| `PullRequestEvent` | Webhook Receiver | Evaluation Filter | `github-events` |
| `IssueComment` | Webhook Receiver | Comment Filter | `github-events` |
| `PushEvent` | Webhook Receiver | Push Filter | `github-events` |
| `EvaluationJob` | Eval Filter / Comment Filter | Mass Rebuilder | _(direct queue)_ |
| `BuildJob` | Mass Rebuilder / Comment Filter | Builder | `build-jobs` |
| `BuildResult` | Builder | Comment Poster, Stats | `build-results` |
| `BuildLogMsg` | Builder | Log Collector | `logs` |
| `EventMessage` | Any service | Stats Collector | `stats` |

---

## Failure Modes and Recovery

### Transient Failures

| Failure | Recovery Mechanism |
|---------|-------------------|
| GitHub API 401 (expired token) | `NackRequeue` → retry after token refresh |
| GitHub API 5xx | `NackRequeue` → retry |
| RabbitMQ connection lost | `lapin` reconnect / systemd restart |
| Build timeout | `BuildStatus::TimedOut` → report to GitHub |

### Permanent Failures

| Failure | Handling |
|---------|----------|
| Invalid message JSON | `Ack` (discard) + log error |
| PR force-pushed (SHA gone) | `Ack` (skip) — `MissingSha` |
| GitHub API 4xx (not 401/422) | `Ack` + add `tickborg-internal-error` label |
| Merge conflict | Report failure status to GitHub, `Ack` |

### Dead Letter Behavior

Messages `NackDump`'d (rejected without requeue) are discarded unless a
dead-letter exchange is configured in RabbitMQ. This is used for permanently
invalid messages that should not be retried.
