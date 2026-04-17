#[derive(serde::Serialize, serde::Deserialize, Debug, Clone)]
pub struct Repo {
    pub owner: String,
    pub name: String,
    pub full_name: String,
    pub clone_url: String,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct Pr {
    pub target_branch: Option<String>,
    pub number: u64,
    pub head_sha: String,
}

/// Information about a push event trigger (direct push to a branch).
#[derive(serde::Serialize, serde::Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct PushTrigger {
    pub head_sha: String,
    pub branch: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub before_sha: Option<String>,
}
