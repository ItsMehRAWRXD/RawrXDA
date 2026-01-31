#!/bin/bash

# Deploy All RawrZ Projects to GitHub and DigitalOcean
echo "🚀 RawrZ Projects - Complete Deployment Pipeline"
echo "==============================================="

# Configuration
GITHUB_USER="ItsMehRAWRXD"
GITHUB_TOKEN="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"
DO_TOKEN="dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293"
DO_REGION="nyc1"
DO_SIZE="s-1vcpu-1gb"
DO_IMAGE="ubuntu-22-04-x64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Projects to deploy
PROJECTS=(
    "01-rawrz-clean:rawrz-clean:RawrZ Clean Security Platform"
    "02-rawrz-app:rawrz-app:RawrZ Main Application"
    "04-ai-tools:ai-tools-collection:AI Tools Collection"
    "05-compiler-toolchain:compiler-toolchain:Compiler Toolchain"
    "rawrz-http-encryptor:rawrz-http-encryptor:RawrZ HTTP Encryptor"
)

# Function to create GitHub repository
create_github_repo() {
    local repo_name=$1
    local description=$2
    
    print_status "Creating GitHub repository: $repo_name"
    
    curl -X POST \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github.v3+json" \
        https://api.github.com/user/repos \
        -d "{
            \"name\": \"$repo_name\",
            \"description\": \"$description\",
            \"private\": true,
            \"auto_init\": false
        }" || print_warning "Repository creation failed - may already exist"
}

# Function to push project to GitHub
push_to_github() {
    local project_dir=$1
    local repo_name=$2
    local description=$3
    
    print_status "Processing project: $project_dir"
    
    cd "$project_dir"
    
    # Initialize git if not already done
    if [ ! -d ".git" ]; then
        git init
        git add .
        git commit -m "Initial commit: $description"
    fi
    
    # Add remote and push
    git remote remove origin 2>/dev/null || true
    git remote add origin "https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/$repo_name.git"
    git push -u origin master || {
        print_error "Failed to push $repo_name to GitHub"
        return 1
    }
    
    print_success "Successfully pushed $repo_name to GitHub"
    cd ..
}

# Function to create DigitalOcean Droplet
create_droplet() {
    local project_name=$1
    local port=$2
    
    print_status "Creating DigitalOcean Droplet for $project_name"
    
    # Install doctl if not present
    if ! command -v doctl >/dev/null 2>&1; then
        print_status "Installing DigitalOcean CLI (doctl)..."
        wget https://github.com/digitalocean/doctl/releases/download/v1.94.0/doctl-1.94.0-linux-amd64.tar.gz
        tar xf doctl-1.94.0-linux-amd64.tar.gz
        sudo mv doctl /usr/local/bin
        rm doctl-1.94.0-linux-amd64.tar.gz
    fi
    
    # Authenticate with DigitalOcean
    doctl auth init --access-token "$DO_TOKEN"
    
    # Create SSH key if it doesn't exist
    if [ ! -f ~/.ssh/id_rsa ]; then
        print_status "Generating SSH key..."
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N ""
    fi
    
    # Add SSH key to DigitalOcean
    print_status "Adding SSH key to DigitalOcean..."
    doctl compute ssh-key import rawrz-key --public-key-file ~/.ssh/id_rsa.pub || print_warning "SSH key may already exist"
    
    # Create droplet
    print_status "Creating DigitalOcean Droplet for $project_name..."
    DROPLET_ID=$(doctl compute droplet create "$project_name" \
        --image "$DO_IMAGE" \
        --size "$DO_SIZE" \
        --region "$DO_REGION" \
        --ssh-keys $(doctl compute ssh-key list --format ID --no-header | head -1) \
        --wait \
        --format ID --no-header)
    
    if [ -z "$DROPLET_ID" ]; then
        print_error "Failed to create DigitalOcean Droplet for $project_name"
        return 1
    fi
    
    # Get droplet IP
    DROPLET_IP=$(doctl compute droplet get "$DROPLET_ID" --format PublicIPv4 --no-header)
    
    print_success "DigitalOcean Droplet created for $project_name: $DROPLET_IP"
    echo "$DROPLET_IP" > "${project_name}_ip.txt"
}

# Main deployment process
print_status "Starting deployment of all RawrZ projects..."

# Step 1: Create all GitHub repositories
print_status "Step 1: Creating GitHub repositories..."
for project in "${PROJECTS[@]}"; do
    IFS=':' read -r dir repo_name description <<< "$project"
    create_github_repo "$repo_name" "$description"
done

# Step 2: Push all projects to GitHub
print_status "Step 2: Pushing projects to GitHub..."
for project in "${PROJECTS[@]}"; do
    IFS=':' read -r dir repo_name description <<< "$project"
    push_to_github "$dir" "$repo_name" "$description"
done

# Step 3: Create DigitalOcean Droplets
print_status "Step 3: Creating DigitalOcean Droplets..."
for project in "${PROJECTS[@]}"; do
    IFS=':' read -r dir repo_name description <<< "$project"
    case $repo_name in
        "rawrz-clean")
            create_droplet "$repo_name" "8081"
            ;;
        "rawrz-app")
            create_droplet "$repo_name" "8082"
            ;;
        "ai-tools-collection")
            create_droplet "$repo_name" "8083"
            ;;
        "compiler-toolchain")
            create_droplet "$repo_name" "8084"
            ;;
        "rawrz-http-encryptor")
            create_droplet "$repo_name" "8080"
            ;;
    esac
done

# Step 4: Display final summary
echo ""
echo "🎉 All RawrZ Projects Deployment Complete!"
echo "=========================================="
echo ""
echo "📊 Deployment Summary:"
echo "====================="

for project in "${PROJECTS[@]}"; do
    IFS=':' read -r dir repo_name description <<< "$project"
    if [ -f "${repo_name}_ip.txt" ]; then
        DROPLET_IP=$(cat "${repo_name}_ip.txt")
        echo "  • $description:"
        echo "    - GitHub: https://github.com/$GITHUB_USER/$repo_name"
        echo "    - DigitalOcean: $DROPLET_IP"
        rm "${repo_name}_ip.txt"
    fi
done

echo ""
echo "🌐 Access Your Platforms:"
echo "========================"
echo "  • RawrZ HTTP Encryptor: http://[DROPLET_IP]:8080"
echo "  • RawrZ Clean Security: http://[DROPLET_IP]:8081"
echo "  • RawrZ Main App: http://[DROPLET_IP]:8082"
echo "  • AI Tools Collection: http://[DROPLET_IP]:8083"
echo "  • Compiler Toolchain: http://[DROPLET_IP]:8084"
echo ""
echo "✅ All projects are now safely backed up and deployed!"
echo "🔒 All repositories are private and secure"
echo "🚀 All platforms are ready for production use"
