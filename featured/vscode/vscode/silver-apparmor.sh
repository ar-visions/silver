#!/bin/sh
# names our code-oss in AppArmor where Ubuntu limits user
# namespaces; asks for root (pkexec) only when the profile differs
install="$1"
limit=/proc/sys/kernel/apparmor_restrict_unprivileged_userns
[ "$(cat "$limit" 2>/dev/null)" = 1 ] || exit 0
code=$(readlink -f "$install/lib/code-oss/code-oss")
want="$install/tmp/silver-code-oss"
sed "s#@CODE@#$code#" silver-code-oss.apparmor > "$want"
cmp -s "$want" /etc/apparmor.d/silver-code-oss && exit 0
pkexec sh -c "cp '$want' /etc/apparmor.d/silver-code-oss && \
    apparmor_parser -r /etc/apparmor.d/silver-code-oss"
