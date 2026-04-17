use crate::message::{Pr, PushTrigger, Repo};

use hubcaps::checks::Conclusion;

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq, Eq)]
pub enum BuildStatus {
    Skipped,
    Success,
    Failure,
    TimedOut,
    HashMismatch,
    UnexpectedError { err: String },
}

impl From<BuildStatus> for String {
    fn from(status: BuildStatus) -> String {
        match status {
            BuildStatus::Skipped => "No attempt".into(),
            BuildStatus::Success => "Success".into(),
            BuildStatus::Failure => "Failure".into(),
            BuildStatus::HashMismatch => "A fixed output derivation's hash was incorrect".into(),
            BuildStatus::TimedOut => "Timed out, unknown build status".into(),
            BuildStatus::UnexpectedError { ref err } => format!("Unexpected error: {err}"),
        }
    }
}

impl From<BuildStatus> for Conclusion {
    fn from(status: BuildStatus) -> Conclusion {
        match status {
            BuildStatus::Skipped => Conclusion::Skipped,
            BuildStatus::Success => Conclusion::Success,
            BuildStatus::Failure => Conclusion::Neutral,
            BuildStatus::HashMismatch => Conclusion::Failure,
            BuildStatus::TimedOut => Conclusion::Neutral,
            BuildStatus::UnexpectedError { .. } => Conclusion::Neutral,
        }
    }
}

pub struct LegacyBuildResult {
    pub repo: Repo,
    pub pr: Pr,
    pub system: String,
    pub output: Vec<String>,
    pub attempt_id: String,
    pub request_id: String,
    pub status: BuildStatus,
    pub skipped_attrs: Option<Vec<String>>,
    pub attempted_attrs: Option<Vec<String>>,
    pub push: Option<PushTrigger>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub enum V1Tag {
    V1,
}

#[derive(serde::Serialize, serde::Deserialize, Debug)]
#[serde(untagged)]
pub enum BuildResult {
    V1 {
        tag: V1Tag, // use serde once all enum variants have a tag
        repo: Repo,
        pr: Pr,
        system: String,
        output: Vec<String>,
        attempt_id: String,
        request_id: String,
        // removed success
        status: BuildStatus,
        skipped_attrs: Option<Vec<String>>,
        attempted_attrs: Option<Vec<String>>,
        #[serde(default, skip_serializing_if = "Option::is_none")]
        push: Option<PushTrigger>,
    },
    Legacy {
        repo: Repo,
        pr: Pr,
        system: String,
        output: Vec<String>,
        attempt_id: String,
        request_id: String,
        success: Option<bool>, // replaced by status
        status: Option<BuildStatus>,
        skipped_attrs: Option<Vec<String>>,
        attempted_attrs: Option<Vec<String>>,
        #[serde(default, skip_serializing_if = "Option::is_none")]
        push: Option<PushTrigger>,
    },
}

impl BuildResult {
    pub fn legacy(&self) -> LegacyBuildResult {
        // TODO: replace this with simpler structs for specific usecases, since
        // it's decouples the structs from serialization.  These can be changed
        // as long as we can translate all enum variants.
        match *self {
            BuildResult::Legacy {
                ref repo,
                ref pr,
                ref system,
                ref output,
                ref attempt_id,
                ref request_id,
                ref attempted_attrs,
                ref skipped_attrs,
                ref push,
                ..
            } => LegacyBuildResult {
                repo: repo.to_owned(),
                pr: pr.to_owned(),
                system: system.to_owned(),
                output: output.to_owned(),
                attempt_id: attempt_id.to_owned(),
                request_id: request_id.to_owned(),
                status: self.status(),
                attempted_attrs: attempted_attrs.to_owned(),
                skipped_attrs: skipped_attrs.to_owned(),
                push: push.to_owned(),
            },
            BuildResult::V1 {
                ref repo,
                ref pr,
                ref system,
                ref output,
                ref attempt_id,
                ref request_id,
                ref attempted_attrs,
                ref skipped_attrs,
                ref push,
                ..
            } => LegacyBuildResult {
                repo: repo.to_owned(),
                pr: pr.to_owned(),
                system: system.to_owned(),
                output: output.to_owned(),
                attempt_id: attempt_id.to_owned(),
                request_id: request_id.to_owned(),
                status: self.status(),
                attempted_attrs: attempted_attrs.to_owned(),
                skipped_attrs: skipped_attrs.to_owned(),
                push: push.to_owned(),
            },
        }
    }

