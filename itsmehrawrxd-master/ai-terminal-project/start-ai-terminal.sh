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
echo "                    "
echo "            "
echo "                  "
echo "                 "
echo "                   "
echo "                       "
echo -e "${NC}"
echo -e "${WHITE}                    Complete Unlock System - All AI Models Available Offline${NC}"
echo ""

echo -e "${GREEN}Starting AI Terminal...${NC}"
echo ""

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo -e "${RED}ERROR: Node.js is not installed or not in PATH${NC}"
    echo -e "${YELLOW}Please install Node.js from https://nodejs.org/${NC}"
    exit 1
fi

# Check if required packages are installed
echo -e "${BLUE}Checking dependencies...${NC}"
if ! npm list chalk ora figlet axios &> /dev/null; then
    echo -e "${YELLOW}Installing required packages...${NC}"
    npm install chalk ora figlet axios
    if [ $? -ne 0 ]; then
        echo -e "${RED}ERROR: Failed to install dependencies${NC}"
        exit 1
    fi
fi

# Start the AI Terminal
echo -e "${GREEN}Starting AI Terminal...${NC}"
echo ""
node ai-terminal.js
