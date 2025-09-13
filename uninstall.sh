#!/bin/bash
#
# uninstall.sh
# Purpose: Stop, disable, and remove the sarah-client *user* systemd service.
#
# Usage:
#   1. cd into the repository directory.
#   2. Run with sudo:  sudo ./uninstall.sh
#   3. Then, as your normal user, run:
#        systemctl --user stop sarah-client.service
#        systemctl --user disable sarah-client.service
#
export TERM=xterm
set -e

# --- Colors & log helpers ---
NC='\033[0m'; RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[1;36m'
info()    { echo -e "${CYAN}[INFO] $1${NC}"; }
success() { echo -e "${GREEN}[✓] $1${NC}"; }
error()   { echo -e "${RED}[✗] $1${NC}"; }

# --- Center banner ---
center_text() {
  local w; w=$(tput cols)
  while IFS= read -r line; do
    local l=${#line}
    (( l < w )) && printf "%*s%s\n" $(( (w-l)/2 )) "" "$line" || echo "$line"
  done
}

# --- Root check ---
[[ $EUID -eq 0 ]] || { error "Run this script with sudo."; exit 1; }
[[ -n "$SUDO_USER" ]] || { error "Do not run directly as root; use sudo."; exit 1; }

SERVICE_USER=$SUDO_USER
USER_HOME=$(eval echo "~$SERVICE_USER")
SERVICE_FILE="$USER_HOME/.config/systemd/user/sarah-client.service"

# Banner
echo -e "${CYAN}"
cat << "EOF" | center_text
███╗   ███╗██╗███╗   ███╗ ██████╗ ███╗   ██╗██╗   ██╗
████╗ ████║██║████╗ ████║██╔═══██╗████╗  ██║╚██╗ ██╔╝
██╔████╔██║██║██╔████╔██║██║   ██║██╔██╗ ██║ ╚████╔╝ 
██║╚██╔╝██║██║██║╚██╔╝██║██║   ██║██║╚██╗██║  ╚██╔╝  
██║ ╚═╝ ██║██║██║ ╚═╝ ██║╚██████╔╝██║ ╚████║   ██║   
╚═╝     ╚═╝╚═╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═══╝   ╚═╝   
EOF
echo -e "${CYAN}   Sarah Client Uninstallation${NC}\n" | center_text

# Remove service file
if [[ -f "$SERVICE_FILE" ]]; then
    info "Removing user service file: $SERVICE_FILE"
    rm -f "$SERVICE_FILE"
    success "Service file deleted."
else
    info "No user service file found at $SERVICE_FILE"
fi

# Reload the user manager later
echo
warn="[Manual step]"
info "To stop & disable the service, run as $SERVICE_USER (no sudo):"
echo "  systemctl --user stop sarah-client.service"
echo "  systemctl --user disable sarah-client.service"
echo
info "Then reload the user daemon:"
echo "  systemctl --user daemon-reload"
echo
success "--- Sarah Client files have been removed ---"
