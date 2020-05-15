#
# Different utilities and constants.
#

PROGNAME="${BASH_SOURCE[${#BASH_SOURCE[@]}-1]##*/}"
PROGARGS="$@"
PROGPATH="$(readlink -f "$(dirname "$0")")"
readonly PROGNAME PROGARGS PROGPATH

enable_colors() {
  C_red='\e[0;31m';    C_bred='\e[1;31m'
  C_green='\e[0;32m';  C_bgreen='\e[1;32m'
  C_yellow='\e[0;33m'; C_byellow='\e[1;33m'
  C_blue='\e[0;34m';   C_bblue='\e[1;34m'
  C_purple='\e[0;35m'; C_bpurple='\e[1;35m'
  C_cyan='\e[0;36m';   C_bcyan='\e[1;36m'
  C_reset='\e[0m'
}

disable_colors() {
  C_red="";    C_bred=""
  C_green="";  C_bgreen=""
  C_yellow=""; C_byellow=""
  C_blue="";   C_bblue=""
  C_purple=""; C_bpurple=""
  C_cyan="";   C_bcyan=""
  C_reset=""
}

log()  { echo >&2 -e ">>> $C_green$*$C_reset"; }
warn() { echo >&2 -e "[W] $C_byellow$*$C_reset"; }
err()  { echo >&2 -e "[E] $C_bred$*$C_reset"; }

die() {
  if [[ -n "$@" ]]; then err "$@"; fi
  exit 1
}

fail() {
  if [[ -n "$@" ]]; then err "$@"; fi
  return 1
}

warn_or_die() {
  local dont_die="${1:-}"
  shift
  case "$dont_die" in
  (y|Y) warn "$@" ;;
  (*)   die "$@" ;;
  esac
}

# is_command_available <command> [ <path> ]
#
# Success, if command found in <path> (if passed), or other PATH locations; fail otherwise.
is_command_available() {
  local cmd="${1:?}"
  local path_prefix="${2:-}"
  env PATH="${path_prefix:+"$path_prefix:"}$PATH" which "${1:?}" &>/dev/null
}

# get_version_component "7.8.9" 2   # => 8
get_version_component() {
  echo "$1" | cut -d. -f"$2"
}

# => "1.2.3"
get_clang_version() { clang -v 2>&1 | grep 'clang version' | grep -Po '\d+\.\d+.\d+' ; }

# => "1.2.3"
get_gcc_version() { gcc -dumpversion ; }

# Get various configuration from /etc/os-release file.
get_gnu_os_distrib_name()    { ( . /etc/os-release && echo "${NAME:-}" ) ; }
get_gnu_os_id()              { ( . /etc/os-release && echo "${ID:-}" ) ; }
get_gnu_os_distrib_version() { ( . /etc/os-release && echo "${VERSION_ID:-}" ) ; }

#---------- some preliminary actions ----------#

__isatty() {
  test -t 1 && test -t 2
}
# enable colors for tty
disable_colors
if __isatty ; then
  enable_colors
fi
unset -f __isatty
