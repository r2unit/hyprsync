_hyprsync() {
    local cur prev commands opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    commands="init daemon sync restore status diff log ping conflicts upgrade version help"
    opts="-c --config -n --dry-run -v --verbose -q --quiet -g --group -d --device -h --help"

    case "$prev" in
        hyprsync)
            COMPREPLY=($(compgen -W "$commands" -- "$cur"))
            return 0
            ;;
        -c|--config)
            COMPREPLY=($(compgen -f -- "$cur"))
            return 0
            ;;
        -g|--group|-d|--device)
            return 0
            ;;
        upgrade)
            COMPREPLY=($(compgen -W "list check" -- "$cur"))
            return 0
            ;;
        conflicts)
            COMPREPLY=($(compgen -W "resolve" -- "$cur"))
            return 0
            ;;
        resolve)
            COMPREPLY=($(compgen -W "--auto" -- "$cur"))
            return 0
            ;;
        *)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=($(compgen -W "$opts" -- "$cur"))
            else
                COMPREPLY=($(compgen -W "$commands" -- "$cur"))
            fi
            return 0
            ;;
    esac
}

complete -F _hyprsync hyprsync
