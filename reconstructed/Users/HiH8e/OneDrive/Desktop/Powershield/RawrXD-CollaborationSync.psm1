# RawrXD-CollaborationSync.psm1
# Real-time collaboration and cloud sync for RawrXD IDE

using namespace System.Net.WebSockets
using namespace System.Threading
using namespace System.Collections.Concurrent
using namespace System.Text.Json
using namespace System.Net.Http

class WebSocketServer {
    [System.Net.WebSockets.WebSocket]$WebSocket
    [ConcurrentQueue[string]]$MessageQueue
    [CancellationTokenSource]$CancellationTokenSource
    [Task]$ReceiveTask
    [Task]$SendTask

    WebSocketServer([System.Net.WebSockets.WebSocket]$webSocket) {
        $this.WebSocket = $webSocket
        $this.MessageQueue = New-Object ConcurrentQueue[string]
        $this.CancellationTokenSource = New-Object CancellationTokenSource
    }

    [void]Start() {
        $this.ReceiveTask = [Task]::Run({ $this.ReceiveMessages() }, $this.CancellationTokenSource.Token)
        $this.SendTask = [Task]::Run({ $this.SendMessages() }, $this.CancellationTokenSource.Token)
    }

    [void]ReceiveMessages() {
        $buffer = New-Object byte[] 4096
        $segment = New-Object ArraySegment[byte] $buffer

        while (-not $this.CancellationTokenSource.Token.IsCancellationRequested) {
            try {
                $result = $this.WebSocket.ReceiveAsync($segment, $this.CancellationTokenSource.Token).Result

                if ($result.MessageType -eq [WebSocketMessageType]::Text) {
                    $message = [Encoding]::UTF8.GetString($buffer, 0, $result.Count)
                    $this.HandleMessage($message)
                }
                elseif ($result.MessageType -eq [WebSocketMessageType]::Close) {
                    break
                }
            }
            catch {
                break
            }
        }
    }

    [void]SendMessages() {
        while (-not $this.CancellationTokenSource.Token.IsCancellationRequested) {
            $message = $null
            if ($this.MessageQueue.TryDequeue([ref]$message)) {
                $buffer = [Encoding]::UTF8.GetBytes($message)
                $segment = New-Object ArraySegment[byte] $buffer
                $this.WebSocket.SendAsync($segment, [WebSocketMessageType]::Text, $true, $this.CancellationTokenSource.Token).Wait()
            }
            else {
                [Thread]::Sleep(10)
            }
        }
    }

    [void]HandleMessage([string]$message) {
        $data = [JsonSerializer]::Deserialize($message, [hashtable])

        switch ($data.type) {
            "cursor" { $this.HandleCursorUpdate($data) }
            "edit" { $this.HandleEdit($data) }
            "presence" { $this.HandlePresence($data) }
            "selection" { $this.HandleSelection($data) }
        }
    }

    [void]HandleCursorUpdate([hashtable]$data) {
        # Update cursor position for user
        # Implementation depends on UI integration
    }

    [void]HandleEdit([hashtable]$data) {
        # Apply operational transform
        # Implementation depends on document model
    }

    [void]HandlePresence([hashtable]$data) {
        # Update user presence
        # Implementation depends on UI integration
    }

    [void]HandleSelection([hashtable]$data) {
        # Update user selection
        # Implementation depends on UI integration
    }

    [void]SendMessage([hashtable]$message) {
        $json = [JsonSerializer]::Serialize($message)
        $this.MessageQueue.Enqueue($json)
    }

    [void]Stop() {
        $this.CancellationTokenSource.Cancel()
        $this.WebSocket.CloseAsync([WebSocketCloseStatus]::NormalClosure, "Server shutdown", [CancellationToken]::None).Wait()
    }
}

class OperationalTransformEngine {
    [List[hashtable]]$Operations
    [int]$ClientVersion
    [ConcurrentDictionary[string, int]]$ClientVersions

    OperationalTransformEngine() {
        $this.Operations = New-Object List[hashtable]
        $this.ClientVersion = 0
        $this.ClientVersions = New-Object ConcurrentDictionary[string, int]
    }

    [hashtable]TransformOperation([hashtable]$operation, [string]$clientId) {
        $clientVersion = $this.ClientVersions.GetOrAdd($clientId, 0)

        # Get operations that happened after client's last version
        $concurrentOps = $this.Operations | Where-Object { $_.version -gt $clientVersion }

        # Transform the operation against concurrent operations
        $transformedOp = $operation
        foreach ($concurrentOp in $concurrentOps) {
            $transformedOp = $this.TransformAgainst($transformedOp, $concurrentOp)
        }

        return $transformedOp
    }

