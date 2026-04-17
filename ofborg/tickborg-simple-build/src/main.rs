extern crate log;

use std::env;
use std::path::Path;

use tickborg::buildtool;
use tickborg::config;

fn main() {
    tickborg::setup_log();

    log::info!("Loading config...");
    let cfg = config::load(env::args().nth(1).unwrap().as_ref());
    let executor = cfg.build_executor();

    log::info!("Running build...");
    // Build the first known project as a smoke test
    if let Some(project) = buildtool::known_projects().first() {
        match executor.build_project(Path::new("./"), project) {
            Ok(mut output) => {
                use std::io::Read;
                let mut buf = String::new();
                output.read_to_string(&mut buf).ok();
                print!("{buf}");
            }
            Err(mut output) => {
                use std::io::Read;
                let mut buf = String::new();
                output.read_to_string(&mut buf).ok();
                eprintln!("Build failed:\n{buf}");
            }
        }
    } else {
        log::error!("No projects configured");
    }
}
