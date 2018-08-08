#!/bin/bash
#D, V, I, W, E are options for verbosity
_LOG_FILTERS=-s" Lagarith.test_runner:V UnitTestRunnerActivity:V Lagarith.NAG:V Lagarith.DbgPrint:V opencl:D"
_LINE_MODE=false
KC_ENTER=66
KC_PAGEUP=92
KC_PAGEDOWN=93
_LOG_SERIAL=0
_SCREENSHOT_SERIAL=0
_PKGID=com.lagarith.test_runner
_LAUNCHER_ACTIVITY=com.lagarith.UnitTestRunnerActivity

_proj_root=$(dirname "${0}")
#_proj_root=$(cd "${_proj_root}"; cd ..; pwd)
#hack! find a way to fix this...
_build_root="${_proj_root}/build/android"
_bin_dir="${_build_root}/bin"

if [[ ! -x "$(which adb 2> /dev/null)" ]]; then
  export PATH=${PATH}:$(cygpath "${ANDROID_HOME}")/platform-tools
  if [[ ! -x "$(which adb 2> /dev/null)" ]]; then
    echo "adb not in path and ANDROID_HOME not set to valid android sdk directory." 1>&2
    echo "exiting." 1>&2
    exit 1
  fi
fi

function initDataDst() {
  if [[ ! -n "${_data_dst}" ]]; then
    _data_dst="$(adb shell 'echo -n ${EXTERNAL_STORAGE}' 2> /dev/null)"
    if [[ -n "${_data_dst}" ]]; then
      _data_dst="/${_data_dst}/Android/data/${_PKGID}"
    else
      1>&2 echo "Device not connected."
      return 1
    fi
  fi
  return 0
}

function clearLog() {
  adb logcat -c
}

function as_chars() {
  if [[ "${1}" == "-h" ]]; then
    echo "usage: as_chars - send text characters"
    return 0
  fi
  
  if [[ "${1}" ]]; then
    adb shell input text "${@}"
  fi
}

function as_key() {
  if [[ "${1}" == "-h" ]]; then
    echo "usage: as_key - send key code"
    return 0
  fi
  
  local _kc=${1}
  
  if [[ "${_kc:0:1}" == '$' ]]; then
    _kc=$(eval "echo ${_kc}")
  fi

  adb shell input keyevent "${_kc}"
}

function as_line() {
  if [[ "${1}" == "-h" ]]; then
    echo "usage: as_line - send text characters followed by newline key code"
    return 0
  fi
  
  as_chars "${@}"
  as_key ${KC_ENTER}
}

function as_linemode() {
  case "${1}" in
  "-h") echo "usage: as_linemode [true|false]"
        echo "  if no arg specified, reports line mode value.";;
  "") echo "linemode is: ${_LINE_MODE}";;
  f*|F*|0|off|Off|OFF) _LINE_MODE=false;;
  *) _LINE_MODE=true;;
  esac
}

function as_newlog() {
  _log_filters=${_LOG_FILTERS}
  _start=start
  
  while [[ "${1}" ]]; do
    case "${1}" in
    -h*) echo "usage: as_newlog [-c]lear [-a]ll"; return 0;;
    "-c") clearLog ;;
    "-a") _log_filters= ;;
    "-no_win") _start=;;
    esac
    shift
  done
  
  ${_start} adb logcat ${_log_filters}
}

function as_reboot() {
  while [[ "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_reboot - reboot android device"; return 0;;
    esac
    shift
  done

  adb reboot
}

function as_savelog() {
  local _log_filters=${_LOG_FILTERS}
  local _log_file=
  
  while [[ "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_savelog [-a]ll [outfile]"; return 0;;
    "-a"|"-all") _log_filters= ;;
    *) _log_file="${1}" ;;
    esac
    shift
  done    

  if [[ "${_log_file}" == "" ]]; then
    _log_file="${TMP}/asole_log_$$_$((_LOG_SERIAL++)).txt"
  fi
  
  adb logcat -d ${_log_filters} > "${_log_file}"
  start "${_log_file}"
}

as_pushdata() {
  if ! initDataDst; then
    return 1
  fi

  if [[ ! -d "${_bin_dir}" ]]; then
    echo "${_bin_dir} does not exist."
    return 1
  fi

  while [[ -n "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_pushdata - push test data to device"; return 0;;
    *) _argv="${_argv}${1} ";;
    esac
    shift
  done

  echo -n "Pushing test data..."
  if 2>&1 adb push "${_bin_dir}/." "${_data_dst}/" > /dev/null; then
    echo "done."
  else
    1>&2 echo "failed!"
  fi
}

function as_launch() {
  if ! initDataDst; then
    return 1
  fi

  local _argv=
  local _argv_path="${_data_dst}/argv.txt"

  while [[ -n "${1}" ]]; do
    case "${1}" in
    "-h"|"-help") echo "usage: as_launch [arg1, arg2...] | [--] - launches the test runner";
    echo " when arguments provided they will stick on repeated launches until changed or cleared."
    echo "  -- : clear previous argv list"
     return 0;;
    "--") _argv="--";;
    *) _argv="${_argv}${1} ";;
    esac
    shift
  done

  #if [[ "${_argv}" == "--" ]]; then
  #  adb shell "rm -f \"${_argv_path}\"" 2>&1 > /dev/null
  #elif [[ -n "${_argv}" ]]; then
  #  (echo -n "${_argv}" > argv.txt; adb push argv.txt "${_argv_path}"; rm argv.txt) 2>&1  > /dev/null
  #fi

  #_argv=$(adb shell cat ${_argv_path} 2> /dev/null)
  #_argv=$(gawk -v lines="${_argv}" 'BEGIN { gsub(/[ \t\r\n]+/, " ", lines); print lines; exit(0); }')
  echo "launching ${_PKGID}/${_LAUNCHER_ACTIVITY}"

  adb shell am start "${_PKGID}/${_LAUNCHER_ACTIVITY}"
}