    [hashtable]TransformAgainst([hashtable]$op1, [hashtable]$op2) {
        # Simple operational transform for text operations
        if ($op1.type -eq "insert" -and $op2.type -eq "insert") {
            if ($op1.position -le $op2.position) {
                return $op1
            }
            else {
                return @{
                    type = "insert"
                    position = $op1.position + $op1.text.Length
                    text = $op1.text
                }
            }
        }
        elseif ($op1.type -eq "delete" -and $op2.type -eq "delete") {
            # Handle overlapping deletes
            return $this.TransformDeleteAgainstDelete($op1, $op2)
        }
        elseif ($op1.type -eq "insert" -and $op2.type -eq "delete") {
            return $this.TransformInsertAgainstDelete($op1, $op2)
        }
        elseif ($op1.type -eq "delete" -and $op2.type -eq "insert") {
            return $this.TransformDeleteAgainstInsert($op1, $op2)
        }

        return $op1
    }

    [hashtable]TransformDeleteAgainstDelete([hashtable]$op1, [hashtable]$op2) {
        # Complex delete transformation logic
        $start1 = $op1.position
        $end1 = $start1 + $op1.length
        $start2 = $op2.position
        $end2 = $start2 + $op2.length

        if ($end1 -le $start2) {
            # op1 is completely before op2
            return $op1
        }
        elseif ($end2 -le $start1) {
            # op2 is completely before op1
            return @{
                position = $op1.position - $op2.length
                length = $op1.length
                type = "delete"
            }
        }
        else {
            # Overlapping deletes - merge or split
            $overlapStart = [math]::Max($start1, $start2)
            $overlapEnd = [math]::Min($end1, $end2)

            if ($overlapStart -ge $overlapEnd) {
                # No overlap after adjustment
                return $op1
            }

            # Adjust position and length
            $newStart = [math]::Min($start1, $start2)
            $newLength = ($end1 - $start1) + ($end2 - $start2) - ($overlapEnd - $overlapStart)

            return @{
                position = $newStart
                length = $newLength
                type = "delete"
            }
        }
    }

    [hashtable]TransformInsertAgainstDelete([hashtable]$op1, [hashtable]$op2) {
        $insertPos = $op1.position
        $deleteStart = $op2.position
        $deleteEnd = $deleteStart + $op2.length

        if ($insertPos -le $deleteStart) {
            return $op1
        }
        elseif ($insertPos -ge $deleteEnd) {
            return @{
                position = $insertPos - $op2.length
                text = $op1.text
                type = "insert"
            }
        }
        else {
            # Insert position is within delete range - invalid operation
            throw "Invalid operation: insert within delete range"
        }
    }

    [hashtable]TransformDeleteAgainstInsert([hashtable]$op1, [hashtable]$op2) {
        $deleteStart = $op1.position
        $deleteEnd = $deleteStart + $op1.length
        $insertPos = $op2.position

        if ($insertPos -le $deleteStart) {
            return @{
                position = $deleteStart + $op2.text.Length
                length = $op1.length
                type = "delete"
            }
        }
        elseif ($insertPos -ge $deleteEnd) {
            return $op1
        }
        else {
            # Complex case: insert within delete range
            $beforeLength = $insertPos - $deleteStart
            $afterLength = $deleteEnd - $insertPos

            if ($beforeLength -gt 0) {
                return @{
                    position = $deleteStart
                    length = $beforeLength
                    type = "delete"
                }
            }
            elseif ($afterLength -gt 0) {
                return @{
                    position = $insertPos + $op2.text.Length
                    length = $afterLength
                    type = "delete"
                }
            }
            else {
                # Delete range is entirely within insert - operation becomes no-op
                return $null
            }
        }
    }

    [void]ApplyOperation([hashtable]$operation, [string]$clientId) {
        $transformedOp = $this.TransformOperation($operation, $clientId)

        if ($transformedOp) {
            $transformedOp.version = ++$this.ClientVersion
            $this.Operations.Add($transformedOp)
            $this.ClientVersions[$clientId] = $this.ClientVersion
        }
    }
}

class PresenceManager {
    [ConcurrentDictionary[string, hashtable]]$Users
    [System.Timers.Timer]$HeartbeatTimer

    PresenceManager() {
        $this.Users = New-Object ConcurrentDictionary[string, hashtable]
        $this.HeartbeatTimer = New-Object System.Timers.Timer
        $this.HeartbeatTimer.Interval = 30000  # 30 seconds
        $this.HeartbeatTimer.AutoReset = $true
        $this.HeartbeatTimer.Elapsed += $this.CheckHeartbeats
        $this.HeartbeatTimer.Start()
    }

