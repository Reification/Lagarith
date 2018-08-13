#!/bin/bash
function pull_stdin() {
  while read imx; do
    adb pull "/${imx}"
  done
}
 (adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/mismatched_*.png";
  adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/decompressed_*.png"; 
  adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/*.avi") \
| dos2unix | pull_stdin
