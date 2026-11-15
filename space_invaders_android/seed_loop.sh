#!/bin/bash
# Gradle --offline を回し、足りない部品を curl で補充するループ。
# 収束（ビルド成功）か、補充が進まなくなったら止まる。
cd ~/workspace/space_invaders_android/project || exit 1
export JAVA_HOME=$HOME/workspace/android-tools/jdk
export ANDROID_HOME=$HOME/workspace/android-tools/sdk
export GRADLE_USER_HOME=$HOME/workspace/android-tools/gradle-home
export GRADLE_OPTS="-Xmx2g"
LOGDIR=~/workspace/space_invaders_android
for i in $(seq 1 30); do
  log="$LOGDIR/loop$i.log"
  timeout 900 ~/workspace/android-tools/gradle/bin/gradle --offline assembleDebug --console=plain > "$log" 2>&1
  rc=$?
  echo "round $i: gradle exit=$rc"
  if [ $rc -eq 0 ]; then
    echo "BUILD OK"
    break
  fi
  out=$(python3 ~/workspace/space_invaders_android/fetch_missing.py "$log" | tail -1)
  echo "round $i: $out"
  if [ "$out" = "total fetched: 0 / wanted 0" ]; then
    echo "STUCK: gradle failed but nothing parseable was missing"
    break
  fi
  case "$out" in
    "total fetched: 0 /"*)
      echo "STUCK: wanted files could not be fetched"
      grep -oE "(Could not find|No cached version of) [^ ]+" "$log" | sort -u | head -20
      break
      ;;
  esac
done
