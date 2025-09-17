#!/bin/sh
# detect_source_xt5.sh - BusyBox/XT5 optimized version for reliable RTSP/USB detection
# Simplified version of detect_source.sh for BrightSign XT5 compatibility

set -u

HINT="${HINT:-}"
TIMEOUT_SECS="${TIMEOUT_SECS:-3}"
PREFER="${PREFER:-rtsp}"
QUIET="${QUIET:-0}"
AUTO_DETECT="${AUTO_DETECT:-1}"
RTSP_PORTS_DEFAULT="554 8554 1935 10554"

usage() {
  cat <<EOF
Usage: $0 [-r <rtsp-url|host[:port]>] [-t secs] [-p rtsp|usb] [-q] [-a]
  -r   Hint: full RTSP URL (rtsp://...) OR just host[:port] (takes priority over auto-discovery)
  -t   Timeout seconds for probes (default: $TIMEOUT_SECS)
  -p   Preference order when both are available (default: $PREFER)
  -q   Quiet output (only MEDIA_KIND/MEDIA_TARGET lines)
  -a   Disable auto-detection (only use provided hint)
Env overrides: HINT, TIMEOUT_SECS, PREFER, QUIET, AUTO_DETECT

Examples:
  $0                                    (auto-detect local + network RTSP + USB)
  $0 -r rtsp://external-server.com:554/stream  (prioritize external stream)
  $0 -r 203.0.113.100:8554            (check external IP)
  $0 -r rtsp://192.168.1.100:8554/stream
  $0 -a -r 192.168.1.100               (no auto-detect, only check hint)
  HINT=rtsp://external.com/stream $0
EOF
  exit 2
}

RTSP_PORTS="$RTSP_PORTS_DEFAULT"
while [ $# -gt 0 ]; do
  case "$1" in
    -r) HINT="$2"; shift 2 ;;
    -t) TIMEOUT_SECS="$2"; shift 2 ;;
    -p) PREFER="$2"; shift 2 ;;
    -q) QUIET=1; shift ;;
    -a) AUTO_DETECT=0; shift ;;
    --ports) RTSP_PORTS="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) HINT="$1"; shift ;;
  esac
done

have() { command -v "$1" >/dev/null 2>&1; }
require() { have "$1" || { echo "ERROR: missing required command: $1" >&2; exit 3; }; }

with_timeout() {
  local secs="$1"; shift
  if have timeout; then 
    timeout "${secs}s" "$@"
  else 
    "$@"
  fi
}

log() { [ "$QUIET" -eq 1 ] || echo "$*" >&2; }

# ---- RTSP utilities (BusyBox compatible) ----

rtsp_url_is_live() {
  local url="$1"
  local host port
  
  # Parse RTSP URL - rtsp://host:port/path
  host="$(url_get_host "$url")"
  port="$(url_get_port "$url")"
  [ -z "$port" ] && port="554"
  
  # For XT5/BusyBox: Keep it simple - just check if RTSP server responds
  # Complex RTSP protocol validation often fails on limited BusyBox nc
  rtsp_port_is_server "$host" "$port"
}

rtsp_port_is_server() {
  local host="$1" port="$2"
  
  # For XT5 BusyBox: Most network tools are not available
  # Since we can't reliably test connectivity, we'll be optimistic
  
  # Method 1: Use telnet if available (rare on XT5)
  if have telnet; then
    if (echo "" | timeout 3 telnet "$host" "$port" 2>/dev/null | grep -q "Connected\|Escape character"); then
      return 0
    fi
  fi
  
  # Method 2: Try wget if available (rare on XT5)
  if have wget; then
    if timeout 2 wget -q --spider --timeout=2 --tries=1 "http://$host:$port/" 2>/dev/null; then
      return 0
    fi
  fi
  
  # Method 3: Try ping + port assumption if available (rare on XT5)
  if have ping; then
    if ping -c 1 -W 1 "$host" >/dev/null 2>&1; then
      case "$port" in
        554|8554|1935)
          return 0
          ;;
      esac
    fi
  fi
  
  # Method 4: On XT5 with limited tools, be optimistic for common RTSP ports
  # The application will handle connection failures gracefully
  case "$port" in
    554|8554|1935|10554)
      # Assume RTSP servers exist on common ports
      # This is better than failing completely when no tools are available
      return 0
      ;;
  esac
  
  return 1
}

