use crate::buildtool::detect_changed_projects;

/// Tags PRs based on which projects were modified.
pub struct ProjectTagger {
    selected: Vec<String>,
}

impl Default for ProjectTagger {
    fn default() -> Self {
        Self {
            selected: vec![],
        }
    }
}

impl ProjectTagger {
    pub fn new() -> Self {
        Default::default()
    }

    /// Analyze changed files and generate project labels.
    pub fn analyze_changes(&mut self, changed_files: &[String]) {
        let projects = detect_changed_projects(changed_files);
        for project in projects {
            self.selected.push(format!("project: {project}"));
        }

        // Check for cross-cutting changes
        let has_ci = changed_files.iter().any(|f| {
            f.starts_with(".github/") || f.starts_with("ci/")
        });
        let has_docs = changed_files.iter().any(|f| {
            f.starts_with("docs/") || f.ends_with(".md")
        });
        let has_root = changed_files.iter().any(|f| {
            !f.contains('/') && !f.ends_with(".md")
        });

        if has_ci {
            self.selected.push("scope: ci".into());
        }
        if has_docs {
            self.selected.push("scope: docs".into());
        }
        if has_root {
            self.selected.push("scope: root".into());
        }
    }

    pub fn tags_to_add(&self) -> Vec<String> {
        self.selected.clone()
    }

    pub fn tags_to_remove(&self) -> Vec<String> {
        vec![]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_project_tagger() {
        let mut tagger = ProjectTagger::new();
        tagger.analyze_changes(&[
            "meshmc/CMakeLists.txt".into(),
            "mnv/src/main.c".into(),
            ".github/workflows/ci.yml".into(),
            "README.md".into(),
        ]);
        let tags = tagger.tags_to_add();
        assert!(tags.contains(&"project: meshmc".into()));
        assert!(tags.contains(&"project: mnv".into()));
        assert!(tags.contains(&"scope: ci".into()));
        assert!(tags.contains(&"scope: docs".into()));
    }

    #[test]
    fn test_project_tagger_no_projects() {
        let mut tagger = ProjectTagger::new();
        tagger.analyze_changes(&["README.md".into()]);
        let tags = tagger.tags_to_add();
        assert!(!tags.iter().any(|t| t.starts_with("project:")));
        assert!(tags.contains(&"scope: docs".into()));
    }
}
