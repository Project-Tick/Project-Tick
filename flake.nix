{
  inputs = {
    garnix-lib.url = "github:garnix-io/garnix-lib";
    Haskell.url = "github:garnix-io/haskell-module";
    UptimeKuma.url = "github:garnix-io/uptime-kuma-module";
    User.url = "github:garnix-io/user-module";
    Rust.url = "github:garnix-io/rust-module";
    NodeJS.url = "github:garnix-io/nodejs-module";
    PostgreSQL.url = "github:garnix-io/postgresql-module";
  };

  nixConfig = {
    extra-substituters = [ "https://cache.garnix.io" ];
    extra-trusted-public-keys = [ "cache.garnix.io:CTFPyKSLcx5RMJKfLo5EEPUObbA78b0YQ2DTCJXqr9g=" ];
  };

  outputs = inputs: inputs.garnix-lib.lib.mkModules {
    modules = [
      inputs.Haskell.garnixModules.default
      inputs.UptimeKuma.garnixModules.default
      inputs.User.garnixModules.default
      inputs.Rust.garnixModules.default
      inputs.NodeJS.garnixModules.default
      inputs.PostgreSQL.garnixModules.default
    ];

    config = { pkgs, ... }: {
      haskell = {
        haskell-project = {
          buildDependencies = [  ];
          devTools = [  ];
          ghcVersion = "9.8";
          runtimeDependencies = [  ];
          src = ./.;
          webServer = null;
        };
      };
      uptimeKuma = {
        uptimeKuma-project = {
          path = "/";
          port = 3001;
        };
      };
      user = {
        user-project = {
          authorizedSshKeys = [ "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAID/Oc7+PX9MZjGv36+JI+iY8UuOPuH+2lHyhYswyaF0W froster12@naver.com" ];
          groups = [  ];
          shell = "bash";
          user = "samet";
        };
      };
      rust = {
        rust-project = {
          buildDependencies = [  ];
          devTools = [  ];
          runtimeDependencies = [  ];
          src = ./.;
          webServer = null;
        };
      };
      nodejs = {
        nodejs-project = {
          buildDependencies = [  ];
          devTools = [  ];
          prettier = false;
          runtimeDependencies = [  ];
          src = ./.;
          testCommand = "npm run test";
          webServer = null;
        };
      };
      postgresql = {
        postgresql-project = {
          port = 5432;
        };
      };

      garnix.deployBranch = "master";
    };
  };
}
