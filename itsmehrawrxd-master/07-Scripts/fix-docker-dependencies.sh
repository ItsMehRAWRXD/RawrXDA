#!/bin/bash
# Fix Docker Dependencies on Ubuntu/Debian
# Resolves containerd.io conflicts

echo "Fixing Docker Dependencies..."
echo "============================="

# Remove existing Docker packages
echo "Removing existing Docker packages..."
apt-get remove -y docker docker-engine docker.io containerd runc
apt-get purge -y docker docker-engine docker.io containerd runc

# Clean up any remaining packages
apt-get autoremove -y
apt-get autoclean

# Update package list
echo "Updating package list..."
apt-get update

# Install prerequisites
echo "Installing prerequisites..."
apt-get install -y \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg \
    lsb-release

# Add Docker's official GPG key
echo "Adding Docker GPG key..."
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

# Add Docker repository
echo "Adding Docker repository..."
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | tee /etc/apt/sources.list.d/docker.list > /dev/null

# Update package list again
echo "Updating package list with Docker repository..."
apt-get update

# Install Docker Engine
echo "Installing Docker Engine..."
apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

# Start and enable Docker
echo "Starting Docker service..."
systemctl start docker
systemctl enable docker

# Add current user to docker group
echo "Adding user to docker group..."
usermod -aG docker $USER

# Verify installation
echo "Verifying Docker installation..."
docker --version
docker-compose --version

echo "Docker installation completed successfully!"
echo "You may need to log out and back in for group changes to take effect."
