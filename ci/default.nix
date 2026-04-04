let
  pinned = (builtins.fromJSON (builtins.readFile ./pinned.json)).pins;
in
{
  system ? builtins.currentSystem,
  nixpkgs ? null,
}:
let
  nixpkgs' =
    if nixpkgs == null then
      fetchTarball {
        inherit (pinned.nixpkgs) url;
        sha256 = pinned.nixpkgs.hash;
      }
    else
      nixpkgs;

  pkgs = import nixpkgs' {
    inherit system;
    config = { };
    overlays = [ ];
  };

  fmt =
    let
      treefmtNixSrc = fetchTarball {
        inherit (pinned.treefmt-nix) url;
        sha256 = pinned.treefmt-nix.hash;
      };
      treefmtEval = (import treefmtNixSrc).evalModule pkgs {
        projectRootFile = ".git/config";

        settings.verbose = 1;
        settings.on-unmatched = "debug";

        programs.actionlint.enable = true;

        programs.biome = {
          enable = true;
          validate.enable = false;
          settings.formatter = {
            useEditorconfig = true;
          };
          settings.javascript.formatter = {
            quoteStyle = "single";
            semicolons = "asNeeded";
          };
          settings.json.formatter.enabled = false;
        };
        settings.formatter.biome.excludes = [
          "*.min.js"
        ];

        programs.keep-sorted.enable = true;

        programs.nixfmt = {
          enable = true;
          package = pkgs.nixfmt;
        };

        programs.yamlfmt = {
          enable = true;
          settings.formatter = {
            retain_line_breaks = true;
          };
        };

        programs.zizmor.enable = true;
      };
      fs = pkgs.lib.fileset;
      src = fs.toSource {
        root = ../.;
        fileset = fs.difference ../. (fs.maybeMissing ../.git);
      };
    in
    {
      shell = treefmtEval.config.build.devShell;
      pkg = treefmtEval.config.build.wrapper;
      check = treefmtEval.config.build.check src;
    };

in
rec {
  inherit pkgs fmt;
  codeownersValidator = pkgs.callPackage ./codeowners-validator { };

  shell = pkgs.mkShell {
    packages = [
      fmt.pkg
      codeownersValidator
    ];
  };
}
