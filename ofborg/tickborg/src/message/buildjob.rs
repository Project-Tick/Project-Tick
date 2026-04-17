use crate::commentparser::Subset;
use crate::message::{Pr, PushTrigger, Repo};

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub struct BuildJob {
    pub repo: Repo,
    pub pr: Pr,
    pub subset: Option<Subset>,
    pub attrs: Vec<String>,
    pub request_id: String,
    pub logs: Option<ExchangeQueue>, // (Exchange, Routing Key)
    pub statusreport: Option<ExchangeQueue>, // (Exchange, Routing Key)
    /// If set, this build was triggered by a push event, not a PR.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub push: Option<PushTrigger>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub struct QueuedBuildJobs {
    pub job: BuildJob,
    pub architectures: Vec<String>,
}

pub type ExchangeQueue = (Option<Exchange>, Option<RoutingKey>);
type Exchange = String;
type RoutingKey = String;

impl BuildJob {
    pub fn new(
        repo: Repo,
        pr: Pr,
        subset: Subset,
        attrs: Vec<String>,
        logs: Option<ExchangeQueue>,
        statusreport: Option<ExchangeQueue>,
        request_id: String,
    ) -> BuildJob {
        let logbackrk = format!("{}.{}", repo.full_name.to_lowercase(), pr.number);

        BuildJob {
            repo,
            pr,
            subset: Some(subset),
            attrs,
            logs: Some(logs.unwrap_or((Some("logs".to_owned()), Some(logbackrk)))),
            statusreport: Some(statusreport.unwrap_or((Some("build-results".to_owned()), None))),
            request_id,
            push: None,
        }
    }

    /// Create a build job triggered by a push event.
    pub fn new_push(
        repo: Repo,
        push: PushTrigger,
        attrs: Vec<String>,
        request_id: String,
    ) -> BuildJob {
        let logbackrk = format!(
            "{}.push.{}",
            repo.full_name.to_lowercase(),
            push.branch.replace('/', "-")
        );

        // Fill pr with push info so downstream consumers (comment poster, etc.)
        // can still use pr.head_sha for commit statuses / check runs.
        let pr = Pr {
            number: 0,
            head_sha: push.head_sha.clone(),
            target_branch: Some(push.branch.clone()),
        };

        BuildJob {
            repo,
            pr,
            subset: None,
            attrs,
            logs: Some((Some("logs".to_owned()), Some(logbackrk))),
            statusreport: Some((Some("build-results".to_owned()), None)),
            request_id,
            push: Some(push),
        }
    }

    /// Returns true if this build was triggered by a push event.
    pub fn is_push(&self) -> bool {
        self.push.is_some()
    }
}

pub fn from(data: &[u8]) -> Result<BuildJob, serde_json::error::Error> {
    serde_json::from_slice(data)
}

pub struct Actions {
    pub system: String,
}
