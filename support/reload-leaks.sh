#!/bin/zsh
# live reload leak test: headless orbiter-dbg, N reloads, zero growth
# usage: support/reload-leaks.sh [reloads]   (exit 0 pass, 1 fail)
R=/Users/kalen/src/silver
N=${1:-4}
RT=$(mktemp -d /tmp/rl.XXXX)
SOCK=$RT/trinity-orbiter.sock
LOG=$RT/app.log
s() { printf '%s\n' "$1" | nc -U -w 2 $SOCK 2>/dev/null; }
stat_of() { local t; for k in {1..15}; do t=$(s stats); [ -n "$t" ] && break; sleep 3; done; echo "$t"; }
num() { echo "$1" | sed -n "s/.* $2 \([0-9-]*\).*/\1/p"; }
closes() { grep -ac "old instance closed" $LOG; }
fail() { echo "FAIL: $1"; pkill -9 -x ${BIN:-orbiter-dbg}; exit 1; }

cd $R
if [ -n "$LLDB" ]; then
    (XDG_RUNTIME_DIR=$RT SILVER_ISOLATE=0 SILVER_HEADLESS=1 lldb --batch \
        -o "process handle SIGSEGV -s true -p false" -o "process handle SIGBUS -s true -p false" \
        -o run -k "thread backtrace all -c 25" -k kill \
        -- platform/native/build/${BIN:-orbiter-dbg} --hidden true > $LOG 2>&1 &)
else
    XDG_RUNTIME_DIR=$RT SILVER_ISOLATE=0 SILVER_HEADLESS=1 \
        platform/native/build/${BIN:-orbiter-dbg} --hidden true ${LEAKS:+--leaks} ${LEAKS} > $LOG 2>&1 &
fi
for i in {1..240}; do [ -S $SOCK ] && [ -n "$(s stats)" ] && break; sleep 1; done
[ -S $SOCK ] || fail "app socket never opened"
sleep 8

typeset -a SAMP OBJ
for i in $(seq 0 $N); do
    if [ $i -gt 0 ]; then
        touch $R/orbiter/Editor.ag
        for k in {1..30}; do [ $(grep -ac "reload staged" $LOG) -ge $i ] && break; sleep 1; done
        for t in {1..10}; do
            B=$(s "bounds reload_ready"); set -- $(echo $B | tr -c '0-9.\n' ' ')
            if [ -n "$1" ]; then
                s "click $(( $1 + $3 / 2 )) $(( $2 + $4 / 2 ))" >/dev/null
                for k in {1..5}; do [ $(grep -ac "apply requested" $LOG) -ge $i ] && break 2; sleep 1; done
            else
                sleep 1
            fi
        done
        [ $(grep -ac "apply requested" $LOG) -ge $i ] || fail "reload $i: apply never taken"
        for k in {1..400}; do [ $(closes) -ge $i ] && break; sleep 1; done
        [ $(closes) -ge $i ] || fail "reload $i: old instance never closed"
        sleep 6
    fi
    st=$(stat_of)
    [ -n "$CENSUS" ] && s "types 1" >/dev/null
    if [ -n "$LEAKS" ]; then
        c9=$(grep -ac "^--leaks:" $LOG); s leaks >/dev/null
        for k in {1..240}; do [ $(grep -ac "^--leaks:" $LOG) -gt $c9 ] && break; sleep 1; done
    fi
    if [ -z "$st" ]; then
        sample $(pgrep -x orbiter-dbg | head -1) 2 -file $RT/hang.txt >/dev/null 2>&1
        fail "reload $i: app not answering (stacks in $RT/hang.txt)"
    fi
    SAMP+=( $(num "$st" samplers) ); OBJ+=( $(num "$st" objects) )
    echo "reload $i: samplers ${SAMP[-1]} objects ${OBJ[-1]}"
    grep -aE "stop reason = signal SIG(SEGV|BUS)|signal [0-9]|DOUBLE-DROP|DOUBLE-FREE|DEAD-USE|pipeline creation fail|mvk-error" $LOG \
        | grep -v "syntax: " | head -3 | grep . && fail "fault in the app log ($LOG)"
done
pkill -9 -x ${BIN:-orbiter-dbg}

# reload 1 is the baseline: startup caches settle on the first swap
b=2; e=$(( N + 1 ))
[ ${SAMP[$e]} -le ${SAMP[$b]} ] || fail "samplers grew ${SAMP[$b]} -> ${SAMP[$e]}"
lim=$(( OBJ[$b] + OBJ[$b] / 200 ))
[ ${OBJ[$e]} -le $lim ] || fail "objects grew ${OBJ[$b]} -> ${OBJ[$e]} (limit $lim)"
echo "PASS: $N reloads, samplers ${SAMP[$b]} -> ${SAMP[$e]}, objects ${OBJ[$b]} -> ${OBJ[$e]}"
[ -n "$CENSUS" ] || rm -rf $RT