    [void]UpdatePresence([string]$userId, [hashtable]$presenceData) {
        $presenceData.LastSeen = Get-Date
        $this.Users.AddOrUpdate($userId, $presenceData, { $presenceData })
    }

    [void]RemoveUser([string]$userId) {
        $user = $null
        $this.Users.TryRemove($userId, [ref]$user)
    }

    [array]GetActiveUsers() {
        $now = Get-Date
        $activeUsers = @()

        foreach ($user in $this.Users.GetEnumerator()) {
            $timeSinceLastSeen = $now - $user.Value.LastSeen
            if ($timeSinceLastSeen.TotalSeconds -lt 60) {  # Consider active if seen within 1 minute
                $activeUsers += $user.Value
            }
        }

        return $activeUsers
    }

    [void]CheckHeartbeats([object]$sender, [System.Timers.ElapsedEventArgs]$e) {
        $now = Get-Date
        $inactiveUsers = @()

        foreach ($user in $this.Users.GetEnumerator()) {
            $timeSinceLastSeen = $now - $user.Value.LastSeen
            if ($timeSinceLastSeen.TotalMinutes -gt 5) {  # Remove after 5 minutes
                $inactiveUsers += $user.Key
            }
        }

        foreach ($userId in $inactiveUsers) {
            $this.RemoveUser($userId)
        }
    }
}

class VoiceCommunicationManager {
    # Placeholder for WebRTC voice communication
    [bool]$IsInitialized

    VoiceCommunicationManager() {
        $this.IsInitialized = $false
    }

    [void]Initialize() {
        # Initialize WebRTC components
        # This would require additional libraries in a real implementation
        $this.IsInitialized = $true
    }

    [void]StartCall([string]$peerId) {
        if (-not $this.IsInitialized) {
            $this.Initialize()
        }
        # Implement WebRTC call setup
    }

    [void]EndCall() {
        # Implement call teardown
    }
}

class CloudSyncManager {
    [string]$Provider
    [string]$AccessToken
    [string]$SyncFolder
    [hashtable]$LocalFiles
    [hashtable]$RemoteFiles
    [System.Timers.Timer]$SyncTimer

    CloudSyncManager([string]$provider, [string]$accessToken, [string]$syncFolder) {
        $this.Provider = $provider
        $this.AccessToken = $accessToken
        $this.SyncFolder = $syncFolder
        $this.LocalFiles = @{}
        $this.RemoteFiles = @{}
        $this.SyncTimer = New-Object System.Timers.Timer
        $this.SyncTimer.Interval = 60000  # Sync every minute
        $this.SyncTimer.AutoReset = $true
        $this.SyncTimer.Elapsed += $this.PerformSync
        $this.SyncTimer.Start()
    }

    [void]PerformSync([object]$sender, [System.Timers.ElapsedEventArgs]$e) {
        $this.ScanLocalFiles()
        $this.ScanRemoteFiles()
        $this.ResolveConflicts()
        $this.SyncChanges()
    }

