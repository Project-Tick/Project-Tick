use crate::buildtool::detect_changed_projects;
use crate::checkout::CachedProjectCo;
use crate::commentparser::Subset;
use crate::commitstatus::CommitStatus;
use crate::evalchecker::EvalChecker;
use crate::message::buildjob::BuildJob;
use crate::message::evaluationjob::EvaluationJob;
use crate::tasks::eval::{EvaluationComplete, EvaluationStrategy, StepResult};
use crate::tasks::evaluate::update_labels;

use std::path::Path;

use hubcaps::issues::IssueRef;
use regex::Regex;
use uuid::Uuid;

/// Project Tick specific labels from PR title keywords
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
    TITLE_LABELS
        .iter()
        .filter(|(word, _label)| {
            let re = Regex::new(&format!("\\b{word}\\b")).unwrap();
            re.is_match(&title_lower)
        })
        .map(|(_word, label)| (*label).into())
        .collect()
}

/// Parses Conventional Commit messages to extract affected project scopes
fn parse_commit_scopes(messages: &[String]) -> Vec<String> {
    let scope_re = Regex::new(r"^[a-z]+\(([^)]+)\)").unwrap();
    let colon_re = Regex::new(r"^([a-z0-9_-]+):").unwrap();

    let mut projects: Vec<String> = messages
        .iter()
        .filter_map(|line| {
            let trimmed = line.trim();
            // Try Conventional Commits: "feat(meshmc): ..."
            if let Some(caps) = scope_re.captures(trimmed) {
                Some(caps[1].to_string())
            }
            // Try simple "project: description"
            else if let Some(caps) = colon_re.captures(trimmed) {
                let candidate = caps[1].to_string();
                // Only accept known project names
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

pub struct MonorepoStrategy<'a> {
    job: &'a EvaluationJob,
    issue_ref: &'a IssueRef,
    changed_projects: Option<Vec<String>>,
}

impl<'a> MonorepoStrategy<'a> {
    pub fn new(job: &'a EvaluationJob, issue_ref: &'a IssueRef) -> MonorepoStrategy<'a> {
        Self {
            job,
            issue_ref,
            changed_projects: None,
        }
    }

    async fn tag_from_title(&self) {
        let title = match self.issue_ref.get().await {
            Ok(issue) => issue.title.to_lowercase(),
            Err(_) => return,
        };

        let labels = label_from_title(&title);

        if labels.is_empty() {
            return;
        }

        update_labels(self.issue_ref, &labels, &[]).await;
    }

    fn queue_builds(&self) -> StepResult<Vec<BuildJob>> {
        if let Some(ref projects) = self.changed_projects {
            if !projects.is_empty() && projects.len() <= 15 {
                Ok(vec![BuildJob::new(
                    self.job.repo.clone(),
                    self.job.pr.clone(),
                    Subset::Project,
                    projects.clone(),
                    None,
                    None,
                    Uuid::new_v4().to_string(),
                )])
            } else {
                Ok(vec![])
            }
        } else {
            Ok(vec![])
        }
    }
}

impl EvaluationStrategy for MonorepoStrategy<'_> {
    async fn pre_clone(&mut self) -> StepResult<()> {
        self.tag_from_title().await;
        Ok(())
    }

    async fn on_target_branch(&mut self, _dir: &Path, status: &mut CommitStatus) -> StepResult<()> {
        status
            .set_with_description(
                "Analyzing changed projects",
                hubcaps::statuses::State::Pending,
            )
            .await?;
        Ok(())
    }

    fn after_fetch(&mut self, co: &CachedProjectCo) -> StepResult<()> {
        // Strategy 1: detect from changed files
        let changed_files = co
            .files_changed_from_head(&self.job.pr.head_sha)
            .unwrap_or_default();
        let mut projects = detect_changed_projects(&changed_files);

        // Strategy 2: also parse commit messages for scopes
        let commit_scopes = parse_commit_scopes(
            &co.commit_messages_from_head(&self.job.pr.head_sha)
                .unwrap_or_else(|_| vec!["".to_owned()]),
        );

        for scope in commit_scopes {
            if !projects.contains(&scope) {
                projects.push(scope);
            }
        }

        projects.sort();
        projects.dedup();
        self.changed_projects = Some(projects);

        Ok(())
    }

    async fn after_merge(&mut self, status: &mut CommitStatus) -> StepResult<()> {
        let project_list = self
            .changed_projects
            .as_ref()
            .map(|p| p.join(", "))
            .unwrap_or_else(|| "none".into());
        status
            .set_with_description(
                &format!("Changed: {project_list}"),
                hubcaps::statuses::State::Pending,
            )
            .await?;
        Ok(())
    }

    fn evaluation_checks(&self) -> Vec<EvalChecker> {
        vec![]
    }

    async fn all_evaluations_passed(
        &mut self,
        status: &mut CommitStatus,
    ) -> StepResult<EvaluationComplete> {
        status
            .set_with_description(
                "Scheduling project builds",
                hubcaps::statuses::State::Pending,
            )
            .await?;

        let builds = self.queue_builds()?;
        Ok(EvaluationComplete { builds })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_label_from_title() {
        assert_eq!(
            label_from_title("feat(meshmc): add new block type"),
            vec![String::from("project: meshmc")]
        );
        assert_eq!(
            label_from_title("fix windows build for meshmc"),
            vec![String::from("project: meshmc"), String::from("platform: windows")]
        );
        assert_eq!(
            label_from_title("fix darwin support"),
            vec![String::from("platform: macos")]
        );
        assert_eq!(
            label_from_title("docs: update README"),
            Vec::<String>::new()
        );
    }

    #[test]
    fn test_parse_commit_scopes() {
        let messages = vec![
            "feat(meshmc): add new feature".into(),
            "fix(mnv): resolve segfault".into(),
            "chore: update CI".into(),
            "Merge pull request #123 from feature/xyz".into(),
            "neozip: bump version".into(),
        ];
        let scopes = parse_commit_scopes(&messages);
        assert_eq!(scopes, vec!["meshmc", "mnv", "neozip"]);
    }

    #[test]
    fn test_parse_commit_scopes_unknown() {
        let messages = vec![
            "feat(unknownproject): something".into(),
            "docs: update readme".into(),
        ];
        let scopes = parse_commit_scopes(&messages);
        // "unknownproject" should be included (scope from conventional commit)
        // "docs" should NOT be included (not a known project)
        assert_eq!(scopes, vec!["unknownproject"]);
    }
}
