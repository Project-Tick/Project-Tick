use crate::ghevent::Repository;

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub struct PushEvent {
    #[serde(rename = "ref")]
    pub git_ref: String,
    pub before: String,
    pub after: String,
    pub created: bool,
    pub deleted: bool,
    pub forced: bool,
    pub repository: Repository,
    pub pusher: Pusher,
    pub head_commit: Option<HeadCommit>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub struct Pusher {
    pub name: String,
    pub email: Option<String>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub struct HeadCommit {
    pub id: String,
    pub message: String,
    pub timestamp: String,
    pub added: Option<Vec<String>>,
    pub removed: Option<Vec<String>>,
    pub modified: Option<Vec<String>>,
}

impl PushEvent {
    /// Branch adını döndürür (refs/heads/main -> main)
    pub fn branch(&self) -> Option<&str> {
        self.git_ref.strip_prefix("refs/heads/")
    }

    /// Tag push mi?
    pub fn is_tag(&self) -> bool {
        self.git_ref.starts_with("refs/tags/")
    }

    /// Branch silme event'i mi?
    pub fn is_delete(&self) -> bool {
        self.deleted
    }

    /// Boş commit (000...) mi?
    pub fn is_zero_sha(&self) -> bool {
        self.after.chars().all(|c| c == '0')
    }
}
