#!/bin/bash
function pull_stdin() {
  while read imx; do
    adb pull "/${imx}"
  done
}
 (adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/*_frame_*.png";
  adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/decompressed_*.png"; 
  adb shell "ls /sdcard/Android/data/com.lagarith.test_runner/test_data/*.lags") \
| dos2unix | pull_stdin