    [void]ScanLocalFiles() {
        $files = Get-ChildItem $this.SyncFolder -Recurse -File
        $this.LocalFiles = @{}

        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($this.SyncFolder.Length + 1)
            $this.LocalFiles[$relativePath] = @{
                LastModified = $file.LastWriteTime
                Size = $file.Length
                Hash = $this.GetFileHash($file.FullName)
            }
        }
    }

    [void]ScanRemoteFiles() {
        switch ($this.Provider) {
            "OneDrive" {
                $this.RemoteFiles = $this.GetOneDriveFiles()
            }
            "GoogleDrive" {
                $this.RemoteFiles = $this.GetGoogleDriveFiles()
            }
            "Dropbox" {
                $this.RemoteFiles = $this.GetDropboxFiles()
            }
        }
    }

    [hashtable]GetOneDriveFiles() {
        $files = @{}
        try {
            $headers = @{ Authorization = "Bearer $($this.AccessToken)" }
            $response = Invoke-RestMethod -Uri "https://graph.microsoft.com/v1.0/me/drive/root/children" -Headers $headers -Method Get

            foreach ($item in $response.value) {
                if (-not $item.folder) {
                    $files[$item.name] = @{
                        LastModified = [DateTime]::Parse($item.lastModifiedDateTime)
                        Size = $item.size
                        Id = $item.id
                    }
                }
            }
        }
        catch {
            Write-Warning "Failed to scan OneDrive files: $_"
        }

        return $files
    }

    [hashtable]GetGoogleDriveFiles() {
        $files = @{}
        try {
            $headers = @{ Authorization = "Bearer $($this.AccessToken)" }
            $response = Invoke-RestMethod -Uri "https://www.googleapis.com/drive/v3/files?pageSize=1000" -Headers $headers -Method Get

            foreach ($file in $response.files) {
                $files[$file.name] = @{
                    LastModified = [DateTime]::Parse($file.modifiedTime)
                    Size = $file.size
                    Id = $file.id
                }
            }
        }
        catch {
            Write-Warning "Failed to scan Google Drive files: $_"
        }

        return $files
    }

    [hashtable]GetDropboxFiles() {
        $files = @{}
        try {
            $headers = @{
                Authorization = "Bearer $($this.AccessToken)"
                "Content-Type" = "application/json"
            }
            $body = @{ path = "" } | ConvertTo-Json
            $response = Invoke-RestMethod -Uri "https://api.dropboxapi.com/2/files/list_folder" -Headers $headers -Method Post -Body $body

            foreach ($entry in $response.entries) {
                if ($entry.'.tag' -eq 'file') {
                    $files[$entry.name] = @{
                        LastModified = [DateTime]::Parse($entry.server_modified)
                        Size = $entry.size
                        Id = $entry.id
                    }
                }
            }
        }
        catch {
            Write-Warning "Failed to scan Dropbox files: $_"
        }

        return $files
    }

    [void]ResolveConflicts() {
        foreach ($fileName in $this.LocalFiles.Keys) {
            if ($this.RemoteFiles.ContainsKey($fileName)) {
                $local = $this.LocalFiles[$fileName]
                $remote = $this.RemoteFiles[$fileName]

                if ($local.Hash -ne $remote.Hash -and
                    [math]::Abs(($local.LastModified - $remote.LastModified).TotalMinutes) -lt 5) {
                    # Conflict detected - keep local version and mark for upload
                    $this.LocalFiles[$fileName].Conflict = $true
                }
            }
        }
    }

    [void]SyncChanges() {
        # Upload new/changed local files
        foreach ($fileName in $this.LocalFiles.Keys) {
            $local = $this.LocalFiles[$fileName]
            $remote = $this.RemoteFiles[$fileName]

            if (-not $remote -or $local.LastModified -gt $remote.LastModified -or $local.Conflict) {
                $this.UploadFile($fileName)
            }
        }

        # Download new/changed remote files
        foreach ($fileName in $this.RemoteFiles.Keys) {
            $remote = $this.RemoteFiles[$fileName]
            $local = $this.LocalFiles[$fileName]

            if (-not $local -or $remote.LastModified -gt $local.LastModified) {
                $this.DownloadFile($fileName)
            }
        }
    }

    [void]UploadFile([string]$fileName) {
        $localPath = Join-Path $this.SyncFolder $fileName

        switch ($this.Provider) {
            "OneDrive" {
                $this.UploadToOneDrive($fileName, $localPath)
            }
            "GoogleDrive" {
                $this.UploadToGoogleDrive($fileName, $localPath)
            }
            "Dropbox" {
                $this.UploadToDropbox($fileName, $localPath)
            }
        }
    }

    [void]DownloadFile([string]$fileName) {
        $localPath = Join-Path $this.SyncFolder $fileName

        switch ($this.Provider) {
            "OneDrive" {
                $this.DownloadFromOneDrive($fileName, $localPath)
            }
            "GoogleDrive" {
                $this.DownloadFromGoogleDrive($fileName, $localPath)
            }
            "Dropbox" {
                $this.DownloadFromDropbox($fileName, $localPath)
            }
        }
    }

    [void]UploadToOneDrive([string]$fileName, [string]$localPath) {
        try {
            $headers = @{ Authorization = "Bearer $($this.AccessToken)" }
            $content = Get-Content $localPath -Raw -Encoding UTF8
            $response = Invoke-RestMethod -Uri "https://graph.microsoft.com/v1.0/me/drive/root:/$($fileName):/content" -Headers $headers -Method Put -Body $content
        }
        catch {
            Write-Warning "Failed to upload to OneDrive: $_"
        }
    }

    [void]DownloadFromOneDrive([string]$fileName, [string]$localPath) {
        try {
            $headers = @{ Authorization = "Bearer $($this.AccessToken)" }
            $response = Invoke-RestMethod -Uri "https://graph.microsoft.com/v1.0/me/drive/root:/$($fileName):/content" -Headers $headers -Method Get
            $response | Out-File $localPath -Encoding UTF8
        }
        catch {
            Write-Warning "Failed to download from OneDrive: $_"
        }
    }

    [void]UploadToGoogleDrive([string]$fileName, [string]$localPath) {
        # Google Drive upload implementation
        # Requires multipart upload for large files
    }

    [void]DownloadFromGoogleDrive([string]$fileName, [string]$localPath) {
        # Google Drive download implementation
    }

    [void]UploadToDropbox([string]$fileName, [string]$localPath) {
        try {
            $headers = @{
                Authorization = "Bearer $($this.AccessToken)"
                "Content-Type" = "application/octet-stream"
                "Dropbox-API-Arg" = "{`"path`": `"/$fileName`",`"mode`": `"overwrite`"}"
            }
            $content = Get-Content $localPath -Raw -Encoding UTF8
            $response = Invoke-RestMethod -Uri "https://content.dropboxapi.com/2/files/upload" -Headers $headers -Method Post -Body $content
        }
        catch {
            Write-Warning "Failed to upload to Dropbox: $_"
        }
    }

    [void]DownloadFromDropbox([string]$fileName, [string]$localPath) {
        try {
            $headers = @{
                Authorization = "Bearer $($this.AccessToken)"
                "Dropbox-API-Arg" = "{`"path`": `"/$fileName`"}"
            }
            $response = Invoke-RestMethod -Uri "https://content.dropboxapi.com/2/files/download" -Headers $headers -Method Post
            $response | Out-File $localPath -Encoding UTF8
        }
        catch {
            Write-Warning "Failed to download from Dropbox: $_"
        }
    }

    [string]GetFileHash([string]$filePath) {
        $hasher = [System.Security.Cryptography.SHA256]::Create()
        $stream = [System.IO.File]::OpenRead($filePath)
        $hash = $hasher.ComputeHash($stream)
        $stream.Close()
        return [BitConverter]::ToString($hash).Replace("-", "").ToLower()
    }
}

