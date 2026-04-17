use crate::asynccmd::{AsyncCmd, SpawnedAsyncCmd};
use crate::message::buildresult::BuildStatus;

use std::fmt;
use std::fs;
use std::io::{BufRead, BufReader, Seek, SeekFrom};
use std::path::Path;
use std::process::{Command, Stdio};

use tempfile::tempfile;

/// Identifies which build system a project uses.
#[derive(Clone, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum BuildSystem {
    CMake,
    Meson,
    Autotools,
    Cargo,
    Gradle,
    Make,
    Custom { command: String },
}

impl fmt::Display for BuildSystem {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BuildSystem::CMake => write!(f, "cmake"),
            BuildSystem::Meson => write!(f, "meson"),
            BuildSystem::Autotools => write!(f, "autotools"),
            BuildSystem::Cargo => write!(f, "cargo"),
            BuildSystem::Gradle => write!(f, "gradle"),
            BuildSystem::Make => write!(f, "make"),
            BuildSystem::Custom { command } => write!(f, "custom({command})"),
        }
    }
}

/// Project-specific build configuration.
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct ProjectBuildConfig {
    pub name: String,
    pub path: String,
    pub build_system: BuildSystem,
    pub build_timeout_seconds: u16,
    pub configure_args: Vec<String>,
    pub build_args: Vec<String>,
    pub test_command: Option<Vec<String>>,
}

/// Container image used for isolated builds.
const BUILD_CONTAINER_IMAGE: &str = "localhost/tickborg-builder:latest";

/// The build executor — replaces ofborg's Nix struct.
#[derive(Clone, Debug)]
pub struct BuildExecutor {
    pub build_timeout: u16,
    /// Whether to run builds inside a container for isolation.
    pub use_container: bool,
}

impl BuildExecutor {
    pub fn new(build_timeout: u16) -> Self {
        Self {
            build_timeout,
            use_container: true,
        }
    }

    /// Build a project using its configured build system.
    pub fn build_project(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        if self.use_container {
            self.build_project_containerized(project_root, config)
        } else {
            self.build_project_direct(project_root, config)
        }
    }

    /// Build inside a podman container with full isolation.
    fn build_project_containerized(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let project_dir_str = project_dir.to_string_lossy();

        // Build a shell script that runs configure + build inside the container
        let mut script = String::from("set -e\n");

        // Configure step
        if let Some(configure_args) = self.configure_script_fragment(&config.build_system, config) {
            script.push_str(&configure_args);
            script.push('\n');
        }

        // Build step
        script.push_str(&self.build_script_fragment(&config.build_system, config));

        let timeout = if config.build_timeout_seconds > 0 {
            config.build_timeout_seconds
        } else {
            self.build_timeout
        };

        let mut cmd = Command::new("podman");
        cmd.args([
            "run",
            "--rm",
            // Security: no network access
            "--network=none",
            // Resource limits
            "--memory=4g",
            "--cpus=4",
            // Timeout (podman --timeout kills the container)
            &format!("--timeout={}", timeout),
            // Read-only root filesystem for the container
            "--read-only",
            // Tmpfs for /tmp inside the container
            "--tmpfs=/tmp:rw,size=512m",
            // Drop all capabilities
            "--cap-drop=ALL",
            // No new privileges
            "--security-opt=no-new-privileges",
            // Mount project directory as the only writable volume
            &format!("--volume={}:/build:Z", project_dir_str),
            "--workdir=/build",
            BUILD_CONTAINER_IMAGE,
            "sh", "-c", &script,
        ]);

        self.run(cmd, true)
    }

    /// Build directly on the host (fallback, not recommended).
    fn build_project_direct(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);

        // Run configure/generate step if needed
        if let Some(configure_cmd) = self.configure_command(&project_dir, config) {
            if let Err(log) = self.run(configure_cmd, true) {
                return Err(log);
            }
        }

