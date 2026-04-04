{
  system ? builtins.currentSystem,
  pkgs ? (import ../../ci { inherit system; }).pkgs,
}:

pkgs.callPackage (
  {
    gh,
    importNpmLock,
    mkShell,
    nodejs,
  }:
  mkShell {
    packages = [
      gh
      importNpmLock.hooks.linkNodeModulesHook
      nodejs
    ];

    npmDeps = importNpmLock.buildNodeModules {
      npmRoot = ./.;
      inherit nodejs;
    };
  }
) { }
