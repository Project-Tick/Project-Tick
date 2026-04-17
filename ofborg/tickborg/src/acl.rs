use crate::systems::System;

pub struct Acl {
    trusted_users: Option<Vec<String>>,
    repos: Vec<String>,
}

impl Acl {
    pub fn new(repos: Vec<String>, mut trusted_users: Option<Vec<String>>) -> Acl {
        if let Some(ref mut users) = trusted_users {
            for user in users.iter_mut() {
                *user = user.to_lowercase();
            }
        }

        let repos = repos.into_iter().map(|r| r.to_lowercase()).collect();

        Acl {
            trusted_users,
            repos,
        }
    }

    pub fn is_repo_eligible(&self, name: &str) -> bool {
        self.repos.contains(&name.to_lowercase())
    }

    pub fn build_job_architectures_for_user_repo(&self, user: &str, repo: &str) -> Vec<System> {
        if self.can_build_unrestricted(user, repo) {
            System::all_known_systems()
        } else {
            // Non-trusted users can only build on primary platforms
            System::primary_systems()
        }
    }

    pub fn build_job_destinations_for_user_repo(
        &self,
        user: &str,
        repo: &str,
    ) -> Vec<(Option<String>, Option<String>)> {
        self.build_job_architectures_for_user_repo(user, repo)
            .iter()
            .map(|system| system.as_build_destination())
            .collect()
    }

    pub fn can_build_unrestricted(&self, user: &str, repo: &str) -> bool {
        if let Some(ref users) = self.trusted_users {
            if repo.to_lowercase().starts_with("project-tick/") {
                users.contains(&user.to_lowercase())
            } else {
                false
            }
        } else {
            // If trusted_users is disabled (and thus None), everybody can build
            // unrestricted
            true
        }
    }
}
