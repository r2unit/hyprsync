# hyprsync

A tiny sync daemon that keeps your dotfiles and configs in sync across your Linux machines.

It uses SSH keys and rsync under the hood, works over LAN or Tailscale, and runs as a peer on every machine. No server, no client, just peers talking to each other.

## Install

```bash
# yolo
curl -fsSL https://raw.githubusercontent.com/r2unit/hyprsync/main/install.sh | bash
```

```bash
# aur (arch linux)
yay -S hyprsync
```
