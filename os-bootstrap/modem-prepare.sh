#!/bin/sh
# the phone's modem and clock: run by init before orbiter (the SIM
# and the saved time), then with --enable for the phone's life: every
# 15 s it puts right what is not (a SIM late at boot, a dropped
# session), so service comes up whatever order things start in

device=qrtr://0
qmicli=${QMICLI:-/usr/bin/qmicli}
mmcli=${MMCLI:-/usr/bin/mmcli}
saved=/var/lib/orbiter-clock
log=/var/log/modem-prepare.log

say() {
    echo "$(cut -d' ' -f1 /proc/uptime) $*" >> "$log"
}

online() {
    "$qmicli" -d "$device" --dms-set-operating-mode=online >/dev/null
}

# the SIM as the modem's primary session; 0 once it is (and online)
sim() {
    status=$("$qmicli" -d "$device" --uim-get-card-status 2>/dev/null)
    case "$status" in
        *"Primary GW:   slot "*) return 0 ;;
    esac
    aid=$(printf '%s\n' "$status" | /usr/bin/awk '
        /Application type:.*usim/ { usim = 1; next }
        usim && /Application ID:/ {
            getline
            gsub(/[^0-9A-Fa-f]/, "")
            print
            exit
        }')
    [ -n "$aid" ] || return 1
    request="session-type=primary-gw-provisioning,activate=yes"
    request="$request,slot=1,aid=$aid"
    if "$qmicli" -d "$device" \
        --uim-change-provisioning-session="$request" >/dev/null 2>&1
    then
        online
        say "sim: session set up, online"
        return 0
    fi
    say "sim: the session request failed"
    return 1
}

# ModemManager gives up on a SIM it found missing; once the SIM's
# session is up it starts again, at most every 5 minutes
restarted=0
restart() {
    now=$(cut -d. -f1 /proc/uptime)
    [ $((now - restarted)) -ge 300 ] || return
    sim || return
    restarted=$now
    /usr/bin/pkill -x ModemManager
    sleep 2
    /usr/sbin/ModemManager --debug >> /var/log/ModemManager.log 2>&1 &
    say "modem: ModemManager had failed, started again"
}

# ModemManager's modem enabled; 0 once it is
enabled() {
    list=$("$mmcli" -L 2>/dev/null)
    modem=${list#*Modem/}
    [ "$modem" != "$list" ] || return 1
    modem=${modem%% *}
    case "$modem" in
        ''|*[!0-9]*) return 1 ;;
    esac
    st=$("$mmcli" -m "$modem" 2>/dev/null | /usr/bin/awk -F': ' '
        / state:/ { gsub(/\033\[[0-9;]*m/, "", $2); print $2; exit }')
    case "$st" in
        registered|connected|connecting|enabled|searching) return 0 ;;
        failed) restart; return 1 ;;
    esac
    if "$mmcli" -m "$modem" --enable >/dev/null 2>&1; then
        say "modem: enabled (was $st)"
        return 0
    fi
    return 1
}

# the network's time: the modem's system time is utc in ms since
# 1980-01-06; only a time past 2026 is the network's
clock() {
    ms=$("$qmicli" -d "$device" --dms-get-time 2>/dev/null |
        /usr/bin/awk -F"'" '/System time:/ {
            split($2, a, " "); print a[1]; exit }')
    s=$(( ${ms:-0} / 1000 + 315964800 ))
    [ "$s" -gt 1767225600 ] || return 1
    /usr/bin/date -u -s "@$s" >/dev/null
    say "clock: the network's time"
    return 0
}

if [ "$1" = "--enable" ]; then
    timed=0
    while :; do
        sim
        enabled
        [ "$timed" = 1 ] || { clock && timed=1; }
        /usr/bin/date -u +%s > "$saved.new" && mv "$saved.new" "$saved"
        sleep 15
    done
fi

if [ "$1" = "--clock" ]; then
    clock
    exit $?
fi

# the boot: the saved time first (never 1970 under orbiter), then the
# SIM for 30 s at most (the --enable run goes on trying after)
say "boot"
now=$(/usr/bin/date -u +%s)
last=$(cat "$saved" 2>/dev/null)
if [ "${last:-0}" -gt "$now" ]; then
    /usr/bin/date -u -s "@$last" >/dev/null
    say "clock: the saved time"
fi
n=0
while [ "$n" -lt 30 ]; do
    sim && { online; exit 0; }
    n=$((n + 1))
    sleep 1
done
say "sim: not ready at boot; the --enable run keeps trying"
exit 0
