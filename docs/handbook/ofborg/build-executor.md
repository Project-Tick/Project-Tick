# Tickborg — Build Executor

## Overview

The **build executor** is the component responsible for actually running builds
of sub-projects in the Project Tick monorepo. Unlike the original ofborg which
used `nix-build` exclusively, tickborg's build executor supports multiple build
systems: CMake, Meson, Autotools, Cargo, Gradle, Make, and custom commands.

The build executor is invoked by the **builder** binary
(`tickborg/src/bin/builder.rs`) which consumes `BuildJob` messages from
architecture-specific queues.

---

## Key Source Files

| File | Purpose |
|------|---------|
| `tickborg/src/buildtool.rs` | Build system abstraction, `BuildExecutor`, `ProjectBuildConfig` |
| `tickborg/src/tasks/build.rs` | `BuildWorker`, `JobActions` — the task implementation |
| `tickborg/src/bin/builder.rs` | Binary entry point |
| `tickborg/src/asynccmd.rs` | Async subprocess execution |

---

## Build System Abstraction

### `BuildSystem` Enum

```rust
// tickborg/src/buildtool.rs
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum BuildSystem {
    CMake,
    Meson,
    Autotools,
    Cargo,
    Gradle,
    Make,
    Custom { command: String },
}
```

Each variant corresponds to a well-known build system with a standard
invocation pattern.

### `ProjectBuildConfig`

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ProjectBuildConfig {
    pub name: String,
    pub path: String,
    pub build_system: BuildSystem,
    pub build_timeout_seconds: u16,
    pub configure_args: Vec<String>,
    pub build_args: Vec<String>,
    pub test_command: Option<Vec<String>>,
}
```

Each sub-project in the monorepo has a `ProjectBuildConfig` that specifies:
- **name**: Human-readable project name (e.g., `"meshmc"`, `"mnv"`)
- **path**: Relative path within the repository
- **build_system**: Which build system to use
- **build_timeout_seconds**: Maximum time allowed for the build
- **configure_args**: Arguments passed to the configure step
- **build_args**: Arguments passed to the build step
- **test_command**: Custom test command (overrides the default for the build system)

### `BuildExecutor`

```rust
#[derive(Clone, Debug)]
pub struct BuildExecutor {
    pub build_timeout: u16,
}

impl BuildExecutor {
    pub fn new(build_timeout: u16) -> Self {
        Self { build_timeout }
    }
}
```

The `BuildExecutor` is created from the configuration with a minimum timeout
of 300 seconds:

```rust
// config.rs
impl Config {
    pub fn build_executor(&self) -> BuildExecutor {
        if self.build.build_timeout_seconds < 300 {
            error!(?self.build.build_timeout_seconds,
                "Please set build_timeout_seconds to at least 300");
            panic!();
        }
        BuildExecutor::new(self.build.build_timeout_seconds)
    }
}
```

---

## Build Commands Per System

### CMake

```rust
fn build_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Command {
    let build_dir = project_dir.join("build");
    let mut cmd = Command::new("cmake");
    cmd.arg("--build").arg(&build_dir);
    cmd.args(["--config", "Release"]);
    for arg in &config.build_args { cmd.arg(arg); }
    cmd.current_dir(project_dir);
    cmd
}
```

Test command (default):
```rust
let mut cmd = Command::new("ctest");
cmd.arg("--test-dir").arg("build");
cmd.args(["--output-on-failure"]);
```

### Meson

```rust
let mut cmd = Command::new("meson");
cmd.arg("compile");
cmd.args(["-C", "build"]);
```

Test:
```rust
let mut cmd = Command::new("meson");
cmd.arg("test").args(["-C", "build"]);
```

### Autotools / Make

```rust
let mut cmd = Command::new("make");
cmd.args(["-j", &num_cpus().to_string()]);
```

Test:
```rust
let mut cmd = Command::new("make");
cmd.arg("check");
```

### Cargo

```rust
let mut cmd = Command::new("cargo");
cmd.arg("build").arg("--release");
```

Test:
```rust
let mut cmd = Command::new("cargo");
cmd.arg("test");
```

### Gradle

```rust
let gradlew = project_dir.join("gradlew");
let prog = if gradlew.exists() {
    gradlew.to_string_lossy().to_string()
} else {
    "gradle".to_string()
};
let mut cmd = Command::new(prog);
cmd.arg("build");
```

Gradle prefers the wrapper (`gradlew`) if present.

### Custom

```rust
let mut cmd = Command::new("sh");
cmd.args(["-c", command]);
```

---

## Build Execution Methods

### Synchronous Build

```rust
impl BuildExecutor {
    pub fn build_project(
        &self, project_root: &Path, config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let cmd = self.build_command(&project_dir, config);
        self.run(cmd, true)
    }
}
```

Returns `Ok(File)` with stdout/stderr on success, `Err(File)` on failure.
The `File` contains the captured output.

### Asynchronous Build

```rust
impl BuildExecutor {
    pub fn build_project_async(
        &self, project_root: &Path, config: &ProjectBuildConfig,
    ) -> SpawnedAsyncCmd {
        let project_dir = project_root.join(&config.path);
        let cmd = self.build_command(&project_dir, config);
        AsyncCmd::new(cmd).spawn()
    }
}
```

Returns a `SpawnedAsyncCmd` that allows streaming output line-by-line.

### Test Execution

```rust
impl BuildExecutor {
    pub fn test_project(
        &self, project_root: &Path, config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let cmd = self.test_command(&project_dir, config);
        self.run(cmd, true)
    }
}
```

If `config.test_command` is set, it is used directly. Otherwise, the default
test command for the build system is used.

---

## Async Command Execution (`asynccmd.rs`)

The `AsyncCmd` abstraction wraps `std::process::Command` to provide:
- Non-blocking output streaming via channels
- Separate stderr/stdout capture
- Exit status monitoring

```rust
pub struct AsyncCmd {
    command: Command,
}

