{
  description = "Development shell for PvZ-Portable";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          lib = pkgs.lib;
          cmakePrefixes = with pkgs; [
            SDL2.dev
            SDL2.out
            libopenmpt.dev
            libopenmpt.out
            libjpeg.dev
            libjpeg.out
            libpng.dev
            libpng.out
            zlib.dev
            zlib.out
          ];
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake
              ninja
              gcc
              clang-tools
              pkg-config
              python3
            ];

            buildInputs = cmakePrefixes;

            shellHook = ''
              export CMAKE_PREFIX_PATH="${lib.concatStringsSep ":" (map toString cmakePrefixes)}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
            '';
          };
        });
    };
}
