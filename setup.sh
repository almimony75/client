#!/bin/bash
#
# setup.sh
# Purpose: Install dependencies, build the project, and create a systemd USER service
#          for the Sarah client on Arch Linux.
#
# Usage:
#   1. Clone your Sarah-client repository.
#   2. cd into the repository directory.
#   3. Run: sudo ./setup.sh
#   4. Then, as your user (no sudo):
#        systemctl --user daemon-reload
#        systemctl --user enable --now sarah-client.service
#
export TERM=xterm
set -e

# --- Color Definitions & Log Helpers ---
NC='\033[0m'; RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[1;36m'
info()    { echo -e "${CYAN}[INFO] $1${NC}"; }
success() { echo -e "${GREEN}[✓] $1${NC}"; }
error()   { echo -e "${RED}[✗] $1${NC}"; }
warn()    { echo -e "${YELLOW}[!] $1${NC}"; }

# --- Helper to center banner text ---
center_text() {
  local term_width
  term_width=$(tput cols)
  while IFS= read -r line; do
    local stripped
    stripped=$(echo -e "$line" | sed 's/\x1B\[[0-9;]*[a-zA-Z]//g')
    local len=${#stripped}
    if (( len < term_width )); then
      printf "%*s%s\n" $(( (term_width - len) / 2 )) "" "$line"
    else
      echo "$line"
    fi
  done
}

# --- Pre-flight checks ---
check_root() {
  if [[ $EUID -ne 0 ]]; then
    error "Run this script with sudo."
    exit 1
  fi
  if [[ -z "$SUDO_USER" ]]; then
    error "Do not run directly as root; use sudo."
    exit 1
  fi
  [[ -d "src" ]] || { error "Run this script from the project root (src/ not found)."; exit 1; }
}

# --- Banner ---
echo -e "${CYAN}"
cat << "EOF" | center_text
███╗   ███╗██╗███╗   ███╗ ██████╗ ███╗   ██╗██╗   ██╗
████╗ ████║██║████╗ ████║██╔═══██╗████╗  ██║╚██╗ ██╔╝
██╔████╔██║██║██╔████╔██║██║   ██║██╔██╗ ██║ ╚████╔╝ 
██║╚██╔╝██║██║██║╚██╔╝██║██║   ██║██║╚██╗██║  ╚██╔╝  
██║ ╚═╝ ██║██║██║ ╚═╝ ██║╚██████╔╝██║ ╚████║   ██║   
╚═╝     ╚═╝╚═╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═══╝   ╚═╝   
EOF
echo -e "${CYAN}    Client Setup Script for Arch Linux${NC}\n" | center_text

check_root
info "--- Sarah Client Setup Started ---"
echo

# === Stage 1: Install system dependencies ===
info "Installing system dependencies..."
pacman -Syu --noconfirm
PACKAGES=(base-devel cmake git portaudio espeak-ng)
pacman -S --needed "${PACKAGES[@]}" --noconfirm
success "Dependencies installed."
echo

# === Stage 2: Build project ===
info "Preparing build directories..."
mkdir -p lib include models keywords

if [[ -f "lib/libpv_porcupine.so" ]]; then
  info "Porcupine library already present."
else
  info "Downloading Picovoice Porcupine..."
  TMP=$(mktemp -d)
  git clone --quiet --recurse-submodules https://github.com/Picovoice/porcupine.git "$TMP"
  cp "$TMP"/lib/linux/x86_64/libpv_porcupine.so ./lib/
  cp "$TMP"/lib/common/porcupine_params.pv ./models/
  cp "$TMP"/include/{picovoice.h,pv_porcupine.h} ./include/
  rm -rf "$TMP"
  success "Porcupine set up locally."
fi

info "Compiling Sarah client..."
g++ src/wakeword.cpp src/main.cpp src/client.cpp src/recorder.cpp src/configLoader.cpp src/AppLogger.cpp \
   -I include -O3 -flto \
   -lportaudio \
   -L./lib -Wl,-rpath,'$ORIGIN/lib' -lpv_porcupine \
   -o sarah-client
chown "$SUDO_USER:$SUDO_USER" sarah-client
success "Build complete (./sarah-client)."
echo

# === Stage 3: Create systemd user service ===
info "Creating systemd user service..."

SERVICE_USER=$SUDO_USER
USER_HOME=$(eval echo "~$SERVICE_USER")
USER_SERVICE_DIR="$USER_HOME/.config/systemd/user"
REPO_DIR=$(pwd)

mkdir -p "$USER_SERVICE_DIR"
chown -R "$SERVICE_USER:$SERVICE_USER" "$USER_HOME/.config"

cat > "$USER_SERVICE_DIR/sarah-client.service" <<EOF
[Unit]
Description=Sarah Voice Assistant Client (User Service)
After=network-online.target pulseaudio.service
Wants=network-online.target pulseaudio.service

[Service]
Type=simple
WorkingDirectory=$REPO_DIR
ExecStart=$REPO_DIR/sarah-client
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=default.target
EOF
success "Service file written to $USER_SERVICE_DIR/sarah-client.service"

info "Enabling lingering for $SERVICE_USER..."
loginctl enable-linger "$SERVICE_USER"
success "Lingering enabled."
echo

# === Final instructions ===
success "--- Sarah Client setup is complete ---"
warn "Do NOT move or delete this directory; the service depends on it."
echo
info "Next steps (run as $SERVICE_USER, no sudo):"
echo -e "  ${YELLOW}systemctl --user daemon-reload${NC}"
echo -e "  ${YELLOW}systemctl --user enable --now sarah-client.service${NC}"
echo
info "To view status/logs later:"
echo -e "  ${YELLOW}systemctl --user status sarah-client.service${NC}"
echo -e "  ${YELLOW}journalctl --user -u sarah-client.service -f${NC}"
echo