pub struct SpawnedAsyncCmd {
    waiter: JoinHandle<Option<Result<ExitStatus, io::Error>>>,
    rx: Receiver<String>,
}
```

### Spawning

```rust
impl AsyncCmd {
    pub fn new(cmd: Command) -> AsyncCmd {
        AsyncCmd { command: cmd }
    }

    pub fn spawn(mut self) -> SpawnedAsyncCmd {
        let mut child = self.command
            .stdin(Stdio::null())
            .stderr(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .unwrap();

        // Sets up channels and monitoring threads
        // ...
    }
}
```

The spawn implementation:
1. Creates a `sync_channel` for output lines (buffer size: 30).
2. Spawns a reader thread for stdout.
3. Spawns a reader thread for stderr.
4. Spawns a waiter thread for each (stdout thread, stderr thread, child process).
5. Returns a `SpawnedAsyncCmd` whose `rx` receiver yields lines as they arrive.

```rust
fn reader_tx<R: 'static + Read + Send>(
    read: R, tx: SyncSender<String>,
) -> thread::JoinHandle<()> {
    let read = BufReader::new(read);
    thread::spawn(move || {
        for line in read.lines() {
            let to_send = match line {
                Ok(line) => line,
                Err(e) => {
                    error!("Error reading data in reader_tx: {:?}", e);
                    "Non-UTF8 data omitted from the log.".to_owned()
                }
            };
            if let Err(e) = tx.send(to_send) {
                error!("Failed to send log line: {:?}", e);
            }
        }
    })
}
```

The channel buffer size is intentionally small (30) to apply backpressure:

```rust
const OUT_CHANNEL_BUFFER_SIZE: usize = 30;
```

---

## The Builder Binary

### Entry Point

```rust
// src/bin/builder.rs
#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    tickborg::setup_log();

    let arg = env::args().nth(1).unwrap_or_else(|| panic!("usage: ..."));
    let cfg = config::load(arg.as_ref());

    let conn = easylapin::from_config(&builder_cfg.rabbitmq).await?;
    let mut handles: Vec<Pin<Box<dyn Future<Output = ()> + Send>>> = Vec::new();

    for system in &cfg.build.system {
        handles.push(create_handle(&conn, &cfg, system.to_string()).await?);
    }

    future::join_all(handles).await;
}
```

The builder creates one consumer handle per configured system. This allows a
single builder process to serve multiple architectures (e.g., `x86_64-linux`
and `aarch64-linux`).

### Channel Setup

```rust
async fn create_handle(
    conn: &lapin::Connection, cfg: &config::Config, system: String,
) -> Result<Pin<Box<dyn Future<Output = ()> + Send>>, Box<dyn Error>> {
    let mut chan = conn.create_channel().await?;
    let cloner = checkout::cached_cloner(Path::new(&cfg.checkout.root));
    let build_executor = cfg.build_executor();

    // Declare build-jobs exchange (Fanout)
    chan.declare_exchange(/* build-jobs, Fanout */);

    // Declare and bind the system-specific queue
    let queue_name = format!("build-inputs-{system}");
    chan.declare_queue(/* queue_name, durable */);
    chan.bind_queue(/* queue_name ← build-jobs */);

    // Start consuming
    let handle = easylapin::NotifyChannel(chan).consume(
        tasks::build::BuildWorker::new(
            cloner, build_executor, system, cfg.runner.identity.clone()
        ),
        easyamqp::ConsumeConfig {
            queue: queue_name,
            consumer_tag: format!("{}-builder", cfg.whoami()),
            no_local: false, no_ack: false, no_wait: false, exclusive: false,
        },
    ).await?;

    Ok(handle)
}
```

### Development Mode (`build_all_jobs`)

When `runner.build_all_jobs` is set to `true`, the builder creates an
exclusive, auto-delete queue instead of the named durable one:

```rust
if cfg.runner.build_all_jobs != Some(true) {
    // Normal: named durable queue
    let queue_name = format!("build-inputs-{system}");
    chan.declare_queue(QueueConfig { durable: true, exclusive: false, ... });
} else {
    // Dev mode: ephemeral queue (receives ALL jobs)
    warn!("Building all jobs, please don't use this unless ...");
    chan.declare_queue(QueueConfig { durable: false, exclusive: true, auto_delete: true, ... });
}
```

---

## The `BuildWorker`

```rust
// tasks/build.rs
pub struct BuildWorker {
    cloner: checkout::CachedCloner,
    build_executor: buildtool::BuildExecutor,
    system: String,
    identity: String,
}

