#!/usr/bin/env bash
# 构建 收集车控制台 V3.2（无 Gradle，直接调用 Android SDK 工具）
#
# 用法：
#   SDK=/path/to/android-sdk ./build.sh
# 可选：
#   KEYSTORE=/path/to/your.keystore KS_PASS=****** KEY_PASS=****** ./build.sh
#
# 依赖：JDK 17+、Android SDK platform 34、build-tools 35.0.0+
set -euo pipefail

SDK="${SDK:?请设置 SDK=/path/to/android-sdk}"
BT="$SDK/build-tools/35.0.0"
JAR="$SDK/platforms/android-34/android.jar"

[ -x "$BT/d8" ] || { echo "找不到 $BT/d8（需要 build-tools 35.0.0）"; exit 1; }
[ -f "$JAR" ] || { echo "找不到 $JAR（需要 platform 34）"; exit 1; }

cd "$(dirname "$0")"
rm -rf build
mkdir -p build/obj build/dex

echo "[1/5] javac"
javac -source 8 -target 8 -bootclasspath "$JAR" -d build/obj $(find src -name "*.java")

echo "[2/5] d8"
"$BT/d8" --release --min-api 23 --lib "$JAR" --output build/dex $(find build/obj -name "*.class")

echo "[3/5] aapt2"
"$BT/aapt2" compile --dir res -o build/res.zip
"$BT/aapt2" link -o build/app-unsigned.apk -I "$JAR" --manifest AndroidManifest.xml \
    -R build/res.zip --min-sdk-version 23 --target-sdk-version 34 \
    --version-code 5 --version-name 3.2

echo "[4/5] package"
cp build/app-unsigned.apk build/app.apk
( cd build && zip -j -q app.apk dex/classes.dex )
"$BT/zipalign" -f -p 4 build/app.apk build/app-aligned.apk

echo "[5/5] sign"
if [ -n "${KEYSTORE:-}" ]; then
    "$BT/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:${KS_PASS}" \
        --key-pass "pass:${KEY_PASS}" --out build/CarControl-v3.2.apk build/app-aligned.apk
    echo "已签名：build/CarControl-v3.2.apk"
else
    cp build/app-aligned.apk build/CarControl-v3.2-unsigned.apk
    echo "未提供 KEYSTORE，输出未签名包：build/CarControl-v3.2-unsigned.apk"
fi
