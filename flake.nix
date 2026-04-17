{
  description = " Project Tick is a project dedicated to providing developers with ease of use and users with long-lasting software.";

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
            Welcome to Project Tick!
          '';
        in

        {
          default = mkShell {
            name = "project-tick";

            packages = [

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

            shellHook = ''
              git submodule update --init --force

              echo ${lib.escapeShellArg welcomeMessage}
            '';
          };
        }
      );

      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt-rfc-style);
    };
}
