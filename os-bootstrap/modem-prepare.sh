#!/bin/sh

device=qrtr://0
qmicli=${QMICLI:-/usr/bin/qmicli}
mmcli=${MMCLI:-/usr/bin/mmcli}
attempt=0
status=
aid=

online() {
    "$qmicli" -d "$device" --dms-set-operating-mode=online
}

enable() {
    while [ "$attempt" -lt 60 ]; do
        list=$("$mmcli" -L 2>/dev/null)
        modem=${list#*Modem/}
        if [ "$modem" != "$list" ]; then
            modem=${modem%% *}
            case "$modem" in
                ''|*[!0-9]*) ;;
                *) "$mmcli" -m "$modem" --enable && return 0 ;;
            esac
        fi
        attempt=$((attempt + 1))
        sleep 1
    done
    return 1
}

if [ "$1" = "--enable" ]; then
    enable
    exit $?
fi

while [ "$attempt" -lt 30 ]; do
    status=$("$qmicli" -d "$device" \
        --uim-get-card-status 2>/dev/null)
    case "$status" in
        *"Primary GW:   slot "*) online; exit $? ;;
    esac
    aid=$(printf '%s\n' "$status" | /usr/bin/awk '
        /Application type:.*usim/ { usim = 1; next }
        usim && /Application ID:/ {
            getline
            gsub(/[^0-9A-Fa-f]/, "")
            print
            exit
        }')
    [ -n "$aid" ] && break
    attempt=$((attempt + 1))
    sleep 1
done

[ -n "$aid" ] || exit 1
request="session-type=primary-gw-provisioning,activate=yes"
request="$request,slot=1,aid=$aid"
"$qmicli" -d "$device" \
    --uim-change-provisioning-session="$request" || exit 1
online
