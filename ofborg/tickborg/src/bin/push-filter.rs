use std::env;
use std::error::Error;

use tracing::{error, info};

use tickborg::config;
use tickborg::easyamqp::{self, ChannelExt, ConsumerExt};
use tickborg::easylapin;
use tickborg::tasks;

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    tickborg::setup_log();

    let arg = env::args()
        .nth(1)
        .unwrap_or_else(|| panic!("usage: {} <config>", std::env::args().next().unwrap()));
    let cfg = config::load(arg.as_ref());

    let Some(filter_cfg) = config::load(arg.as_ref()).push_filter else {
        error!("No push filter configuration found!");
        panic!();
    };

    let conn = easylapin::from_config(&filter_cfg.rabbitmq).await?;
    let mut chan = conn.create_channel().await?;

    chan.declare_exchange(easyamqp::ExchangeConfig {
        exchange: "github-events".to_owned(),
        exchange_type: easyamqp::ExchangeType::Topic,
        passive: false,
        durable: true,
        auto_delete: false,
        no_wait: false,
        internal: false,
    })
    .await?;

    // Declare the build-jobs exchange (fanout) that the builder consumes from
    chan.declare_exchange(easyamqp::ExchangeConfig {
        exchange: "build-jobs".to_owned(),
        exchange_type: easyamqp::ExchangeType::Fanout,
        passive: false,
        durable: true,
        auto_delete: false,
        no_wait: false,
        internal: false,
    })
    .await?;

    // Declare the build-results exchange for the comment poster
    chan.declare_exchange(easyamqp::ExchangeConfig {
        exchange: "build-results".to_owned(),
        exchange_type: easyamqp::ExchangeType::Fanout,
        passive: false,
        durable: true,
        auto_delete: false,
        no_wait: false,
        internal: false,
    })
    .await?;

    let queue_name = String::from("push-build-inputs");
    chan.declare_queue(easyamqp::QueueConfig {
        queue: queue_name.clone(),
        passive: false,
        durable: true,
        exclusive: false,
        auto_delete: false,
        no_wait: false,
    })
    .await?;

    chan.bind_queue(easyamqp::BindQueueConfig {
        queue: queue_name.clone(),
        exchange: "github-events".to_owned(),
        routing_key: Some("push.*".to_owned()),
        no_wait: false,
    })
    .await?;

    let handle = easylapin::WorkerChannel(chan)
        .consume(
            tasks::pushfilter::PushFilterWorker::new(
                cfg.acl(),
                filter_cfg.default_attrs,
            ),
            easyamqp::ConsumeConfig {
                queue: queue_name.clone(),
                consumer_tag: format!("{}-push-filter", cfg.whoami()),
                no_local: false,
                no_ack: false,
                no_wait: false,
                exclusive: false,
            },
        )
        .await?;

    info!("Fetching push events from {}", &queue_name);
    handle.await;

    drop(conn); // Close connection.
    info!("Closed the session... EOF");
    Ok(())
}