class ConflictResolver {
    [hashtable]$Strategies

    ConflictResolver() {
        $this.Strategies = @{
            "KeepLocal" = { param($localPath, $remotePath) Copy-Item $localPath $remotePath -Force }
            "KeepRemote" = { param($localPath, $remotePath) Copy-Item $remotePath $localPath -Force }
            "Merge" = { param($localPath, $remotePath) $this.MergeFiles($localPath, $remotePath) }
            "Prompt" = { param($localPath, $remotePath) $this.PromptUser($localPath, $remotePath) }
        }
    }

    [void]Resolve([string]$localPath, [string]$remotePath, [string]$strategy = "Prompt") {
        $action = $this.Strategies[$strategy]
        if ($action) {
            & $action $localPath $remotePath
        }
    }

    [void]MergeFiles([string]$localPath, [string]$remotePath) {
        # Simple merge strategy - combine files with conflict markers
        $localContent = Get-Content $localPath -Raw
        $remoteContent = Get-Content $remotePath -Raw

        $merged = @"
<<<<<<< LOCAL
$localContent
=======
$remoteContent
>>>>>>> REMOTE
"@

        $merged | Out-File $localPath -Encoding UTF8
    }

    [void]PromptUser([string]$localPath, [string]$remotePath) {
        $choice = [System.Windows.Forms.MessageBox]::Show(
            "Conflict detected for file: $(Split-Path $localPath -Leaf)`n`nChoose resolution:",
            "Sync Conflict",
            [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
            [System.Windows.Forms.MessageBoxIcon]::Question,
            [System.Windows.Forms.MessageBoxDefaultButton]::Button1,
            0,
            "Keep Local",
            "Keep Remote"
        )

        switch ($choice) {
            "Yes" { Copy-Item $localPath $remotePath -Force }
            "No" { Copy-Item $remotePath $localPath -Force }
            "Cancel" { } # Do nothing
        }
    }
}

# Export functions
function New-WebSocketServer {
    param([System.Net.WebSockets.WebSocket]$webSocket)
    return [WebSocketServer]::new($webSocket)
}

function New-OperationalTransformEngine {
    return [OperationalTransformEngine]::new()
}

function New-PresenceManager {
    return [PresenceManager]::new()
}

function New-VoiceCommunicationManager {
    return [VoiceCommunicationManager]::new()
}

function New-CloudSyncManager {
    param([string]$provider, [string]$accessToken, [string]$syncFolder)
    return [CloudSyncManager]::new($provider, $accessToken, $syncFolder)
}

function New-ConflictResolver {
    return [ConflictResolver]::new()
}

Export-ModuleMember -Function * -Variable *