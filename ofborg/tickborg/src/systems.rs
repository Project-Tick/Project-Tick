#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum System {
    X8664Linux,
    Aarch64Linux,
    X8664Darwin,
    Aarch64Darwin,
    X8664Windows,
    Aarch64Windows,
    X8664FreeBSD,
}

impl System {
    pub fn all_known_systems() -> Vec<Self> {
        vec![
            Self::X8664Linux,
            Self::Aarch64Linux,
            Self::X8664Darwin,
            Self::Aarch64Darwin,
            Self::X8664Windows,
            Self::Aarch64Windows,
            Self::X8664FreeBSD,
        ]
    }

    /// The primary CI platforms (Linux only for now)
    pub fn primary_systems() -> Vec<Self> {
        vec![
            Self::X8664Linux,
        ]
    }

    /// Systems that can run full test suites
    pub fn can_run_tests(&self) -> bool {
        matches!(
            self,
            System::X8664Linux | System::Aarch64Linux | System::X8664Darwin | System::Aarch64Darwin | System::X8664Windows
        )
    }

    /// GitHub Actions runner label for this system
    pub fn runner_label(&self) -> &'static str {
        match self {
            System::X8664Linux => "ubuntu-latest",
            System::Aarch64Linux => "ubuntu-24.04-arm",
            System::X8664Darwin => "macos-15",
            System::Aarch64Darwin => "macos-15",
            System::X8664Windows => "windows-2025",
            System::Aarch64Windows => "windows-2025-arm",
            System::X8664FreeBSD => "ubuntu-latest", // cross-compile or VM
        }
    }
}

impl std::fmt::Display for System {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            System::X8664Linux => write!(f, "x86_64-linux"),
            System::Aarch64Linux => write!(f, "aarch64-linux"),
            System::X8664Darwin => write!(f, "x86_64-darwin"),
            System::Aarch64Darwin => write!(f, "aarch64-darwin"),
            System::X8664Windows => write!(f, "x86_64-windows"),
            System::Aarch64Windows => write!(f, "aarch64-windows"),
            System::X8664FreeBSD => write!(f, "x86_64-freebsd"),
        }
    }
}

impl System {
    pub fn as_build_destination(&self) -> (Option<String>, Option<String>) {
        (None, Some(format!("build-inputs-{self}")))
    }
}
