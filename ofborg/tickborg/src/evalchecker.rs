use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::process::Command;

/// A generic check that can be run against a checkout
pub struct EvalChecker {
    name: String,
    command: String,
    args: Vec<String>,
}

impl EvalChecker {
    pub fn new(name: &str, command: &str, args: Vec<String>) -> EvalChecker {
        EvalChecker {
            name: name.to_owned(),
            command: command.to_owned(),
            args,
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn execute(&self, path: &Path) -> Result<File, File> {
        let output = Command::new(&self.command)
            .args(&self.args)
            .current_dir(path)
            .output();

        let tmp = tempfile::NamedTempFile::new().expect("Failed to create temp file");
        let tmp_path = tmp.into_temp_path().to_path_buf();

        match output {
            Ok(result) => {
                let mut f = File::create(&tmp_path).expect("Failed to create output file");
                f.write_all(&result.stdout).ok();
                f.write_all(&result.stderr).ok();
                drop(f);
                let file = File::open(&tmp_path).expect("Failed to open output file");
                if result.status.success() {
                    Ok(file)
                } else {
                    Err(file)
                }
            }
            Err(e) => {
                let mut f = File::create(&tmp_path).expect("Failed to create output file");
                write!(f, "Failed to execute {}: {}", self.command, e).ok();
                drop(f);
                Err(File::open(&tmp_path).expect("Failed to open output file"))
            }
        }
    }

    pub fn cli_cmd(&self) -> String {
        let mut cli = vec![self.command.clone()];
        cli.append(&mut self.args.clone());
        cli.join(" ")
    }
}
