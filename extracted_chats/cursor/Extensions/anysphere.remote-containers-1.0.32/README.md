# Cursor Dev Containers

This extension enables you to use Docker containers as your development environment. It provides a consistent, isolated workspace with all the tools and dependencies you need.

## What This Extension Does

- Runs your development environment inside a Docker container
- Mounts your workspace files into the container
- Installs and runs extensions inside the container
- Provides seamless integration with Cursor as if everything were running locally

## Prerequisites

### Docker Installation Options

You can use Docker in several ways:
- Local Docker installation
- Remote Docker environment
- Other Docker-compliant CLIs (unofficially supported)
- Kubernetes pods (requires `kubectl`)

### System Requirements

#### Local Docker Setup

**Windows:**
- [Docker Desktop](https://www.docker.com/products/docker-desktop) with WSL2 Backend

**macOS:**
- [Docker Desktop](https://www.docker.com/products/docker-desktop)

**Linux:**
- [Docker CE/EE](https://docs.docker.com/install/#supported-platforms) 18.06+
- [Docker Compose](https://docs.docker.com/compose/install) 1.21+
- Note: Ubuntu snap package is not supported

#### Supported Container Systems
- Debian 9+
- Ubuntu 16.04+
- CentOS / RHEL 7+
- Alpine Linux

x86_64 and arm64 architectures are supported.

## Installation Guide

### Windows / macOS Setup

1. Install [Docker Desktop](https://www.docker.com/products/docker-desktop)
2. For Windows - Ensure that WSL2 is enabled:
   - Open Docker Desktop settings
   - Enable **Use the WSL2 based engine**
   - Verify your distribution under **Resources > WSL Integration**

### Linux Setup

1. Install Docker CE/EE following [official instructions](https://docs.docker.com/install/#supported-platforms)
2. Install Docker Compose if needed
3. Add your user to the docker group:
   ```bash
   sudo usermod -aG docker $USER
   ```
4. Sign out and back in for changes to take effect

## Git Integration Tips

- For Windows users: Configure consistent line endings when working with the same repository in both container and Windows
- Git credentials are automatically shared with containers
- SSH keys can be shared with containers (see [Sharing Git credentials](https://aka.ms/vscode-remote/containers/git))

## Alpine Linux Support

- Requires Cursor v0.50.5 or newer
- Required packages: `bash`, `libstdc++`, and `wget`
- Add to your Dockerfile:
  ```dockerfile
  RUN apk add --no-cache bash libstdc++ wget
  ```

## Opening Remote Containers via the CLI

It is possible to open workspaces in a remote container directly via the `cursor` CLI via the following command:

```bash
cursor --folder-uri vscode-remote://<authority>+<spec>[@(wsl|ssh-remote)+<nested spec>]/<folder path>
```

The `authority` can be either `attached-container`, `dev-container`, or `k8s-container`, depending on which type of container you're connecting to.
The `spec` is a hex-encoded representation of a JSON blob containing the specification for the container.
When connecting to a container located in a remove environment (e.g. over `ssh` or `wsl`), the remote authorities can be nested. Cursor will first resolve the innermost authority,
and then connect to the container accessible on that remote host.

### Attached Containers


The schema for the `spec` is:
```json
{
  settingType: 'container';  // This must be the literal string 'container'
  containerId: string;       // The ID of the container
}
```

For example, to open the `/app` folder on the `6021b49999b7` container:

```bash
CONF='{"settingType":"container", "containerId": "6021b49999b7"}'
HEX_CONF=$(printf "$CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri "vscode-remote://attached-container+${HEX_CONF}/app"
```


### Dev Containers


The schema for the `spec` is:
```json
{
  settingType: 'config';    // This must be the literal string 'config'
  workspacePath: string;    // The path to the workspace
  devcontainerPath: string; // The path to the devcontainer.json file within the workspace
}
```

For example, to open the `/workspaces/repo` folder located within `/home/user/repo` (with the `.devcontainer.json` file at `/home/user/repo/.devcontainer/devcontainer.json`):

```bash
CONF='{"settingType":"config", "workspacePath": "/home/user/repo", "devcontainerPath": "/home/user/repo/.devcontainer/devcontainer.json"}'
HEX_CONF=$(printf "$CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri "vscode-remote://dev-container+${HEX_CONF}/workspaces/repo"
```



### Kubernetes Pods

The schema for the `spec` is:
```json
{
  settingType: 'pod'; // This must be the literal string 'pod'
  podname: string;    // The pod name
  name: string;       // The container name (within the pod)
  context: string;    // The local kubernetes context
  namespace: string;  // The kubernetes namespace
}
```

For example, to open the `/app` folder on the `ubuntu:ubuntu` pod in the `default` namespace:

```bash
CONF='{"settingType":"pod", "podname": "ubuntu", "name": "ubuntu", "context": "docker-desktop", "namespace": "default"}'
HEX_CONF=$(printf "$CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri "vscode-remote://k8s-container+${HEX_CONF}/app"
```

### Nested Containers over WSL

If you would like to connect to a container from _within_ WSL (Windows Subsystem for Linux), use the nested syntax. This is useful when the `devcontainer.json` file, or kubernetes config, is located within WSL.

For example, to connect to a dev container located within `/home/user/repo` (with the `.devcontainer.json` file at `/home/user/repo/.devcontainer/devcontainer.json`) in the `Ubuntu-24.04` WSL distribution

```bash
CONF='{"settingType":"config", "workspacePath": "/home/user/repo", "devcontainerPath": "/home/user/repo/.devcontainer/devcontainer.json"}'
HEX_CONF=$(printf "$CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri vscode-remote://dev-container+${HEX_CONF}@wsl+Ubuntu-24.04/workspaces/repo
```

### Nested Containers over SSH

If you would like to connect to a container that is accessible over a SSH host, use the nested syntax. This is useful when the container is on the SSH host, or you must jump via this SSH host to access a Kubernetes cluster.

For example, to connect to a dev container located within `/home/user/repo` (with the `.devcontainer.json` file at `/home/user/repo/.devcontainer/devcontainer.json`) on an SSH host named `loginnode`:

```bash
CONF='{"settingType":"config", "workspacePath": "/home/user/repo", "devcontainerPath": "/home/user/repo/.devcontainer/devcontainer.json"}'
HEX_CONF=$(printf "$CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri vscode-remote://dev-container+${HEX_CONF}@ssh-remote+loginnode/workspaces/repo
```

When it's not possible use a short hostname, you can also specify the full connection string (e.g. what would be passed into `ssh` command line) as another hex-encoded JSON blob. For example, to explicitly specify the username `user`, port `22` on host `76.76.21.21`:


```bash
DOCKER_CONF='{"settingType":"config", "workspacePath": "/home/user/repo", "devcontainerPath": "/home/user/repo/.devcontainer/devcontainer.json"}'
DOCKER_HEX_CONF=$(printf "$DOCKER_CONF" | od -A n -t x1 | tr -d '[\n\t ]')
SSH_CONF='{"hostName":"user@76.76.21.21 -p 22"}'
SSH_HEX_CONF=$(printf "$SSH_CONF" | od -A n -t x1 | tr -d '[\n\t ]')
cursor --folder-uri vscode-remote://dev-container+${DOCKER_HEX_CONF}@ssh-remote+${SSH_HEX_CONF}/workspaces/repo
```

## Security Warning

⚠️ Only connect to trusted containers. While docker provides some isolation, it is not foolproof. A compromised remote system could potentially execute code on your local machine through the Remote Containers connection.


## Additional Resources

- [Alternative Docker Options](https://code.visualstudio.com/remote/advancedcontainers/docker-options)
- [Troubleshooting Guide](https://aka.ms/vscode-remote/containers/troubleshooting)
- [Remote Docker Host Setup](https://aka.ms/vscode-remote/containers/remote-host)
