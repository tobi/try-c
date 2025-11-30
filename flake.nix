{
  description = "try - A fast, interactive CLI tool for managing ephemeral development workspaces";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          default = pkgs.stdenv.mkDerivation {
            pname = "try";
            version = "1.0.0";

            src = ./.;

            nativeBuildInputs = with pkgs; [
              gcc
              gnumake
            ];

            buildInputs = with pkgs; [
              # Add any runtime dependencies here
            ];

            buildPhase = ''
              make
            '';

            installPhase = ''
              mkdir -p $out/bin
              cp dist/try $out/bin/
            '';

            meta = with pkgs.lib; {
              description = "A fast, interactive CLI tool for managing ephemeral development workspaces";
              homepage = "https://github.com/tobi/try-c";
              license = licenses.mit;
              maintainers = [ ];
              platforms = platforms.unix;
            };
          };
        };

        apps.default = flake-utils.lib.mkApp {
          drv = self.packages.${system}.default;
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            gcc
            gnumake
            valgrind  # For memory leak testing
            gdb       # For debugging
          ];

          shellHook = ''
            echo "try development environment"
            echo "Run 'make' to build"
            echo "Run 'make test' to run tests"
          '';
        };
      });
}