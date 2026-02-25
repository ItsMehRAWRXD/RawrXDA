# Cursor Remote Containers Changelog

# 1.0.32
- Refactor GPG agent forwarding on Windows to use the assuan protocol

# 1.0.31
- Add GPG agent forwarding support for commit signing inside dev containers
  - New setting `dev.containers.enableGPGAgentForwarding` (default: true) to enable/disable GPG agent forwarding
  - Automatically exports and imports GPG public keys and ownertrust into containers
  - Forwards the host `gpg-agent` socket into Docker and Kubernetes containers
  - Supports Windows GPG4Win Assuan sockets with proper nonce authentication

# 1.0.30
- Bug fixes and improvements

# 1.0.29
- Fix a performance issue where spinners continued forever after all reconnection attempts were exhausted

# 1.0.28
- Fix issues when using devcontainer templates that hung at the "Container Started" step
- Bump Devcontainers CLI to 0.80.2
- Prompt for the architecture when it cannot be determined automatically

# 1.0.27
- Bind to "0.0.0.0" on macOS when forwarding ports < 1024

# 1.0.26
- Improve performance when searching for `devcontainer.json` files

# 1.0.25
- When recreating an attached Container, Cursor will fallback to looking up the container by name if the original container ID no longer exists.

# 1.0.24
- Bug fixes and improvements

# 1.0.23
- Added the setting `dev.containers.showReopenInContainerPrompt` to control whether prompts to reopen folders in containers should be shown (default: true).
- Fixed issues where, when using Docker over WSL or Docker over SSH, the extension would incorrectly detect whether the Anysphere Remote SSH or WSL extension is installed.
- Deprecated support for using Docker over SSH with Anysphere Remote SSH versions 1.0.1 and earlier
- When reopening a folder in a container, the container will reopen in the same window rather than in a new window.

# 1.0.22
- When copying the `.gitconfig`, skip copying the `core.editor`,  `core.sshCommand`, and `gpg.program` settings if the executable cannot be found in the container.
  Only applies to newly built containers.
- Skipping `.gitconfig` copy if `git` is not found within the container

# 1.0.21
- Fixed issues with how `onAutoForward` on the `portsAttributes` and `otherPortsAttributes` were not consistently applied

# 1.0.20
- Fix an issue where the interactive terminal for lifecycle scripts would refocus whenever new output is available
- Fix compatibility when opening containers in WSL that used WSL-style paths without a nested authority
- Support `${localEnv:VAR}` substitutions in attached container configuration files for the `remoteUser` and `remoteEnv` properties

# 1.0.19
- Use an interactive terminal to run Devcontainer Lifecycle Scripts
- Respect the `waitFor` option in the `devcontainer.json` when running lifecycle scripts. The editor will now load after the `waitFor` event completes.
- Add support for attached container configuration files

# 1.0.18
- Fix permissions when copying the server into the Docker container
- Remove a connection check (added in 1.0.16) that caused timeouts and connection failures for otherwise successful connections

# 1.0.17
- Fix an issue when the window failed to open due to `localhost` resolution. Fixed by explicitly binding to both
`127.0.0.1` and `[::1]`.

## 1.0.16
- Fix an issue where opt-out extensions (those beginning with a `-`) in a `devcontainer.json` file were not being excluded
- Support copying symlinked `.gitconfig` files into the remote container
- Apply `portAttributes` and `otherPortAttributes` from the `devcontainer.json` when forwarding ports
- Forward ports for both `IPv4` and `IPv6`, and open ports on `localhost` instead of `127.0.0.1` locally
- Fix an issue where the `remoteEnv` was persisted across rebuilt containers
- Fix connecting to remote containers over SSH when using an explicit `user@host` in the SSH connection string
- Configure the `dotfiles` after copying the `.gitconfig` and configuring the `SSH_AUTH_SOCK`
- Respect the settings `dev.containers.gpuAvailability`, `dev.containers.workspaceMountConsistency`, and `dev.containers.defaultFeatures`

## 1.0.15
- Fix an issue where the `remote.localPortHost` setting was not respected when forwarding ports
- Add `dev.containers.copyGitConfig` setting to control whether the `.gitconfig` file is copied into the remote container
- Improve the reliability of SSH Auth Socket Forwarding

## 1.0.14
- Add support for dotfiles:
  - `dotfiles.repository`: URL of a Git repository containing dotfiles, or owner/name for GitHub repositories
  - `dotfiles.targetPath`: Target path to clone the dotfiles repository (defaults to ~/dotfiles)
  - `dotfiles.installCommand`: Command to run after cloning the dotfiles repository (defaults to finding first of: install.sh, install, bootstrap.sh, bootstrap, setup.sh, setup)