        let cmd = self.build_command(&project_dir, config);
        self.run(cmd, true)
    }

    /// Generate shell fragment for the configure step (runs inside container).
    fn configure_script_fragment(&self, build_system: &BuildSystem, config: &ProjectBuildConfig) -> Option<String> {
        match build_system {
            BuildSystem::CMake => {
                let mut parts = vec!["cmake -S . -B build -DCMAKE_BUILD_TYPE=Release".to_string()];
                for arg in &config.configure_args {
                    parts.push(shell_escape(arg));
                }
                Some(parts.join(" "))
            }
            BuildSystem::Meson => {
                let mut parts = vec!["meson setup build".to_string()];
                for arg in &config.configure_args {
                    parts.push(shell_escape(arg));
                }
                // Meson setup fails if build dir already exists; handle both cases
                Some(format!(
                    "if [ -d build ]; then meson setup --reconfigure build {}; else {}; fi",
                    config.configure_args.iter().map(|a| shell_escape(a)).collect::<Vec<_>>().join(" "),
                    parts.join(" ")
                ))
            }
            BuildSystem::Autotools => {
                let mut parts = vec!["./configure".to_string()];
                for arg in &config.configure_args {
                    parts.push(shell_escape(arg));
                }
                Some(format!("if [ -x ./configure ]; then {}; fi", parts.join(" ")))
            }
            _ => None,
        }
    }

    /// Generate shell fragment for the build step (runs inside container).
    fn build_script_fragment(&self, build_system: &BuildSystem, config: &ProjectBuildConfig) -> String {
        match build_system {
            BuildSystem::CMake => {
                let mut parts = vec!["cmake --build build --config Release".to_string()];
                for arg in &config.build_args {
                    parts.push(shell_escape(arg));
                }
                parts.join(" ")
            }
            BuildSystem::Meson => {
                let mut parts = vec!["meson compile -C build".to_string()];
                for arg in &config.build_args {
                    parts.push(shell_escape(arg));
                }
                parts.join(" ")
            }
            BuildSystem::Autotools | BuildSystem::Make => {
                let cpus = num_cpus();
                let mut parts = vec![format!("make -j{cpus}")];
                for arg in &config.build_args {
                    parts.push(shell_escape(arg));
                }
                parts.join(" ")
            }
            BuildSystem::Cargo => {
                let mut parts = vec!["cargo build --release".to_string()];
                for arg in &config.build_args {
                    parts.push(shell_escape(arg));
                }
                parts.join(" ")
            }
            BuildSystem::Gradle => {
                // Inside container, always use gradle (wrapper won't exist)
                let mut parts = vec!["if [ -x ./gradlew ]; then ./gradlew build; else gradle build; fi".to_string()];
                for arg in &config.build_args {
                    parts.push(shell_escape(arg));
                }
                parts.join(" ")
            }
            BuildSystem::Custom { command } => {
                command.clone()
            }
        }
    }

    /// Generate the configure/setup command for build systems that need it.
    fn configure_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Option<Command> {
        match &config.build_system {
            BuildSystem::CMake => {
                let mut cmd = Command::new("cmake");
                cmd.arg("-S").arg(".");
                cmd.arg("-B").arg("build");
                cmd.arg("-DCMAKE_BUILD_TYPE=Release");
                for arg in &config.configure_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                Some(cmd)
            }
            BuildSystem::Meson => {
                let build_dir = project_dir.join("build");
                if build_dir.exists() {
                    // Meson doesn't allow re-setup, use reconfigure
                    let mut cmd = Command::new("meson");
                    cmd.arg("setup").arg("--reconfigure").arg("build");
                    for arg in &config.configure_args {
                        cmd.arg(arg);
                    }
                    cmd.current_dir(project_dir);
                    Some(cmd)
                } else {
                    let mut cmd = Command::new("meson");
                    cmd.arg("setup").arg("build");
                    for arg in &config.configure_args {
                        cmd.arg(arg);
                    }
                    cmd.current_dir(project_dir);
                    Some(cmd)
                }
            }
            BuildSystem::Autotools => {
                let configure = project_dir.join("configure");
                if configure.exists() {
                    let mut cmd = Command::new("./configure");
                    for arg in &config.configure_args {
                        cmd.arg(arg);
                    }
                    cmd.current_dir(project_dir);
                    Some(cmd)
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    /// Build a project asynchronously.
    pub fn build_project_async(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> SpawnedAsyncCmd {
        let project_dir = project_root.join(&config.path);
        let cmd = self.build_command(&project_dir, config);
        AsyncCmd::new(cmd).spawn()
    }

    /// Run tests for a project.
    pub fn test_project(
        &self,
        project_root: &Path,
        config: &ProjectBuildConfig,
    ) -> Result<fs::File, fs::File> {
        let project_dir = project_root.join(&config.path);
        let cmd = self.test_command(&project_dir, config);
        self.run(cmd, true)
    }

    fn build_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Command {
        match &config.build_system {
            BuildSystem::CMake => {
                let build_dir = project_dir.join("build");
                let mut cmd = Command::new("cmake");
                cmd.arg("--build").arg(&build_dir);
                cmd.args(["--config", "Release"]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Meson => {
                let mut cmd = Command::new("meson");
                cmd.arg("compile");
                cmd.args(["-C", "build"]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Autotools => {
                let mut cmd = Command::new("make");
                cmd.args(["-j", &num_cpus().to_string()]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Cargo => {
                let mut cmd = Command::new("cargo");
                cmd.arg("build").arg("--release");
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Gradle => {
                let gradlew = project_dir.join("gradlew");
                let prog = if gradlew.exists() {
                    gradlew.to_string_lossy().to_string()
                } else {
                    "gradle".to_string()
                };
                let mut cmd = Command::new(prog);
                cmd.arg("build");
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Make => {
                let mut cmd = Command::new("make");
                cmd.args(["-j", &num_cpus().to_string()]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Custom { command } => {
                let mut cmd = Command::new("sh");
                cmd.args(["-c", command]);
                for arg in &config.build_args {
                    cmd.arg(arg);
                }
                cmd.current_dir(project_dir);
                cmd
            }
        }
    }

    fn test_command(&self, project_dir: &Path, config: &ProjectBuildConfig) -> Command {
        if let Some(ref test_cmd) = config.test_command {
            let mut cmd = Command::new(&test_cmd[0]);
            for arg in &test_cmd[1..] {
                cmd.arg(arg);
            }
            cmd.current_dir(project_dir);
            return cmd;
        }

        match &config.build_system {
            BuildSystem::CMake => {
                let mut cmd = Command::new("ctest");
                cmd.arg("--test-dir").arg("build");
                cmd.args(["--output-on-failure"]);
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Meson => {
                let mut cmd = Command::new("meson");
                cmd.arg("test").args(["-C", "build"]);
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Autotools | BuildSystem::Make => {
                let mut cmd = Command::new("make");
                cmd.arg("check");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Cargo => {
                let mut cmd = Command::new("cargo");
                cmd.arg("test").arg("--release");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Gradle => {
                let gradlew = project_dir.join("gradlew");
                let prog = if gradlew.exists() {
                    gradlew.to_string_lossy().to_string()
                } else {
                    "gradle".to_string()
                };
                let mut cmd = Command::new(prog);
                cmd.arg("test");
                cmd.current_dir(project_dir);
                cmd
            }
            BuildSystem::Custom { command } => {
                let mut cmd = Command::new("sh");
                cmd.args(["-c", command]);
                cmd.current_dir(project_dir);
                cmd
            }
        }
    }

    pub fn run(&self, mut cmd: Command, keep_stdout: bool) -> Result<fs::File, fs::File> {
        let stderr = tempfile().expect("Fetching a stderr tempfile");
        let mut reader = stderr.try_clone().expect("Cloning stderr to the reader");

        let stdout: Stdio = if keep_stdout {
            Stdio::from(stderr.try_clone().expect("Cloning stderr for stdout"))
        } else {
            Stdio::null()
        };

        let status = cmd
            .stdout(stdout)
            .stderr(Stdio::from(stderr))
            .status()
            .expect("Running build command ...");

        reader
            .seek(SeekFrom::Start(0))
            .expect("Seeking to Start(0)");

        if status.success() {
            Ok(reader)
        } else {
            Err(reader)
        }
    }
}

fn num_cpus() -> usize {
    std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4)
}

/// Escape a string for safe use in a shell command.
fn shell_escape(s: &str) -> String {
    if s.chars().all(|c| c.is_alphanumeric() || c == '-' || c == '_' || c == '.' || c == '/' || c == '=') {
        s.to_string()
    } else {
        format!("'{}'", s.replace('\'', "'\\''"))
    }
}

pub fn lines_from_file(file: fs::File) -> Vec<String> {
    BufReader::new(file)
        .lines()
        .map_while(Result::ok)
        .collect()
}

pub fn wait_for_build_status(spawned: SpawnedAsyncCmd) -> BuildStatus {
    match spawned.wait() {
        Ok(s) => match s.code() {
            Some(0) => BuildStatus::Success,
            Some(_code) => BuildStatus::Failure,
            None => BuildStatus::UnexpectedError {
                err: "process terminated by signal".into(),
            },
        },
        Err(err) => BuildStatus::UnexpectedError {
            err: format!("failed on interior command {err}"),
        },
    }
}

/// Known projects in the Project Tick monorepo.
pub fn known_projects() -> Vec<ProjectBuildConfig> {
    vec![
        ProjectBuildConfig {
            name: "mnv".into(),
            path: "mnv".into(),
            build_system: BuildSystem::Autotools,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: Some(vec!["make".into(), "check".into()]),
        },
        ProjectBuildConfig {
            name: "cgit".into(),
            path: "cgit".into(),
            build_system: BuildSystem::Make,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "cmark".into(),
            path: "cmark".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "neozip".into(),
            path: "neozip".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "genqrcode".into(),
            path: "genqrcode".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "json4cpp".into(),
            path: "json4cpp".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "tomlplusplus".into(),
            path: "tomlplusplus".into(),
            build_system: BuildSystem::Meson,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "libnbtplusplus".into(),
            path: "libnbtplusplus".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "meshmc".into(),
            path: "meshmc".into(),
            build_system: BuildSystem::CMake,
            build_timeout_seconds: 3600,
            configure_args: vec![],
            build_args: vec!["--config".into(), "Release".into()],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "forgewrapper".into(),
            path: "forgewrapper".into(),
            build_system: BuildSystem::Gradle,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
        ProjectBuildConfig {
            name: "corebinutils".into(),
            path: "corebinutils".into(),
            build_system: BuildSystem::Make,
            build_timeout_seconds: 1800,
            configure_args: vec![],
            build_args: vec![],
            test_command: None,
        },
    ]
}

/// Look up a project by name.
pub fn find_project(name: &str) -> Option<ProjectBuildConfig> {
    known_projects().into_iter().find(|p| p.name == name)
}

/// Detect which projects changed based on modified file paths.
pub fn detect_changed_projects(changed_files: &[String]) -> Vec<String> {
    let projects = known_projects();
    let mut changed = Vec::new();

    for project in &projects {
        let prefix = format!("{}/", project.path);
        if changed_files.iter().any(|f| f.starts_with(&prefix)) {
            changed.push(project.name.clone());
        }
    }

    changed.sort();
    changed.dedup();
    changed
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_detect_changed_projects() {
        let files = vec![
            "mnv/src/main.c".into(),
            "mnv/Makefile.am".into(),
            "meshmc/CMakeLists.txt".into(),
            "README.md".into(),
        ];
        let changed = detect_changed_projects(&files);
        assert_eq!(changed, vec!["meshmc", "mnv"]);
    }

    #[test]
    fn test_detect_changed_projects_none() {
        let files = vec!["README.md".into(), ".gitignore".into()];
        let changed = detect_changed_projects(&files);
        assert!(changed.is_empty());
    }

    #[test]
    fn test_find_project() {
        assert!(find_project("meshmc").is_some());
        assert!(find_project("nonexistent").is_none());
        assert_eq!(
            find_project("meshmc").unwrap().build_system,
            BuildSystem::CMake
        );
        assert_eq!(
            find_project("forgewrapper").unwrap().build_system,
            BuildSystem::Gradle
        );
    }

    #[test]
    fn test_build_system_display() {
        assert_eq!(BuildSystem::CMake.to_string(), "cmake");
        assert_eq!(BuildSystem::Meson.to_string(), "meson");
        assert_eq!(BuildSystem::Cargo.to_string(), "cargo");
        assert_eq!(BuildSystem::Gradle.to_string(), "gradle");
    }
}