function catTestLog() {
  echo "#OutLog:"
  adb shell cat "${_data_dst}/test_out_log.txt"
  echo ""
  echo "#ErrLog:"
  adb shell cat "${_data_dst}/test_err_log.txt"
}

function as_pulltestlog() {
  local _log_file=

  if ! initDataDst; then
    return 1
  fi

  while [[ "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_pulltestlog - pull test log from device."
          return 0;;
    *) _log_file="${1}";;
    esac
    shift
  done

  if [[ ! -n "${_log_file}" ]]; then
    _log_file="${TMP}/testlog_$$_$((_LOG_SERIAL++)).txt"
  fi

  catTestLog > "${_log_file}"
  start "${_log_file}"
}

function as_screenshot() {
  local _screenshot_file=
  
  while [[ "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_screenshot [outfile.png]"; return 0;;
    *) _screenshot_file="${1}" ;;
    esac
    shift
  done    

  if [[ "${_screenshot_file}" == "" ]]; then
    _screenshot_file="${HOME}/screenshot_$$_$((_SCREENSHOT_SERIAL++)).png"
  fi
  
  adb shell screencap -p | sed 's/\r$//' > "${_screenshot_file}"
  echo "Saved screenshot to ${_screenshot_file}"  
}

function as_kill() {
  while [[ "${1}" ]]; do
    case "${1}" in
    "-h") echo "usage: as_kill - stops the running game"; return 0;;
    esac
    shift
  done
  
  adb shell pm clear ${_PKGID}
}

function as_shell() {
  if [[ "${1}" == "-h" ]]; then
    echo "usage: as_shell [args] - run shell or shell command on device."
    return 0;
  fi

  adb shell "${@}"
}

function as_help() {
  if [[ "${1}" ]]; then
    case ${1} in
    as_clear|as_help|as_quit|as_exit) ;;
    as_*) "${1}" -h; return 0;;
    esac
  fi
  
  echo "A)ndroid Con(sole"
  echo ""
  echo "asole commands:"
  echo "  as_chars <string> - send char codes to device without terminal 'enter'"
  echo "  as_clear - clear screen"
  echo "  as_exit - quit asole"
  echo "  as_help [as_command] - print this message or help about a command"
  echo "  as_key <key_code> - send single key code, e.g. 66 (enter)"
  echo "  as_line [string] - send char codes to device followed by terminal 'enter'"
  echo "  as_linemode [true|false] - set linemode (auto-send 'enter') on or off."
  echo "  as_newlog - open new log window"
  echo "  as_reboot - reboot device"
  echo "  as_savelog - save log to file."
  echo "  as_screenshot - save screenshot to file."
  echo "  as_pushdata - push test data to device."
  echo "  as_launch [test args] | -- - launch app."
  echo "  as_kill - kill app process."
  echo "  as_pulltestlog - pull test log from device."
  echo "  as_shell [args] - run shell or shell command on device."
  echo ""
  echo "empty command lines are ignored."
  echo "all other lines are sent as keystrokes to android device."
  echo "each line sent is followed by the 'enter' keycode (when linemode is true)."
      
  echo ""
  echo "use %s for literal space."
  echo "e.g. \"hello,%sworld.\""
  echo ""
}

if [[ "${1}" == "-log-only" ]]; then
  shift
  as_newlog -no_win "${@}"
  exit 0
fi

#open separate console window with log output.
if [[ -x "$(which start)" && "${1}" == "-log" ]]; then
  shift
  as_newlog "${@}"
fi

function processCmd() {
  case ${1} in
  as_clear)
    clear;;
  as_exit|as_quit)
    exit 0;;
  as_*)
    "${@}";;
  *) return 0;;
  esac
  
  return 1
}

as_help

p="asole> "

while read -e -p "${p}" t; do
  if [[ "${t}" ]]; then
    #returns true if not an asole command (needs to go to device)
    #returns false if is an asole command
    if processCmd ${t}; then
      if ${_LINE_MODE}; then
        as_line "${t}"
      else
        as_chars "${t}"
      fi
    fi
    history -s "${t}"
  fi
done
