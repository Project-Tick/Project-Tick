//! Evaluates the expression like Hydra would, with regards to
//! architecture support and recursed packages.

use std::fs::File;
use std::io::{self, Read};

#[derive(Debug)]
pub enum Error {
    Io(io::Error),
    Internal(Box<dyn std::error::Error>),
    CommandFailed(File),
    StatsParse(File, Result<u64, io::Error>, serde_json::Error),
    UncleanEvaluation(Vec<String>),
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Error {
        Error::Io(e)
    }
}

impl Error {
    pub fn display(self) -> String {
        match self {
            Error::Io(err) => format!("Failed during the setup of executing nix-env: {err:?}"),
            Error::Internal(err) => format!("Internal error: {err:?}"),
            Error::CommandFailed(mut fd) => {
                let mut buffer = Vec::new();
                let read_result = fd.read_to_end(&mut buffer);
                let bufstr = String::from_utf8_lossy(&buffer);

                match read_result {
                    Ok(_) => format!("nix-env failed:\n{bufstr}"),
                    Err(err) => format!(
                        "nix-env failed and loading the error result caused a new error {err:?}\n\n{bufstr}"
                    ),
                }
            }
            Error::UncleanEvaluation(warnings) => {
                format!("nix-env did not evaluate cleanly:\n {warnings:?}")
            }
            Error::StatsParse(mut fd, seek, parse_err) => {
                let mut buffer = Vec::new();
                let read_result = fd.read_to_end(&mut buffer);
                let bufstr = String::from_utf8_lossy(&buffer);

                let mut lines =
                    String::from("Parsing nix-env's performance statistics failed.\n\n");

                if let Err(seek_err) = seek {
                    lines.push_str(&format!(
                        "Additionally, resetting to the beginning of the output failed with:\n{seek_err:?}\n\n"
                    ));
                }

                if let Err(read_err) = read_result {
                    lines.push_str(&format!(
                        "Additionally, loading the output failed with:\n{read_err:?}\n\n"
                    ));
                }

                lines.push_str(&format!("Parse error:\n{parse_err:?}\n\n"));

                lines.push_str(&format!("Evaluation output:\n{bufstr}"));

                lines
            }
        }
    }
}