- Fix an issue where the workspace suffix was too long for the window title
- Support `remoteUser` and customizations defined as part of the container image or devcontainer features. (Only applies to newly opened containers)
- On macOS and Linux, check for gitconfig in `~./.config/git/config`, in addition to `.gitconfig`.
- Copy the SSH Known Hosts file into the remote container, if the file exists locally and the container does not already have one
- Add `cursor` and `code` to the `PATH` in the remote container
- Provide schema validation for `devcontainer.json` and `devcontainer-feature.json` files

## 1.0.13
- Fix an issue where `dev.containers.defaultExtensions` was not respected for containers with a `devcontainer.json` file.
- Forward ports that are specified in the `forwardPorts` section of the `devcontainer.json` file
- Add a prompt to reopen a folder in a container if a `devcontainer.json` file is present
- Added support for the devcontainer `shutdownAction`, where the docker containers or the docker compose stack is stopped when the editor window is closed. The `shutdownAction` is applied only for containers running locally or via WSL, not for Docker over SSH.

## 1.0.12
- Fix an issue where SSH auth sock forwarding would drop when reopening a window or reconnecting to a terminal

## 1.0.11
- Fix an issue (introduced in 1.0.9) where connecting to Kubernetes pods required a running Docker server locally

## 1.0.10
- Support the "Attached Containers" scheme

## 1.0.9
- Support passing the `remoteEnv` into the devcontainer
- Improve performance when using Kubernetes over SSH
- Support SSH auth socket forwarding inside Kubernetes pods
- Add `dev.containers.defaultExtensions` setting to allow for default extensions to be installed in all devcontainers
- Fix an issue where, when using Docker or Kubernetes over WSL, forwarded ports were always randomly assigned
- Copy the `.gitconfig` file into the remote container, if the container does not already have one

## 1.0.8
- Fix a bug where custom Docker paths were not passed through when calling the Dev Containers CLI
- Support Dev Containers in WSL (Requires Anysphere Remote WSL 1.0.4+)

## 1.0.7
- Fix an issue where the `Dev Containers` view and `Open Folder in Container` command always used the local docker runtime, even when connected
  to an SSH environment
- When selecting a devcontainer, show the name (if set), in addition to the filepath
- Add support for switching containers when there are multiple devcontainer files in the same workspace

## v1.0.6
- Stream logs for long-running commands
- Support log levels for the "Remote - Dev Containers" output console
- Fix an issue where the Dev Containers view in the Remote Explorer would hang forever if Docker was not installed

## v1.0.5
- Lower resource utilization with Docker port forwarding
- Add commands to rebuild the devcontainer with or without the Docker cache
- Fix an issue where reinstalling the remote server over SSH reloaded the window without actually reinstalling the server
- Fix an issue where the "Attach to Running Container..." command did not show up in the context menu when using the new MS Containers plugin

## v1.0.4
- Fix an issue where the forwarder processes live after containers stop
- Fix an issue where the "Reopen in Container" option didn't allow for devcontainer files in nested directories
- Fix an issue where port forwarding with host networking could cause the port to be occupied when reopening a window

## v1.0.3
- Add remote connection commands to the "Open in Remote Window" selector

## v1.0.2
- Use separate processes to connect to the remote server to reduce the load on the local extension host.
  When using Remote Containers over SSH, requires Anysphere Remote SSH version 1.0.2 or greater.

## v1.0.1
- Fix JSON parse errors with old versions of Docker.

## v1.0.0
- Simplified README

## v0.0.12
- Add support for Alpine linux remote extension hosts. Requires version 0.50.5+ of Cursor.

## v0.0.11

- Install the extensions from the `devcontainer.json`, if specified in `customizations.vscode.extensions`
- Pre-configure the settings from the `devcontainer.json`, if specified in `customizations.vscode.settings`
- Default to the user of the DOCKERFILE or the default user, rather than `root`
- Execute the `postStartCommand` and `postCreateCommand`, as appropriate
- If running locally, avoid copying the `devContainersSpecCLI.js` to a temporary location

## v0.0.10

- Added support for connecting to containers through remote hosts via SSH. Requires `anysphere.remote-ssh` version >= 0.0.27

## v0.0.9

- Added support for port forwarding
- Added support for attaching to pods running in Kubernetes clusters (via `kubectl`)
- Added right-click menu option "Attach Cursor..." inside the Docker and Kubernetes container views


## v0.0.8

- Added prompt to reinstall the server on failed connections
- Added Kill Server and Reload Window Command
- Added Reinstall Server and Reload Window Command
- Added cleanup of old server binaries to after the new server successfully launches

## v0.0.7

- Added telemetry (enabled when privacy mode is disabled)
