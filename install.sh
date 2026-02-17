#!/bin/bash
set -e

REPO="r2unit/hyprsync"
INSTALL_DIR="$HOME/.local/bin"
DATA_DIR="$HOME/.local/share/hyprsync"

echo "hyprsync installer"
echo ""

if ! command -v curl &> /dev/null; then
    echo "error: curl is required but not installed"
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo "error: git is required but not installed"
    exit 1
fi

if ! command -v rsync &> /dev/null; then
    echo "error: rsync is required but not installed"
    exit 1
fi

echo "fetching latest release..."
LATEST=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')

if [ -z "$LATEST" ]; then
    echo "error: could not fetch latest release"
    exit 1
fi

echo "latest version: $LATEST"

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case $OS in
    linux)
        case $ARCH in
            x86_64)
                ASSET="hyprsync-linux-amd64"
                ;;
            aarch64|arm64)
                ASSET="hyprsync-linux-arm64"
                ;;
            *)
                echo "error: unsupported architecture: $ARCH"
                exit 1
                ;;
        esac
        ;;
    darwin)
        case $ARCH in
            x86_64)
                ASSET="hyprsync-macos-amd64"
                ;;
            arm64)
                ASSET="hyprsync-macos-arm64"
                ;;
            *)
                echo "error: unsupported architecture: $ARCH"
                exit 1
                ;;
        esac
        ;;
    *)
        echo "error: unsupported operating system: $OS"
        exit 1
        ;;
esac

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$LATEST/$ASSET"

echo "downloading $ASSET..."
mkdir -p "$INSTALL_DIR"
curl -sL "$DOWNLOAD_URL" -o "$INSTALL_DIR/hyprsync"
chmod +x "$INSTALL_DIR/hyprsync"

mkdir -p "$DATA_DIR"
echo "script" > "$DATA_DIR/.hyprsync-install-method"

if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo ""
    echo "note: $INSTALL_DIR is not in your PATH"
    echo "add this to your shell config:"
    echo ""
    if [ "$OS" = "darwin" ]; then
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    else
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
    echo ""
fi

echo ""
echo "hyprsync $LATEST installed successfully"
echo ""
echo "next steps:"
echo "  hyprsync init      # interactive setup"
echo "  hyprsync --help    # show all commands"
