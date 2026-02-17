#compdef hyprsync

_hyprsync() {
    local -a commands
    local -a options

    commands=(
        'init:interactive setup wizard'
        'daemon:start the sync daemon'
        'sync:run a one-shot sync'
        'restore:restore files from repo'
        'status:show sync status'
        'diff:show pending changes'
        'log:show sync history'
        'ping:test device connectivity'
        'conflicts:list and resolve conflicts'
        'upgrade:upgrade to latest version'
        'version:show version info'
        'help:show help'
    )

    options=(
        '-c[config file path]:config file:_files'
        '--config[config file path]:config file:_files'
        '-n[dry-run mode]'
        '--dry-run[dry-run mode]'
        '-v[verbose output]'
        '--verbose[verbose output]'
        '-q[quiet mode]'
        '--quiet[quiet mode]'
        '-g[sync specific group]:group:'
        '--group[sync specific group]:group:'
        '-d[sync to specific device]:device:'
        '--device[sync to specific device]:device:'
        '-h[show help]'
        '--help[show help]'
    )

    _arguments -C \
        '1:command:->command' \
        '*:subcommand:->subcommand' \
        $options

    case "$state" in
        command)
            _describe -t commands 'hyprsync commands' commands
            ;;
        subcommand)
            case "${words[2]}" in
                upgrade)
                    local -a upgrade_cmds
                    upgrade_cmds=(
                        'list:list available versions'
                        'check:check for updates'
                    )
                    _describe -t commands 'upgrade commands' upgrade_cmds
                    ;;
                conflicts)
                    local -a conflict_cmds
                    conflict_cmds=(
                        'resolve:resolve conflicts interactively'
                    )
                    _describe -t commands 'conflict commands' conflict_cmds
                    ;;
                resolve)
                    _arguments '--auto[use configured strategy]'
                    ;;
            esac
            ;;
    esac
}

_hyprsync "$@"
