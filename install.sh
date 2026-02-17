#!/bin/bash
set -e

REPO="r2unit/hyprsync"
INSTALL_DIR="$HOME/.local/bin"
DATA_DIR="$HOME/.local/share/hyprsync"
DEV_MODE=false

for arg in "$@"; do
    case $arg in
        --dev)
            DEV_MODE=true
            shift
            ;;
    esac
done

echo "hyprsync installer"
echo ""

detect_pkg_manager() {
    if command -v pacman &> /dev/null; then
        echo "pacman"
    elif command -v apt-get &> /dev/null; then
        echo "apt"
    elif command -v dnf &> /dev/null; then
        echo "dnf"
    elif command -v zypper &> /dev/null; then
        echo "zypper"
    elif command -v brew &> /dev/null; then
        echo "brew"
    else
        echo "unknown"
    fi
}

install_package() {
    local pkg="$1"
    local mgr=$(detect_pkg_manager)

    echo "installing $pkg..."

    case $mgr in
        pacman)
            sudo pacman -S --noconfirm "$pkg"
            ;;
        apt)
            sudo apt-get update -qq && sudo apt-get install -y "$pkg"
            ;;
        dnf)
            sudo dnf install -y "$pkg"
            ;;
        zypper)
            sudo zypper install -y "$pkg"
            ;;
        brew)
            brew install "$pkg"
            ;;
        *)
            echo "error: could not detect package manager"
            echo "please install $pkg manually"
            exit 1
            ;;
    esac
}

check_and_install() {
    local cmd="$1"
    local pkg="${2:-$1}"

    if ! command -v "$cmd" &> /dev/null; then
        echo "$cmd not found"
        install_package "$pkg"
    fi
}

check_and_install curl
check_and_install git
check_and_install rsync
check_and_install ssh openssh

if [ "$DEV_MODE" = true ]; then
    echo "fetching development release..."
    LATEST=$(curl -s "https://api.github.com/repos/$REPO/releases" | grep '"tag_name"' | grep -E 'dev\.' | head -1 | sed -E 's/.*"([^"]+)".*/\1/')

    if [ -z "$LATEST" ]; then
        echo "error: no development release found"
        exit 1
    fi
else
    echo "fetching latest release..."
    LATEST=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')

    if [ -z "$LATEST" ]; then
        echo "error: could not fetch latest release"
        exit 1
    fi
fi

echo "version: $LATEST"

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

COMPLETIONS_URL="https://raw.githubusercontent.com/r2unit/hyprsync/devel/completions"

install_completions() {
    echo "installing shell completions..."

    if [ -n "$BASH_VERSION" ] || [ -f "$HOME/.bashrc" ]; then
        BASH_COMP_DIR="$HOME/.local/share/bash-completion/completions"
        mkdir -p "$BASH_COMP_DIR"
        curl -sL "$COMPLETIONS_URL/hyprsync.bash" -o "$BASH_COMP_DIR/hyprsync"

        if ! grep -q "hyprsync" "$HOME/.bashrc" 2>/dev/null; then
            echo "" >> "$HOME/.bashrc"
            echo "# hyprsync completions" >> "$HOME/.bashrc"
            echo "[ -f \"$BASH_COMP_DIR/hyprsync\" ] && source \"$BASH_COMP_DIR/hyprsync\"" >> "$HOME/.bashrc"
        fi
    fi

    if [ -n "$ZSH_VERSION" ] || [ -f "$HOME/.zshrc" ]; then
        ZSH_COMP_DIR="$HOME/.local/share/zsh/site-functions"
        mkdir -p "$ZSH_COMP_DIR"
        curl -sL "$COMPLETIONS_URL/hyprsync.zsh" -o "$ZSH_COMP_DIR/_hyprsync"

        if ! grep -q "hyprsync" "$HOME/.zshrc" 2>/dev/null; then
            echo "" >> "$HOME/.zshrc"
            echo "# hyprsync completions" >> "$HOME/.zshrc"
            echo "fpath=($ZSH_COMP_DIR \$fpath)" >> "$HOME/.zshrc"
            echo "autoload -Uz compinit && compinit" >> "$HOME/.zshrc"
        fi
    fi
}

install_completions

if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo ""
    echo "note: $INSTALL_DIR is not in your PATH"
    echo "add this to your shell config:"
    echo ""
    echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    echo ""
fi

echo ""
echo "hyprsync $LATEST installed successfully"
echo ""
echo "restart your shell or run 'source ~/.bashrc' (or ~/.zshrc) for completions"
echo ""
echo "next steps:"
echo "  hyprsync init      # interactive setup"
echo "  hyprsync --help    # show all commands"
