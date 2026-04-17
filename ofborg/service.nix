{ config, lib, pkgs, ... }:

let
  cfg = config.services.tickborg;
  tickborg = (import ./flake.nix).packages.${pkgs.system}.tickborg;

  # Her servis icin ortak systemd ayarlari
  commonServiceConfig = {
    User = "tickborg";
    Group = "tickborg";
    PrivateTmp = true;
    ProtectSystem = "strict";
    ProtectHome = true;
    NoNewPrivileges = true;
    ReadWritePaths = [
      "/var/lib/tickborg"
      "/var/log/tickborg"
    ];
    WorkingDirectory = "/var/lib/tickborg";
    Restart = "always";
    RestartSec = 10;
    Environment = [
      "RUST_LOG=tickborg=info"
      "RUST_BACKTRACE=1"
    ];
  };

  # Her servis icin ortak birim ozellikleri
  mkTickborgService = name: extraConfig: {
    enable = cfg.enable;
    after = [ "network-online.target" "rabbitmq.service" ];
    wants = [ "network-online.target" ];
    wantedBy = [ "multi-user.target" ];
    description = "Tickborg ${name}";

    path = with pkgs; [
      git
      bash
      cmake
      gnumake
      gcc
      pkg-config
      # Meson icin
      meson
      ninja
      # Autotools icin
      autoconf
      automake
      libtool
      # Java/Gradle icin
      jdk17
      # Cargo icin
      rustc
      cargo
    ];

    serviceConfig = commonServiceConfig // (extraConfig.serviceConfig or {});

    script = ''
      export HOME=/var/lib/tickborg
      git config --global user.email "tickborg@project-tick.dev"
      git config --global user.name "TickBorg"
      exec ${tickborg}/bin/${extraConfig.binary} ${cfg.configFile}
    '';
  };

in {
  options.services.tickborg = {
    enable = lib.mkEnableOption "Tickborg CI bot servisleri";

    configFile = lib.mkOption {
      type = lib.types.path;
      default = "/etc/tickborg/config.json";
      description = "Tickborg yapilandirma dosyasi yolu";
    };

    enableBuilder = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Builder servisini etkinlestir";
    };

    enableWebhook = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Webhook receiver servisini etkinlestir";
    };

    enableEvaluationFilter = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Evaluation filter servisini etkinlestir";
    };

    enableCommentFilter = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Comment filter servisini etkinlestir";
    };

    enableCommentPoster = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Comment poster servisini etkinlestir";
    };

    enableMassRebuilder = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Mass rebuilder servisini etkinlestir";
    };

    enableLogCollector = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Log collector servisini etkinlestir";
    };

    enableStats = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Stats (Prometheus metrics) servisini etkinlestir";
    };
  };

  config = lib.mkIf cfg.enable {
    users.users.tickborg = {
      description = "Tickborg CI Bot";
      home = "/var/lib/tickborg";
      createHome = true;
      group = "tickborg";
      isSystemUser = true;
    };
    users.groups.tickborg = {};

    systemd.tmpfiles.rules = [
      "d /var/lib/tickborg 0750 tickborg tickborg -"
      "d /var/lib/tickborg/checkout 0750 tickborg tickborg -"
      "d /var/log/tickborg 0750 tickborg tickborg -"
    ];

    systemd.services = lib.mkMerge [
      (lib.mkIf cfg.enableWebhook {
        "tickborg-webhook-receiver" = mkTickborgService "Webhook Receiver" {
          binary = "github_webhook_receiver";
        };
      })

      (lib.mkIf cfg.enableEvaluationFilter {
        "tickborg-evaluation-filter" = mkTickborgService "Evaluation Filter" {
          binary = "evaluation_filter";
        };
      })

      (lib.mkIf cfg.enableCommentFilter {
        "tickborg-comment-filter" = mkTickborgService "Comment Filter" {
          binary = "github_comment_filter";
        };
      })

      (lib.mkIf cfg.enableCommentPoster {
        "tickborg-comment-poster" = mkTickborgService "Comment Poster" {
          binary = "github_comment_poster";
        };
      })

      (lib.mkIf cfg.enableBuilder {
        "tickborg-builder" = mkTickborgService "Builder" {
          binary = "builder";
          serviceConfig = {
            # Builder daha fazla kaynak kullanabilir
            MemoryMax = "8G";
            CPUQuota = "400%";
          };
        };
      })

      (lib.mkIf cfg.enableMassRebuilder {
        "tickborg-mass-rebuilder" = mkTickborgService "Mass Rebuilder" {
          binary = "mass_rebuilder";
        };
      })

      (lib.mkIf cfg.enableLogCollector {
        "tickborg-log-collector" = mkTickborgService "Log Collector" {
          binary = "log_message_collector";
        };
      })

      (lib.mkIf cfg.enableStats {
        "tickborg-stats" = mkTickborgService "Stats" {
          binary = "stats";
        };
      })
    ];
  };
}
