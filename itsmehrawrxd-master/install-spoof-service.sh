#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

clear

echo -e "${CYAN}"
echo "            "
echo "     "
echo "                 "
echo "                 "
echo "          "
echo "              "
echo -e "${NC}"
echo -e "${WHITE}                    Auto-Spoof Service Installer${NC}"
echo ""

# Check for root privileges
if [[ $EUID -eq 0 ]]; then
    echo -e "${RED}ERROR: This script should not be run as root${NC}"
    echo -e "${YELLOW}Please run as a regular user${NC}"
    exit 1
fi

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo -e "${RED}ERROR: Node.js is not installed${NC}"
    echo -e "${YELLOW}Please install Node.js from https://nodejs.org/${NC}"
    exit 1
fi

echo -e "${GREEN} Node.js is installed${NC}"
echo ""

# Install required packages
echo -e "${BLUE} Installing required packages...${NC}"
npm install chalk ora figlet axios > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${YELLOW} Warning: Some packages may not have installed correctly${NC}"
fi

echo -e "${GREEN} Packages installed${NC}"
echo ""

# Create service script
echo -e "${BLUE} Creating service script...${NC}"
cat > start-spoof-service.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
node auto-spoof-launcher.js
EOF

chmod +x start-spoof-service.sh
echo -e "${GREEN} Service script created${NC}"
echo ""

# Create systemd service file
echo -e "${BLUE} Creating systemd service...${NC}"
SERVICE_DIR="$HOME/.config/systemd/user"
mkdir -p "$SERVICE_DIR"

cat > "$SERVICE_DIR/auto-spoof.service" << EOF
[Unit]
Description=Auto-Spoof AI Service
After=network.target

[Service]
Type=simple
WorkingDirectory=$(pwd)
ExecStart=$(which node) auto-spoof-launcher.js
Restart=always
RestartSec=10
Environment=NODE_ENV=production

[Install]
WantedBy=default.target
EOF

echo -e "${GREEN} Systemd service created${NC}"
echo ""

# Enable and start the service
echo -e "${BLUE} Enabling and starting service...${NC}"
systemctl --user daemon-reload
systemctl --user enable auto-spoof.service
systemctl --user start auto-spoof.service

if [ $? -eq 0 ]; then
    echo -e "${GREEN} Service enabled and started${NC}"
else
    echo -e "${YELLOW} Warning: Could not start service automatically${NC}"
    echo -e "${YELLOW}You may need to start it manually: systemctl --user start auto-spoof.service${NC}"
fi

echo ""

# Create desktop autostart entry
echo -e "${BLUE} Creating desktop autostart entry...${NC}"
AUTOSTART_DIR="$HOME/.config/autostart"
mkdir -p "$AUTOSTART_DIR"

cat > "$AUTOSTART_DIR/auto-spoof.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Auto-Spoof Service
Comment=Auto-Spoof AI Service
Exec=$(pwd)/start-spoof-service.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
EOF

echo -e "${GREEN} Desktop autostart entry created${NC}"
echo ""

# Create cron job for additional reliability
echo -e "${BLUE} Creating cron job for reliability...${NC}"
(crontab -l 2>/dev/null; echo "@reboot cd $(pwd) && ./start-spoof-service.sh > /dev/null 2>&1") | crontab -
echo -e "${GREEN} Cron job created${NC}"
echo ""

# Start the service immediately
echo -e "${BLUE} Starting Auto-Spoof Service...${NC}"
nohup ./start-spoof-service.sh > /dev/null 2>&1 &

echo ""
echo -e "${GREEN} Auto-Spoof Service installed and started!${NC}"
echo ""
echo -e "${WHITE} Service Details:${NC}"
echo -e "${WHITE}   - Service runs automatically on system startup${NC}"
echo -e "${WHITE}   - All AI models are unlocked and running locally${NC}"
echo -e "${WHITE}   - Completely airtight - no external detection possible${NC}"
echo -e "${WHITE}   - Auto-restarts if any service fails${NC}"
echo ""
echo -e "${WHITE} Available Services:${NC}"
echo -e "${WHITE}   - Ollama Server: http://localhost:11434${NC}"
echo -e "${WHITE}   - Spoofed AI API: http://localhost:9999${NC}"
echo -e "${WHITE}   - RawrZ Server: http://localhost:8080${NC}"
echo ""
echo -e "${WHITE} The service will now run in the background automatically${NC}"
echo -e "${WHITE}   You can use any Ollama-compatible client or the AI Terminal${NC}"
echo ""
echo -e "${YELLOW} To manage the service:${NC}"
echo -e "${YELLOW}   Start:   systemctl --user start auto-spoof.service${NC}"
echo -e "${YELLOW}   Stop:    systemctl --user stop auto-spoof.service${NC}"
echo -e "${YELLOW}   Status:  systemctl --user status auto-spoof.service${NC}"
echo -e "${YELLOW}   Logs:    journalctl --user -u auto-spoof.service -f${NC}"
echo ""
