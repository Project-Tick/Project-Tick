{
  description = "A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once (Fork of MultiMC)";

  nixConfig = {
    extra-substituters = [ "https://meshmc.cachix.org" ];
    extra-trusted-public-keys = [
      "meshmc.cachix.org-1:6ZNLcfqjVDKmN9/XNWGV3kcjBTL51v1v2V+cvanMkZA="
    ];
  };

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:

    let
      inherit (nixpkgs) lib;

      # While we only officially support aarch and x86_64 on Linux and MacOS,
      # we expose a reasonable amount of other systems for users who want to
      # build for most exotic platforms
      systems = lib.systems.flakeExposed;

      forAllSystems = lib.genAttrs systems;
      nixpkgsFor = forAllSystems (system: nixpkgs.legacyPackages.${system});
    in

    {
      devShells = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};
          llvm = pkgs.llvmPackages_22;
          python = pkgs.python3;
          mkShell = pkgs.mkShell.override { inherit (llvm) stdenv; };

          packages' = self.packages.${system};

          welcomeMessage = ''
            Welcome to MeshMC!

            We just set some things up for you. To get building, you can run:

            ```
            $ cd "$cmakeBuildDir"
            $ ninjaBuildPhase
            $ ninjaInstallPhase
            ```

            Feel free to ask any questions in our Discord server or Matrix space:
              - https://discord.gg/meshmc
              - https://matrix.to/#/#meshmc:matrix.org

            And thanks for helping out :)
          '';

          # Re-use our package wrapper to wrap our development environment
          qt-wrapper-env = packages'.meshmc.overrideAttrs (old: {
            name = "qt-wrapper-env";

            # Required to use script-based makeWrapper below
            strictDeps = true;

            # We don't need/want the unwrapped MeshMC package
            paths = [ ];

            nativeBuildInputs = old.nativeBuildInputs or [ ] ++ [
              # Ensure the wrapper is script based so it can be sourced
              pkgs.makeWrapper
            ];

            # Inspired by https://discourse.nixos.org/t/python-qt-woes/11808/10
            buildCommand = ''
              makeQtWrapper ${lib.getExe pkgs.runtimeShellPackage} "$out"
              sed -i '/^exec/d' "$out"
            '';
          });
        in

        {
          default = mkShell {
            name = "meshmc";

            inputsFrom = [ packages'.meshmc-unwrapped ];

            packages = [
              pkgs.ccache
              llvm.clang-tools
              python # NOTE(@getchoo): Required for run-clang-tidy, etc.

              (pkgs.stdenvNoCC.mkDerivation {
                pname = "clang-tidy-diff";
                inherit (llvm.clang) version;

                nativeBuildInputs = [
                  pkgs.installShellFiles
                  python.pkgs.wrapPython
                ];

                dontUnpack = true;
                dontConfigure = true;
                dontBuild = true;

                postInstall = "installBin ${llvm.libclang.python}/share/clang/clang-tidy-diff.py";
                postFixup = "wrapPythonPrograms";
              })
            ];

            cmakeBuildType = "Debug";
            cmakeFlags = [ "-GNinja" ] ++ packages'.meshmc-unwrapped.cmakeFlags;
            dontFixCmake = true;

            shellHook = ''
              echo "Sourcing ${qt-wrapper-env}"
              source ${qt-wrapper-env}

              git submodule update --init --force

              if [ ! -f compile_commands.json ]; then
                cmakeConfigurePhase
                cd ..
                ln -s "$cmakeBuildDir"/compile_commands.json compile_commands.json
              fi

              echo ${lib.escapeShellArg welcomeMessage}
            '';
          };
        }
      );

      checks = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};
          llvm = pkgs.llvmPackages_22;
        in

        {
          clang-format = pkgs.runCommand "meshmc-clang-format-check"
            {
              nativeBuildInputs = [ llvm.clang-tools pkgs.bash pkgs.ripgrep ];
              src = self;
            }
            ''
              cd "$src"
              bash ./scripts/check-clang-format.sh
              touch "$out"
            '';
        }
      );

      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt-rfc-style);

      overlays.default =
        final: prev:

        let
          llvm = final.llvmPackages_22 or prev.llvmPackages_22;
        in

        {
          meshmc-unwrapped = prev.callPackage ./nix/unwrapped.nix {
            inherit (llvm) stdenv;
            inherit
              self
              ;
          };

          meshmc = final.callPackage ./nix/wrapper.nix { };
        };

      packages = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};

          # Build a scope from our overlay
          meshmcPackages = lib.makeScope pkgs.newScope (final: self.overlays.default final pkgs);

          # Grab our packages from it and set the default
          packages = {
            inherit (meshmcPackages) meshmc-unwrapped meshmc;
            default = meshmcPackages.meshmc;
          };
        in

        # Only output them if they're available on the current system
        lib.filterAttrs (_: lib.meta.availableOn pkgs.stdenv.hostPlatform) packages
      );

      # We put these under legacyPackages as they are meant for CI, not end user consumption
      legacyPackages = forAllSystems (
        system:

        let
          packages' = self.packages.${system};
          legacyPackages' = self.legacyPackages.${system};
        in

        {
          meshmc-debug = packages'.meshmc.override {
            meshmc-unwrapped = legacyPackages'.meshmc-unwrapped-debug;
          };

          meshmc-unwrapped-debug = packages'.meshmc-unwrapped.overrideAttrs {
            cmakeBuildType = "Debug";
            dontStrip = true;
          };
        }
      );
    };
}
