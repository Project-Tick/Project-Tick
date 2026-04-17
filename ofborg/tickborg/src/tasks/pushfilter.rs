use crate::acl;
use crate::ghevent;
use crate::message::buildjob;
use crate::message::{PushTrigger, Repo};
use crate::systems;
use crate::worker;

use tracing::{debug_span, info};
use uuid::Uuid;

pub struct PushFilterWorker {
    acl: acl::Acl,
    /// Default projects/attrs to build when push doesn't match any known project.
    default_attrs: Vec<String>,
}

impl PushFilterWorker {
    pub fn new(acl: acl::Acl, default_attrs: Vec<String>) -> PushFilterWorker {
        PushFilterWorker {
            acl,
            default_attrs,
        }
    }
}

impl worker::SimpleWorker for PushFilterWorker {
    type J = ghevent::PushEvent;

    async fn msg_to_job(
        &mut self,
        _: &str,
        _: &Option<String>,
        body: &[u8],
    ) -> Result<Self::J, String> {
        match serde_json::from_slice(body) {
            Ok(event) => Ok(event),
            Err(err) => Err(format!(
                "Failed to deserialize push event {err:?}: {:?}",
                std::str::from_utf8(body).unwrap_or("<job not utf8>")
            )),
        }
    }

    async fn consumer(&mut self, job: &ghevent::PushEvent) -> worker::Actions {
        let branch = job.branch().unwrap_or_default();
        let span = debug_span!("push", branch = %branch, after = %job.after);
        let _enter = span.enter();

        if !self.acl.is_repo_eligible(&job.repository.full_name) {
            info!("Repo not authorized ({})", job.repository.full_name);
            return vec![worker::Action::Ack];
        }

        // Skip tag events
        if job.is_tag() {
            info!("Skipping tag push: {}", job.git_ref);
            return vec![worker::Action::Ack];
        }

        // Skip branch deletion events
        if job.is_delete() {
            info!("Skipping branch delete: {}", job.git_ref);
            return vec![worker::Action::Ack];
        }

        // Skip zero SHA (shouldn't happen for non-delete, but just in case)
        if job.is_zero_sha() {
            info!("Skipping zero SHA push");
            return vec![worker::Action::Ack];
        }

        let branch_name = branch.to_string();
        info!(
            "Processing push to {}:{} ({})",
            job.repository.full_name, branch_name, job.after
        );

        // Detect which projects changed from the push event's commit info
        let changed_files: Vec<String> = if let Some(ref head) = job.head_commit {
            let mut files = Vec::new();
            if let Some(ref added) = head.added {
                files.extend(added.iter().cloned());
            }
            if let Some(ref removed) = head.removed {
                files.extend(removed.iter().cloned());
            }
            if let Some(ref modified) = head.modified {
                files.extend(modified.iter().cloned());
            }
            files
        } else {
            Vec::new()
        };

        let attrs = if !changed_files.is_empty() {
            let detected = crate::buildtool::detect_changed_projects(&changed_files);
            if detected.is_empty() {
                info!("No known projects changed in push, using defaults");
                self.default_attrs.clone()
            } else {
                info!("Detected changed projects: {:?}", detected);
                detected
            }
        } else {
            info!("No file change info in push event, using defaults");
            self.default_attrs.clone()
        };

        if attrs.is_empty() {
            info!("No projects to build, skipping push");
            return vec![worker::Action::Ack];
        }

        let repo_msg = Repo {
            clone_url: job.repository.clone_url.clone(),
            full_name: job.repository.full_name.clone(),
            owner: job.repository.owner.login.clone(),
            name: job.repository.name.clone(),
        };

        let push_trigger = PushTrigger {
            head_sha: job.after.clone(),
            branch: branch_name,
            before_sha: Some(job.before.clone()),
        };

        let request_id = Uuid::new_v4().to_string();

        let build_job = buildjob::BuildJob::new_push(
            repo_msg.clone(),
            push_trigger,
            attrs.clone(),
            request_id,
        );

        // Schedule the build on all known architectures
        let build_archs = systems::System::primary_systems();
        let mut response = vec![];

        info!(
            "Scheduling push build for {:?} on {:?}",
            attrs, build_archs
        );

        for arch in &build_archs {
            let (exchange, routingkey) = arch.as_build_destination();
            response.push(worker::publish_serde_action(
                exchange, routingkey, &build_job,
            ));
        }

        // Also publish to build-results for the comment poster to pick up
        response.push(worker::publish_serde_action(
            Some("build-results".to_string()),
            None,
            &buildjob::QueuedBuildJobs {
                job: build_job,
                architectures: build_archs.iter().map(|a| a.to_string()).collect(),
            },
        ));

        response.push(worker::Action::Ack);
        response
    }
}