    pub fn pr(&self) -> Pr {
        match self {
            BuildResult::Legacy { pr, .. } => pr.to_owned(),
            BuildResult::V1 { pr, .. } => pr.to_owned(),
        }
    }

    pub fn push(&self) -> Option<PushTrigger> {
        match self {
            BuildResult::Legacy { push, .. } => push.to_owned(),
            BuildResult::V1 { push, .. } => push.to_owned(),
        }
    }

    pub fn is_push(&self) -> bool {
        self.push().is_some()
    }

    pub fn status(&self) -> BuildStatus {
        match *self {
            BuildResult::Legacy {
                ref status,
                ref success,
                ..
            } => status.to_owned().unwrap_or({
                // Fallback for old format.
                match *success {
                    None => BuildStatus::Skipped,
                    Some(true) => BuildStatus::Success,
                    Some(false) => BuildStatus::Failure,
                }
            }),
            BuildResult::V1 { ref status, .. } => status.to_owned(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json;

    #[test]
    fn v1_serialization() {
        let input = r#"{"tag":"V1","repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","status":"Success","skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#;
        let result: BuildResult = serde_json::from_str(input).expect("result required");
        assert_eq!(result.status(), BuildStatus::Success);
        let output = serde_json::to_string(&result).expect("json required");
        assert_eq!(
            output,
            r#"{"tag":"V1","repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","status":"Success","skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#,
            "json of: {:?}",
            result
        );
    }

    #[test]
    fn legacy_serialization() {
        let input = r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","success":true,"status":"Success","skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#;
        let result: BuildResult = serde_json::from_str(input).expect("result required");
        assert_eq!(result.status(), BuildStatus::Success);
        let output = serde_json::to_string(&result).expect("json required");
        assert_eq!(
            output,
            r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","success":true,"status":"Success","skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#,
            "json of: {:?}",
            result
        );
    }

    #[test]
    fn legacy_none_serialization() {
        let input = r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":[],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id"}"#;
        let result: BuildResult = serde_json::from_str(input).expect("result required");
        assert_eq!(result.status(), BuildStatus::Skipped);
        let output = serde_json::to_string(&result).expect("json required");
        assert_eq!(
            output,
            r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":[],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","success":null,"status":null,"skipped_attrs":null,"attempted_attrs":null}"#,
            "json of: {:?}",
            result
        );
    }

    #[test]
    fn legacy_no_status_serialization() {
        let input = r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","success":true,"status":null,"skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#;
        let result: BuildResult = serde_json::from_str(input).expect("result required");
        assert_eq!(result.status(), BuildStatus::Success);
        let output = serde_json::to_string(&result).expect("json required");
        assert_eq!(
            output,
            r#"{"repo":{"owner":"project-tick","name":"Project-Tick","full_name":"project-tick/Project-Tick","clone_url":"https://github.com/project-tick/Project-Tick.git"},"pr":{"target_branch":"master","number":42,"head_sha":"0000000000000000000000000000000000000000"},"system":"x86_64-linux","output":["unpacking sources"],"attempt_id":"attempt-id-foo","request_id":"bogus-request-id","success":true,"status":null,"skipped_attrs":["AAAAAASomeThingsFailToEvaluate"],"attempted_attrs":["hello"]}"#,
            "json of: {:?}",
            result
        );
    }
}
