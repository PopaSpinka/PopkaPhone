#!/bin/bash
# НаомиФон для планшета — сборка APK без Gradle и без скачиваний (docs/TABLET.md): javac → d8 → aapt2 → zipalign →
# apksigner из Android SDK, что уже стоит на Mac (brew: android-commandlinetools — build-tools 35.0.0, платформа
# android-34; JDK 17 — openjdk@17). Ключ подписи — private/tablet (делается при первой сборке, в git не идёт).
#
#   tablet/build.sh        → tablet/build/NaomiTablet.apk
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
SDK="${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}"
BT="$SDK/build-tools/35.0.0"
JAR="$SDK/platforms/android-34/android.jar"
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@17}"
export PATH="$JAVA_HOME/bin:$PATH"
OUT="$HERE/build"
KEYS="$ROOT/private/tablet"

# ключ подписи: один раз, пароль — случайный, рядом (обновление приложения — только тем же ключом)
if [ ! -s "$KEYS/release.jks" ]; then
    mkdir -p "$KEYS"
    chmod 700 "$KEYS"
    (umask 077; openssl rand -hex 24 > "$KEYS/keystore.pass")
    keytool -genkeypair -keystore "$KEYS/release.jks" -storetype PKCS12 -alias naomi -keyalg RSA -keysize 3072 \
        -validity 36500 -dname "CN=NaomiPhone Tablet" -storepass:file "$KEYS/keystore.pass" >/dev/null 2>&1
    chmod 600 "$KEYS/release.jks"
fi

rm -rf "$OUT"
mkdir -p "$OUT/classes" "$OUT/dex"
CODE=$(git -C "$ROOT" rev-list --count HEAD)
NAME="$(cat "$ROOT/VERSION" 2>/dev/null || echo 0.1)-$(git -C "$ROOT" rev-parse --short HEAD)"

# лямбды: LambdaMetafactory в android.jar нет — заглушки из build-tools (их же берёт Gradle), d8 разворачивает лямбды сам
javac -source 8 -target 8 -bootclasspath "$JAR" -classpath "$BT/core-lambda-stubs.jar" -encoding UTF-8 -Xlint:-options \
    -d "$OUT/classes" \
    $(find "$HERE/src" -name '*.java')
"$BT/d8" --release --min-api 26 --lib "$JAR" --output "$OUT/dex" $(find "$OUT/classes" -name '*.class')
"$BT/aapt2" compile --dir "$HERE/res" -o "$OUT/res.zip"
# шрифты часов заставки — те же файлы, что у телефона и приставки (downloads/fonts, в git не идут); нет их — системный
mkdir -p "$OUT/assets/fonts"
cp "$ROOT/downloads/fonts/GoogleSansFlex[ROND,opsz,slnt,wdth,wght].ttf" "$OUT/assets/fonts/GoogleSansFlex.ttf" 2>/dev/null || true
cp "$ROOT/downloads/fonts/GoogleSans[GRAD,opsz,wght].ttf" "$OUT/assets/fonts/GoogleSans.ttf" 2>/dev/null || true
"$BT/aapt2" link -o "$OUT/base.apk" -I "$JAR" --manifest "$HERE/AndroidManifest.xml" --min-sdk-version 26 \
    --target-sdk-version 34 --version-code "$CODE" --version-name "$NAME" --debug-mode -A "$OUT/assets" "$OUT/res.zip"
(cd "$OUT/dex" && zip -q -j "$OUT/base.apk" classes.dex)
"$BT/zipalign" -P 16 -f 4 "$OUT/base.apk" "$OUT/aligned.apk"
"$BT/apksigner" sign --ks "$KEYS/release.jks" --ks-pass "file:$KEYS/keystore.pass" --out "$OUT/NaomiTablet.apk" \
    "$OUT/aligned.apk"
echo "собрано: tablet/build/NaomiTablet.apk ($NAME, $(du -h "$OUT/NaomiTablet.apk" | cut -f1 | tr -d ' '))"
