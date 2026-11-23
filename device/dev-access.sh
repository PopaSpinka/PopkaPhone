#!/bin/sh
# НаомиФон — доступ разработчика на телефоне. Запускается один раз от root:
#   sudo sh /tmp/naomi-dev-access.sh
# Делает: sudo без пароля для phablet, SSH только по ключу и только для phablet, SSH включён
# при загрузке. Корень Ubuntu Touch только для чтения — на время правки открывается на запись.
# Ключ Mac в ~/.ssh/authorized_keys кладёт `tools/naomi access` (без root).
set -eu

[ "$(id -u)" = 0 ] || { echo "нужен root: sudo sh $0" >&2; exit 1; }

mount -o remount,rw /
trap 'sync; mount -o remount,ro / || echo "внимание: корень остался на запись" >&2' EXIT

printf 'phablet ALL=(ALL) NOPASSWD: ALL\n' > /etc/sudoers.d/90-naomi-dev
chmod 0440 /etc/sudoers.d/90-naomi-dev
visudo -cf /etc/sudoers.d/90-naomi-dev >/dev/null

cat > /etc/ssh/sshd_config.d/90-naomi-dev.conf <<'EOF'
# НаомиФон: вход только по ключу и только phablet
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin no
AllowUsers phablet
EOF
sshd -t

systemctl enable ssh >/dev/null 2>&1
systemctl restart ssh
echo "готово: sudo без пароля, SSH по ключу — $(systemctl is-active ssh)"
