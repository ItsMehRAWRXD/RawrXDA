# DigitalOcean Terraform Configuration
# Deploy minimal infrastructure for 2GB quantum GGUF hosting
# Cost: $11/month (Spaces $5 + Basic Droplet $6)

terraform {
  required_version = ">= 1.0"
  required_providers {
    digitalocean = {
      source  = "digitalocean/digitalocean"
      version = "~> 2.28"
    }
  }
}

provider "digitalocean" {
  token = var.do_token
}

# DigitalOcean Spaces Bucket for Model Storage
resource "digitalocean_spaces_bucket" "models" {
  name   = var.bucket_name
  region = var.region
  acl    = "public-read"

  lifecycle_rule {
    enabled = true
    days    = 90
    prefix  = "archive/"
  }

  tags = ["rawrxd", "models", "quantum"]
}

# CDN endpoint for models (auto-configured with TLS)
resource "digitalocean_cdn" "models_cdn" {
  origin           = digitalocean_spaces_bucket.models.bucket_domain_name
  certificate_id   = digitalocean_certificate.default.id
  custom_domain    = var.cdn_domain
  ttl              = 3600

  depends_on = [digitalocean_certificate.default]
}

# TLS Certificate for CDN
resource "digitalocean_certificate" "default" {
  name             = "${var.bucket_name}-cert"
  type             = "lets_encrypt"
  domains          = [var.cdn_domain]
  validation_method = "dns"
}

# Basic Droplet (1 vCPU / 1 GB RAM / 25 GB SSD @ $6/month)
resource "digitalocean_droplet" "llama_server" {
  image              = "docker-20-04"  # Docker pre-installed
  name               = var.droplet_name
  region             = var.region
  size               = var.droplet_size  # s-1vcpu-1gb
  backups            = false             # Disable backups (save 20%)
  ipv6               = true
  monitoring         = true

  ssh_keys = [digitalocean_ssh_key.deploy.fingerprint]

  tags = ["rawrxd", "llama-server", "quantum"]

  user_data = base64encode(templatefile("${path.module}/user_data.sh", {
    MODELS_BUCKET   = digitalocean_spaces_bucket.models.name
    SPACES_REGION   = var.region
    SPACES_KEY      = var.do_spaces_key
    SPACES_SECRET   = var.do_spaces_secret
  }))

  lifecycle {
    ignore_changes = [user_data]
  }
}

# Firewall to restrict access
resource "digitalocean_firewall" "llama" {
  name = "${var.droplet_name}-fw"

  droplet_ids = [digitalocean_droplet.llama_server.id]

  # Allow SSH (limited to your IP for security)
  inbound_rule {
    protocol         = "tcp"
    port_range       = "22"
    sources {
      addresses = var.allowed_ssh_ips
    }
  }

  # Allow HTTP API access (port 8080)
  inbound_rule {
    protocol   = "tcp"
    port_range = "8080"
    sources {
      addresses = ["0.0.0.0/0", "::/0"]
    }
  }

  # Allow outbound access (download models)
  outbound_rule {
    protocol   = "tcp"
    port_range = "443"
    destinations {
      addresses = ["0.0.0.0/0", "::/0"]
    }
  }

  outbound_rule {
    protocol   = "tcp"
    port_range = "80"
    destinations {
      addresses = ["0.0.0.0/0", "::/0"]
    }
  }

  tags = ["rawrxd"]
}

# SSH Key for droplet access
resource "digitalocean_ssh_key" "deploy" {
  name       = "rawrxd-deploy-key"
  public_key = var.ssh_public_key
}

# Firewall rule for reserved IP (future scaling)
resource "digitalocean_reserved_ip" "server" {
  region   = var.region
  droplet_id = digitalocean_droplet.llama_server.id
}

# Outputs
output "droplet_ip" {
  description = "Public IP of llama.cpp server droplet"
  value       = digitalocean_droplet.llama_server.ipv4_address
}

output "droplet_ipv6" {
  description = "IPv6 address of droplet"
  value       = digitalocean_droplet.llama_server.ipv6_address
}

output "api_endpoint" {
  description = "llama.cpp API endpoint"
  value       = "http://${digitalocean_droplet.llama_server.ipv4_address}:8080"
}

output "spaces_bucket" {
  description = "S3-compatible model storage bucket"
  value       = digitalocean_spaces_bucket.models.name
}

output "cdn_domain" {
  description = "CDN domain for model downloads"
  value       = "https://${digitalocean_spaces_bucket.models.name}.${var.region}.cdn.digitaloceanspaces.com"
}

output "monthly_cost" {
  description = "Estimated monthly cost"
  value       = "Spaces: \$5/month + Droplet: \$6/month = \$11/month total"
}

output "credit_runway" {
  description = "Runway on $200 DigitalOcean credit"
  value       = "18 months of free hosting"
}
