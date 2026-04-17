use lapin::message::Delivery;
use std::env;
use std::error::Error;

use tickborg::commentparser;
use tickborg::config;
use tickborg::easylapin;
use tickborg::message::{Pr, Repo, buildjob};
use tickborg::notifyworker::NotificationReceiver;
use tickborg::worker;

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    tickborg::setup_log();

    let arg = env::args().nth(1).expect("usage: build-faker <config>");
    let cfg = config::load(arg.as_ref());

    let conn = easylapin::from_config(&cfg.builder.unwrap().rabbitmq).await?;
    let chan = conn.create_channel().await?;

    let repo_msg = Repo {
        clone_url: "https://github.com/project-tick/Project-Tick.git".to_owned(),
        full_name: "project-tick/Project-Tick".to_owned(),
        owner: "project-tick".to_owned(),
        name: "Project-Tick".to_owned(),
    };

    let pr_msg = Pr {
        number: 42,
        head_sha: "6dd9f0265d52b946dd13daf996f30b64e4edb446".to_owned(),
        target_branch: Some("scratch".to_owned()),
    };

    let logbackrk = "project-tick/Project-Tick.42".to_owned();

    let msg = buildjob::BuildJob {
        repo: repo_msg,
        pr: pr_msg,
        subset: Some(commentparser::Subset::Project),
        attrs: vec!["success".to_owned()],
        logs: Some((Some("logs".to_owned()), Some(logbackrk.to_lowercase()))),
        statusreport: Some((None, Some("scratch".to_owned()))),
        request_id: "bogus-request-id".to_owned(),
        push: None,
    };

    {
        let deliver = Delivery::mock(0, "no-exchange".into(), "".into(), false, vec![]);
        let recv = easylapin::ChannelNotificationReceiver::new(chan.clone(), deliver);

        for _i in 1..2 {
            recv.tell(worker::publish_serde_action(
                None,
                Some("build-inputs-x86_64-darwin".to_owned()),
                &msg,
            ))
            .await;
        }
    }

    Ok(())
}
