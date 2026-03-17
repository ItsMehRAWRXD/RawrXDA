# Terraform Variables for DigitalOcean Deployment

variable "do_token" {
  description = "DigitalOcean API token"
  type        = string
  sensitive   = true
}

variable "do_spaces_key" {
  description = "DigitalOcean Spaces API key"
  type        = string
  sensitive   = true
}

variable "do_spaces_secret" {
  description = "DigitalOcean Spaces API secret"
  type        = string
  sensitive   = true
}

variable "ssh_public_key" {
  description = "SSH public key for droplet access"
  type        = string
  sensitive   = true
}

variable "bucket_name" {
  description = "Spaces bucket name for GGUF models"
  type        = string
  default     = "rawrxd-quantum-models"
}

variable "region" {
  description = "DigitalOcean region"
  type        = string
  default     = "nyc3"
  validation {
    condition     = contains(["nyc1", "nyc3", "sfo1", "sfo3", "lon1", "ams3", "fra1", "sgp1", "blr1", "tor1", "syd1"], var.region)
    error_message = "Invalid DigitalOcean region."
  }
}

variable "droplet_name" {
  description = "Droplet hostname"
  type        = string
  default     = "rawrxd-llama-server"
}

variable "droplet_size" {
  description = "Droplet size slug"
  type        = string
  default     = "s-1vcpu-1gb"  # $6/month
  validation {
    condition     = contains(["s-1vcpu-1gb", "s-1vcpu-2gb", "s-2vcpu-2gb", "s-3vcpu-1gb", "s-2vcpu-4gb"], var.droplet_size)
    error_message = "Invalid droplet size."
  }
}

variable "cdn_domain" {
  description = "Custom CDN domain (e.g., models.rawrxd.dev)"
  type        = string
  default     = "models.rawrxd.dev"
}

variable "allowed_ssh_ips" {
  description = "CIDR blocks allowed SSH access"
  type        = list(string)
  default     = ["0.0.0.0/0"]  # Change to your IP for security
  validation {
    condition     = length(var.allowed_ssh_ips) > 0
    error_message = "At least one SSH IP must be specified."
  }
}
