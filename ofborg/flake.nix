{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }@inputs:
    let
      supportedSystems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs supportedSystems (system: f system);
    in
    {
      devShell = forAllSystems (system: inputs.self.devShells.${system}.default);
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };
        in
        {
          default = pkgs.mkShell {
            name = "tickborg-dev";
            nativeBuildInputs = with pkgs; [
              bash
              rustc
              cargo
              clippy
              rustfmt
              pkg-config
              git
              cmake
            ];
            buildInputs =
              with pkgs;
              lib.optionals stdenv.isDarwin [
                darwin.Security
                libiconv
              ];

            postHook = ''
              checkPhase() (
                  cd "${builtins.toString ./.}/ofborg"
                  set -x
                  cargo fmt
                  git diff --exit-code
                  cargofmtexit=$?

                  cargo clippy
                  cargoclippyexit=$?

                  cargo build && cargo test
                  cargotestexit=$?

                  sum=$((cargofmtexit + cargoclippyexit + cargotestexit))
                  exit $sum
              )
            '';

            RUSTFLAGS = "-D warnings";
            RUST_BACKTRACE = "1";
            RUST_LOG = "tickborg=debug";
          };
        }
      );

      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };

          pkg = pkgs.rustPlatform.buildRustPackage {
            name = "tickborg";
            src = pkgs.nix-gitignore.gitignoreSource [ ] ./.;

            nativeBuildInputs = with pkgs; [
              pkg-config
              pkgs.rustPackages.clippy
            ];

            preBuild = ''
              cargo clippy
            '';

            doCheck = false;
            checkInputs = with pkgs; [ ];

            cargoLock = {
              lockFile = ./Cargo.lock;
              outputHashes = {
                "hubcaps-0.6.2" = "sha256-Vl4wQIKQVRxkpQxL8fL9rndAN3TKLV4OjgnZOpT6HRo=";
                "hyperx-1.4.0" = "sha256-MW/KxxMYvj/DYVKrYa7rDKwrH6s8uQOCA0dR2W7GBeg=";
              };
            };
          };

        in
        {
          inherit pkg;
          default = pkg;

          tickborg = pkgs.runCommand "tickborg-rs" { src = pkg; } ''
            mkdir -p $out/bin
            for f in $(find $src -type f); do
              bn=$(basename "$f")
              ln -s "$f" "$out/bin/$bn"

              # Cargo outputs bins with dashes; create underscore symlinks
              if echo "$bn" | grep -q "-"; then
                ln -s "$f" "$out/bin/$(echo "$bn" | tr '-' '_')"
              fi
            done

            test -e $out/bin/builder
            test -e $out/bin/github_comment_filter
            test -e $out/bin/github_comment_poster
            test -e $out/bin/github_webhook_receiver
            test -e $out/bin/log_message_collector
            test -e $out/bin/evaluation_filter
          '';
        }
      );

      hydraJobs = {
        buildRs = forAllSystems (system: self.packages.${system}.tickborg);
      };
    };
}
