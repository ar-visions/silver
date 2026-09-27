#!/bin/bash
# video test through the browser app: headless, driven over its socket
# usage: support/browser-video.sh [url] [seconds]   (exit 0 pass, 1 fail)
# reports the page frames the browser element shows each second
R=/src/silver
URL=${1:-https://www.youtube.com/watch?v=aqz-KE-bpKQ}
SECS=${2:-40}
RT=$(mktemp -d /tmp/bv.XXXX)
SOCK=$RT/trinity-browser.sock
LOG=$RT/app.log
s() { printf '%s\n' "$1" | nc -U -w 2 $SOCK 2>/dev/null; }
fail() { echo "FAIL: $1 (log $LOG)"; exit 1; }

cd $R
# its own process group: the app, its engine and their children end together
setsid sh -c 'echo $$ > "$0/pid"; exec env XDG_RUNTIME_DIR="$0" SILVER_ISOLATE=0 SILVER_HEADLESS=1 \
    platform/native/build/browser --hidden true --stats true "$1"' $RT "$URL" > $LOG 2>&1 &
for i in {1..20}; do [ -s $RT/pid ] && break; sleep 0.1; done
PID=$(cat $RT/pid)
# every process of this run carries RT in its environment, whatever
# group it ends up in; however the script ends, they all go
sweep() {
    kill -9 -- -$PID 2>/dev/null
    for e in /proc/[0-9]*/environ; do
        grep -qz "^XDG_RUNTIME_DIR=$RT\$" $e 2>/dev/null && kill -9 $(basename $(dirname $e)) 2>/dev/null
    done
}
trap sweep EXIT INT TERM
for i in {1..60}; do [ -S $SOCK ] && [ -n "$(s stats)" ] && break; sleep 1; done
[ -S $SOCK ] || fail "app socket never opened"

# the page loads, then a click in the player starts the video
sleep ${LOAD:-12}
# the pointer arrives, then presses: the player's play button
X=${CLICK_X:-455}; Y=${CLICK_Y:-316}
s "move $(( X - 40 )) $(( Y - 40 ))" > /dev/null
s "move $X $Y" > /dev/null
sleep 0.3
s "press $X $Y" > /dev/null
sleep 0.1
s "release $X $Y" > /dev/null
start=$(grep -ac "browser frames" $LOG)
sleep $SECS
s "shot $RT/end.png" > /dev/null
sleep 1
sweep

grep -aE "signal [0-9]|DOUBLE-DROP|DEAD-USE|fence wait failed|the engine exited" $LOG | head -3 | grep . \
    && fail "fault in the app log"
per=$(grep -a "browser frames" $LOG | tail -n +$(( start + 1 )) | awk '{print $NF}')
[ -n "$per" ] || fail "no frame counts after the click"
echo "$per" | awk -v secs=$SECS '
    { n[NR] = $1; sum += $1 }
    END {
        asort(n)
        printf "page frames shown each second over %d s: mean %.1f, median %d, lowest %d, highest %d\n",
            NR, sum / NR, n[int((NR + 1) / 2)], n[1], n[NR]
    }'
grep -a "^.*fps " $LOG | tail -n +$(( start + 1 )) | awk '{ for (i = 1; i < NF; i++) if ($i == "fps") { s += $(i + 1); c++ } }
    END { if (c) printf "window frames each second: mean %.1f\n", s / c }'
echo "last picture: $RT/end.png"
