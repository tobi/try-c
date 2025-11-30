try() {
  if [ "$1" = "init" ]; then
    /usr/local/bin/try "$@"
    return
  fi
  tmp=$(mktemp)
  /usr/local/bin/try "$@" > "$tmp"
  ret=$?
  if [ $ret -eq 0 ]; then
    . "$tmp"
  fi
  rm -f "$tmp"
  return $ret
}