impl BuildWorker {
    pub fn new(
        cloner: checkout::CachedCloner,
        build_executor: buildtool::BuildExecutor,
        system: String,
        identity: String,
    ) -> BuildWorker { ... }
}
```

The `BuildWorker` implements `SimpleNotifyWorker`, meaning it receives a
`NotificationReceiver` that allows it to stream log lines back during
processing.

---

## `JobActions` — The Streaming Helper

`JobActions` wraps the build job context and provides methods for logging and
reporting:

```rust
pub struct JobActions {
    system: String,
    identity: String,
    receiver: Arc<dyn NotificationReceiver + Send + Sync>,
    job: buildjob::BuildJob,
    line_counter: AtomicU64,
    snippet_log: parking_lot::RwLock<VecDeque<String>>,
    attempt_id: String,
    log_exchange: Option<String>,
    log_routing_key: Option<String>,
    result_exchange: Option<String>,
    result_routing_key: Option<String>,
}
```

### Attempt ID

Each build execution gets a unique UUID v4 `attempt_id`:

```rust
attempt_id: Uuid::new_v4().to_string(),
```

### Snippet Log

The last 10 lines of output are kept in a ring buffer for inclusion in the
build result:

```rust
snippet_log: parking_lot::RwLock::new(VecDeque::with_capacity(10)),
```

### Log Streaming

```rust
impl JobActions {
    pub async fn log_line(&self, line: String) {
        self.line_counter.fetch_add(1, Ordering::SeqCst);

        // Update snippet ring buffer
        {
            let mut snippet_log = self.snippet_log.write();
            if snippet_log.len() >= 10 {
                snippet_log.pop_front();
            }
            snippet_log.push_back(line.clone());
        }

        let msg = buildlogmsg::BuildLogMsg {
            identity: self.identity.clone(),
            system: self.system.clone(),
            attempt_id: self.attempt_id.clone(),
            line_number: self.line_counter.load(Ordering::SeqCst),
            output: line,
        };

        self.tell(worker::publish_serde_action(
            self.log_exchange.clone(),
            self.log_routing_key.clone(),
            &msg,
        )).await;
    }
}
```

Each log line is published as a `BuildLogMsg` to the `logs` exchange in
real-time. The `line_counter` uses `AtomicU64` for thread-safe incrementing.

### Build Start Notification

```rust
pub async fn log_started(&self, can_build: Vec<String>, cannot_build: Vec<String>) {
    let msg = buildlogmsg::BuildLogStart {
        identity: self.identity.clone(),
        system: self.system.clone(),
        attempt_id: self.attempt_id.clone(),
        attempted_attrs: Some(can_build),
        skipped_attrs: Some(cannot_build),
    };
    self.tell(worker::publish_serde_action(
        self.log_exchange.clone(), self.log_routing_key.clone(), &msg,
    )).await;
}
```

### Build Result Reporting

```rust
pub async fn merge_failed(&self) {
    let msg = BuildResult::V1 {
        tag: V1Tag::V1,
        repo: self.job.repo.clone(),
        pr: self.job.pr.clone(),
        system: self.system.clone(),
        output: vec![String::from("Merge failed")],
        attempt_id: self.attempt_id.clone(),
        request_id: self.job.request_id.clone(),
        attempted_attrs: None,
        skipped_attrs: None,
        status: BuildStatus::Failure,
        push: self.job.push.clone(),
    };

    self.tell(worker::publish_serde_action(
        self.result_exchange.clone(),
        self.result_routing_key.clone(),
        &msg,
    )).await;
    self.tell(worker::Action::Ack).await;
}
```

### Other Status Methods

```rust
impl JobActions {
    pub async fn pr_head_missing(&self)     { self.tell(Action::Ack).await; }
    pub async fn commit_missing(&self)      { self.tell(Action::Ack).await; }
    pub async fn nothing_to_do(&self)       { self.tell(Action::Ack).await; }
    pub async fn merge_failed(&self)        { /* publish Failure + Ack */ }
    pub async fn log_started(&self, ...)    { /* publish BuildLogStart */ }
    pub async fn log_line(&self, line)      { /* publish BuildLogMsg */ }
    pub async fn log_instantiation_errors(&self, ...) { /* log each error */ }
    pub fn log_snippet(&self) -> Vec<String> { /* return last 10 lines */ }
}
```

---

## Build Flow

1. **Receive** `BuildJob` from queue
2. **Clone** repository (using `CachedCloner`)
3. **Checkout** target branch
4. **Fetch** PR (if PR-triggered)
5. **Merge** PR into target branch
6. **Determine** which attrs can build on this system
7. **Log start** (`BuildLogStart` message)
8. **For each attr**:
   a. Execute build command
   b. Stream output lines (`BuildLogMsg` messages)
   c. Check exit status
9. **Publish result** (`BuildResult` with `BuildStatus`)
10. **Ack** the original message

---

## Project Detection

The `detect_changed_projects` function in `buildtool.rs` maps changed files
to project names:

```rust
pub fn detect_changed_projects(changed_files: &[String]) -> Vec<String>;
```

It examines the first path component of each changed file and matches it
against known project directories in the monorepo.

The `find_project` function looks up a project by name:

```rust
pub fn find_project(name: &str) -> Option<ProjectBuildConfig>;
```

---

## Build Timeout

The build timeout is enforced at the configuration level:

```rust
pub struct BuildConfig {
    pub system: Vec<String>,
    pub build_timeout_seconds: u16,
    pub extra_env: Option<HashMap<String, String>>,
}
```

The minimum is 300 seconds (5 minutes). This is validated at startup:

```rust
if self.build.build_timeout_seconds < 300 {
    error!("Please set build_timeout_seconds to at least 300");
    panic!();
}
```

When a build times out, the result status is set to `BuildStatus::TimedOut`.

---

## NixOS Service Configuration

The builder has special systemd resource limits:

```nix
# service.nix
"tickborg-builder" = mkTickborgService "Builder" {
    binary = "builder";
    serviceConfig = {
        MemoryMax = "8G";
        CPUQuota = "400%";
    };
};
```

The `CPUQuota = "400%"` allows the builder to use up to 4 CPU cores.

The service PATH includes build tools:

```nix
path = with pkgs; [
    git bash cmake gnumake gcc pkg-config
    meson ninja
    autoconf automake libtool
    jdk17
    rustc cargo
];
```