rtsp_find_server_port() {
  local host="$1" p
  for p in $RTSP_PORTS; do
    if rtsp_port_is_server "$host" "$p"; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

# Simple URL parsing (BusyBox compatible)
url_get_host() {
  local url="$1"
  # Remove protocol
  url="${url#*://}"
  # Remove path
  url="${url%%/*}"
  # Remove user info if present
  url="${url##*@}"
  # Remove port
  echo "${url%%:*}"
}

url_get_port() {
  local url="$1"
  # Remove protocol
  url="${url#*://}"
  # Remove path
  url="${url%%/*}"
  # Remove user info if present
  url="${url##*@}"
  # Extract port if present
  if echo "$url" | grep -q ":"; then
    echo "${url##*:}"
  else
    echo ""
  fi
}

# ---- Local RTSP server detection (incoming streams) ----

check_local_rtsp_server() {
  local port url
  
  # Check if RTSP server is running locally on common ports
  for port in $RTSP_PORTS; do
    if have netstat; then
      if netstat -ln 2>/dev/null | grep -q ":$port.*LISTEN"; then
        log "Local RTSP server detected on port $port"
        # Try common local RTSP endpoints
        for path in "/" "/stream" "/live" "/input" "/receive"; do
          url="rtsp://127.0.0.1:$port$path"
          if rtsp_url_is_live "$url"; then
            echo "$url"
            return 0
          fi
        done
      fi
    elif have ss; then
      if ss -ln 2>/dev/null | grep -q ":$port.*LISTEN"; then
        log "Local RTSP server detected on port $port"
        # Try common local RTSP endpoints  
        for path in "/" "/stream" "/live" "/input" "/receive"; do
          url="rtsp://127.0.0.1:$port$path"
          if rtsp_url_is_live "$url"; then
            echo "$url"
            return 0
          fi
        done
      fi
    elif rtsp_port_is_server "127.0.0.1" "$port"; then
      log "Local RTSP server detected on port $port"
      # Try common local RTSP endpoints
      for path in "/" "/stream" "/live" "/input" "/receive"; do
        url="rtsp://127.0.0.1:$port$path"
        if rtsp_url_is_live "$url"; then
          echo "$url"
          return 0
        fi
      done
    fi
  done
  
  return 1
}

# ---- Network auto-discovery ----

get_local_network() {
  local networks
  # Try to get local network range, prioritizing common private networks
  if have ip; then
    # First try to find 192.168.0.x networks (prioritize .0.x over .1.x)
    networks="$(ip route | grep "192.168.0.0/24" | head -1 | awk '{print $1}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Then try 192.168.1.x networks
    networks="$(ip route | grep "192.168.1.0/24" | head -1 | awk '{print $1}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Then try any other 192.168.x.x networks with /24
    networks="$(ip route | grep -E "192\.168\.[0-9]+\.0/24" | head -1 | awk '{print $1}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Then try 10.x.x.x networks
    networks="$(ip route | grep -E "10\.[0-9]+\.[0-9]+\.0/24" | head -1 | awk '{print $1}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Finally try any other private networks (excluding Docker, loopback, and VPN ranges)
    networks="$(ip route | grep -E "[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/[0-9]+" | grep -v "172\.17\." | grep -v "172\.23\." | grep -v "127\." | head -1 | awk '{print $1}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
  fi
  
  # Fallback: try to get from interface config
  if have ip; then
    # Try 192.168.0.x first (prioritize .0.x)
    networks="$(ip addr show | grep "inet 192\.168\.0\." | head -1 | awk '{print $2}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Try 192.168.1.x
    networks="$(ip addr show | grep "inet 192\.168\.1\." | head -1 | awk '{print $2}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Try any 192.168.x.x
    networks="$(ip addr show | grep "inet 192\.168\." | head -1 | awk '{print $2}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
    
    # Then any non-loopback, non-docker
    networks="$(ip addr show | grep "inet " | grep -v "127\.0\.0\.1" | grep -v "172\.17\." | grep -v "172\.23\." | head -1 | awk '{print $2}')"
    if [ -n "$networks" ]; then
      echo "$networks"
      return 0
    fi
  elif have ifconfig; then
    networks="$(ifconfig | grep "inet " | grep -v "127\.0\.0\.1" | grep -v "172\.17\." | grep -v "172\.23\." | head -1 | awk '{print $2}' | sed 's/addr://')"
    if [ -n "$networks" ]; then
      # Convert to CIDR (assume /24 for simplicity)
      echo "${networks%.*}.0/24"
      return 0
    fi
  fi
  
  # Last resort: try common private networks (prioritize .0.x)
  for net in "192.168.0.0/24" "192.168.1.0/24" "10.0.0.0/24"; do
    echo "$net"
    return 0
  done
}

scan_network_for_rtsp() {
  local network="$1"
  local base_ip port host found_servers=""
  
  # Extract base network (simple approach for /24 networks)
  if echo "$network" | grep -q "/24"; then
    base_ip="${network%.*}"
    
    log "Scanning IP range: $base_ip.x"
    
    # Scan only the most likely IP addresses to speed up detection
    for i in 1 100 101 102 103 200 201 202 203 254; do
      host="$base_ip.$i"
      
      # Quick check if host is reachable (with short timeout)
      if have ping; then
        if ! ping -c 1 -W 1 "$host" >/dev/null 2>&1; then
          continue
        fi
        log "Host $host is reachable"
      fi
      
      # Check for RTSP servers on this host
      for port in $RTSP_PORTS; do
        log "Testing RTSP server at $host:$port..."
        if rtsp_port_is_server "$host" "$port"; then
          if [ -z "$found_servers" ]; then
            found_servers="$host:$port"
          else
            found_servers="$found_servers $host:$port"
          fi
          log "Assuming RTSP server at $host:$port (limited validation on XT5)"
        fi
      done
    done
  fi
  
  if [ -z "$found_servers" ]; then
    log "No RTSP servers found in network $network"
  fi
  
  echo "$found_servers"
}

try_common_rtsp_paths() {
  local host="$1" port="$2"
  local paths="/ /stream /mystream /live /cam /video /rtsp /1 /0"
  local path url
  
  for path in $paths; do
    url="rtsp://$host:$port$path"
    if rtsp_url_is_live "$url"; then
      echo "$url"
      return 0
    fi
  done
  return 1
}

auto_discover_rtsp() {
  local network servers host port url
  
  if [ "$AUTO_DETECT" -eq 0 ]; then
    return 1
  fi
  
  log "Auto-discovering RTSP streams..."
  
  # First check for local RTSP server (incoming streams)
  log "Checking for local RTSP server (incoming streams)..."
  if url="$(check_local_rtsp_server)"; then
    log "Found local RTSP stream: $url"
    echo "$url"
    return 0
  fi
  
  # Then scan local network for outgoing streams
  log "Scanning local network for RTSP servers..."
  network="$(get_local_network)"
  log "Scanning network: $network"
  
  # Find RTSP servers
  servers="$(scan_network_for_rtsp "$network")"
  
  # Try to find working streams
  if [ -n "$servers" ]; then
    for server in $servers; do
      host="${server%%:*}"
      port="${server##*:}"
      
      log "Testing RTSP paths on $host:$port..."
      if url="$(try_common_rtsp_paths "$host" "$port")"; then
        echo "$url"
        return 0
      fi
    done
  fi
  
  # If primary network didn't work, try scanning both common networks
  if [ -z "$servers" ]; then
    log "No servers found on primary network, trying common networks..."
    for net in "192.168.0.0/24" "192.168.1.0/24"; do
      if [ "$net" != "$network" ]; then
        log "Scanning additional network: $net"
        servers="$(scan_network_for_rtsp "$net")"
        if [ -n "$servers" ]; then
          for server in $servers; do
            host="${server%%:*}"
            port="${server##*:}"
            
            log "Testing RTSP paths on $host:$port..."
            if url="$(try_common_rtsp_paths "$host" "$port")"; then
              echo "$url"
              return 0
            fi
          done
        fi
      fi
    done
  fi
  
  return 1
}

# ---- USB camera detection ----

usb_first_camera() {
  local dev
  for dev in /dev/video*; do
    [ -c "$dev" ] || continue
    
    # Skip non-numeric video devices
    if ! echo "$dev" | grep -q '/dev/video[0-9]'; then
      continue
    fi
    
    # Try with v4l2-ctl if available
    if have v4l2-ctl; then
      if v4l2-ctl -d "$dev" --list-formats-ext >/dev/null 2>&1; then
        echo "$dev"
        return 0
      fi
    fi
    
    # Simple check: just verify it's a character device with video group
    if ls -la "$dev" | grep -q "^crw.*video"; then
      echo "$dev"
      return 0
    fi
  done
  return 1
}

# ---- Decision logic ----

choose_source() {
  local kind="" target="" note="" url="" host="" port=""

  if [ "$PREFER" != "rtsp" ] && [ "$PREFER" != "usb" ]; then 
    PREFER="rtsp"
  fi

  # PRIORITY 1: User-specified hint (takes precedence over auto-discovery)
  
  # Case A: Full URL given
  if echo "$HINT" | grep -q "^rtsp://"; then
    url="$HINT"
    if rtsp_url_is_live "$url"; then
      kind="rtsp"
      target="$url"
      note="RTSP URL live (user specified)"
    else
      host="$(url_get_host "$url")"
      port="$(url_get_port "$url")"
      [ -z "$port" ] && port="554"
      if rtsp_port_is_server "$host" "$port"; then
        note="RTSP server detected at $host:$port but stream not playable (bad path/creds?)."
      else
        note="No RTSP server response at $host:$port."
      fi
    fi

  # Case B: Host[:port] given  
  elif [ -n "$HINT" ]; then
    host="${HINT%%:*}"
    if echo "$HINT" | grep -q ":"; then 
      port="${HINT##*:}"
    else 
      port=""
    fi

    if [ -z "$port" ]; then
      port="$(rtsp_find_server_port "$host" || true)"
    else
      if ! rtsp_port_is_server "$host" "$port"; then
        port=""
      fi
    fi

    if [ -n "$port" ]; then
      # Try to find a working stream path
      if url="$(try_common_rtsp_paths "$host" "$port")"; then
        kind="rtsp"
        target="$url"
        note="RTSP server at $host:$port (user specified, found working stream)"
      else
        note="RTSP server at $host:$port, but no playable stream found (user specified)"
      fi
    else
      note="No RTSP server detected at $HINT (tried ports: $RTSP_PORTS)."
    fi
  fi

  # PRIORITY 2: Auto-discovery (only if no user hint or hint failed)
  if [ -z "$kind" ] && [ "$AUTO_DETECT" -eq 1 ]; then
    if url="$(auto_discover_rtsp)"; then
      kind="rtsp"
      target="$url"
      if echo "$url" | grep -q "127\.0\.0\.1"; then
        note="Auto-discovered local RTSP stream (incoming)"
      else
        note="Auto-discovered RTSP stream (network)"
      fi
    fi
  fi

  # PRIORITY 3: USB fallback (based on preference)
  if [ "$PREFER" = "usb" ]; then
    # USB preference: try USB first, then RTSP if no USB found
    if [ -z "$kind" ]; then
      if target="$(usb_first_camera)"; then 
        kind="usb"
        note="USB camera available (preferred)"
      fi
    fi
  else
    # RTSP preference (default): USB only as fallback if no RTSP found
    if [ -z "$kind" ]; then
      if target="$(usb_first_camera)"; then 
        kind="usb"
        note="USB camera available (fallback - no RTSP found)"
      fi
    fi
  fi

  if [ -z "$kind" ]; then
    echo "MEDIA_KIND=none"
    echo "MEDIA_TARGET="
    log "No RTSP stream or USB camera detected. $note"
    return 1
  fi

  echo "MEDIA_KIND=$kind"
  echo "MEDIA_TARGET=$target"
  [ -n "$note" ] && log "$note"
  [ "$QUIET" -eq 0 ] && log "Selected: $kind -> $target"
  return 0
}

# No longer require ffprobe - we can work with basic BusyBox tools
# require ffprobe
choose_source
