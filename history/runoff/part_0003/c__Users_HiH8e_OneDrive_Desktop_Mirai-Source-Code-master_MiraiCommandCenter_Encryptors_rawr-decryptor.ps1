# Rawr Decryptor - AES-256 File Decryption Tool
# Decrypts files encrypted with rawr-encryptor.ps1

param(
  [Parameter(Mandatory = $false)]
  [string]$InputFile,
    
  [Parameter(Mandatory = $false)]
  [string]$OutputFile,
    
  [Parameter(Mandatory = $false)]
  [string]$Password,
    
  [Parameter(Mandatory = $false)]
  [switch]$GUI
)

# AES-256 Decryption Function
function Decrypt-FileAES {
  param(
    [string]$FilePath,
    [string]$OutPath,
    [string]$Key
  )
    
  try {
    # Read encrypted file
    $encryptedBytes = [System.IO.File]::ReadAllBytes($FilePath)
        
    # Generate key and IV from password
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $keyBytes = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Key))
        
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $ivBytes = $md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Key))
        
    # Create AES decryptor
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.KeySize = 256
    $aes.BlockSize = 128
    $aes.Key = $keyBytes
    $aes.IV = $ivBytes
        
    # Decrypt
    $decryptor = $aes.CreateDecryptor()
    $decryptedBytes = $decryptor.TransformFinalBlock($encryptedBytes, 0, $encryptedBytes.Length)
        
    # Write decrypted file
    [System.IO.File]::WriteAllBytes($OutPath, $decryptedBytes)
        
    $aes.Dispose()
        
    return $true
  }
  catch {
    Write-Error "Decryption failed: $_"
    return $false
  }
}

# GUI Mode
if ($GUI) {
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
    
  $form = New-Object System.Windows.Forms.Form
  $form.Text = "Rawr Decryptor - AES-256"
  $form.Size = New-Object System.Drawing.Size(500, 280)
  $form.StartPosition = "CenterScreen"
  $form.FormBorderStyle = "FixedDialog"
  $form.MaximizeBox = $false
    
  # Input file
  $labelInput = New-Object System.Windows.Forms.Label
  $labelInput.Text = "Encrypted File:"
  $labelInput.Location = New-Object System.Drawing.Point(10, 20)
  $labelInput.Size = New-Object System.Drawing.Size(90, 20)
  $form.Controls.Add($labelInput)
    
  $textInput = New-Object System.Windows.Forms.TextBox
  $textInput.Location = New-Object System.Drawing.Point(100, 20)
  $textInput.Size = New-Object System.Drawing.Size(300, 20)
  $form.Controls.Add($textInput)
    
  $btnBrowseInput = New-Object System.Windows.Forms.Button
  $btnBrowseInput.Text = "Browse..."
  $btnBrowseInput.Location = New-Object System.Drawing.Point(410, 18)
  $btnBrowseInput.Size = New-Object System.Drawing.Size(70, 25)
  $btnBrowseInput.Add_Click({
      $openDialog = New-Object System.Windows.Forms.OpenFileDialog
      $openDialog.Filter = "Encrypted Files (*.encrypted)|*.encrypted|All Files (*.*)|*.*"
      if ($openDialog.ShowDialog() -eq "OK") {
        $textInput.Text = $openDialog.FileName
        if ([string]::IsNullOrEmpty($textOutput.Text)) {
          $textOutput.Text = $openDialog.FileName -replace '\.encrypted$', ''
        }
      }
    })
  $form.Controls.Add($btnBrowseInput)
    
  # Output file
  $labelOutput = New-Object System.Windows.Forms.Label
  $labelOutput.Text = "Output File:"
  $labelOutput.Location = New-Object System.Drawing.Point(10, 60)
  $labelOutput.Size = New-Object System.Drawing.Size(80, 20)
  $form.Controls.Add($labelOutput)
    
  $textOutput = New-Object System.Windows.Forms.TextBox
  $textOutput.Location = New-Object System.Drawing.Point(100, 60)
  $textOutput.Size = New-Object System.Drawing.Size(300, 20)
  $form.Controls.Add($textOutput)
    
  $btnBrowseOutput = New-Object System.Windows.Forms.Button
  $btnBrowseOutput.Text = "Browse..."
  $btnBrowseOutput.Location = New-Object System.Drawing.Point(410, 58)
  $btnBrowseOutput.Size = New-Object System.Drawing.Size(70, 25)
  $btnBrowseOutput.Add_Click({
      $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
      $saveDialog.Filter = "All Files (*.*)|*.*"
      if ($saveDialog.ShowDialog() -eq "OK") {
        $textOutput.Text = $saveDialog.FileName
      }
    })
  $form.Controls.Add($btnBrowseOutput)
    
  # Password
  $labelPassword = New-Object System.Windows.Forms.Label
  $labelPassword.Text = "Password:"
  $labelPassword.Location = New-Object System.Drawing.Point(10, 100)
  $labelPassword.Size = New-Object System.Drawing.Size(80, 20)
  $form.Controls.Add($labelPassword)
    
  $textPassword = New-Object System.Windows.Forms.TextBox
  $textPassword.Location = New-Object System.Drawing.Point(100, 100)
  $textPassword.Size = New-Object System.Drawing.Size(300, 20)
  $textPassword.PasswordChar = '*'
  $form.Controls.Add($textPassword)
    
  # Progress
  $labelStatus = New-Object System.Windows.Forms.Label
  $labelStatus.Text = "Ready"
  $labelStatus.Location = New-Object System.Drawing.Point(10, 140)
  $labelStatus.Size = New-Object System.Drawing.Size(470, 20)
  $labelStatus.ForeColor = [System.Drawing.Color]::Green
  $form.Controls.Add($labelStatus)
    
  # Decrypt button
  $btnDecrypt = New-Object System.Windows.Forms.Button
  $btnDecrypt.Text = "🔓 Decrypt"
  $btnDecrypt.Location = New-Object System.Drawing.Point(100, 180)
  $btnDecrypt.Size = New-Object System.Drawing.Size(120, 30)
  $btnDecrypt.BackColor = [System.Drawing.Color]::LightBlue
  $btnDecrypt.Add_Click({
      if ([string]::IsNullOrEmpty($textInput.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Please select encrypted file", "Error")
        return
      }
        
      if ([string]::IsNullOrEmpty($textOutput.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Please select output file", "Error")
        return
      }
        
      if ([string]::IsNullOrEmpty($textPassword.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Please enter password", "Error")
        return
      }
        
      if (-not (Test-Path $textInput.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Encrypted file does not exist", "Error")
        return
      }
        
      $labelStatus.Text = "Decrypting..."
      $labelStatus.ForeColor = [System.Drawing.Color]::Orange
      $form.Refresh()
        
      $result = Decrypt-FileAES -FilePath $textInput.Text -OutPath $textOutput.Text -Key $textPassword.Text
        
      if ($result) {
        $labelStatus.Text = "✅ Decryption successful!"
        $labelStatus.ForeColor = [System.Drawing.Color]::Green
        [System.Windows.Forms.MessageBox]::Show("File decrypted successfully!`n`nOutput: $($textOutput.Text)", "Success", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
      }
      else {
        $labelStatus.Text = "❌ Decryption failed - wrong password?"
        $labelStatus.ForeColor = [System.Drawing.Color]::Red
      }
    })
  $form.Controls.Add($btnDecrypt)
    
  # Close button
  $btnClose = New-Object System.Windows.Forms.Button
  $btnClose.Text = "Close"
  $btnClose.Location = New-Object System.Drawing.Point(280, 180)
  $btnClose.Size = New-Object System.Drawing.Size(120, 30)
  $btnClose.Add_Click({ $form.Close() })
  $form.Controls.Add($btnClose)
    
  $form.ShowDialog() | Out-Null
}
# CLI Mode
else {
  if ([string]::IsNullOrEmpty($InputFile) -or [string]::IsNullOrEmpty($OutputFile) -or [string]::IsNullOrEmpty($Password)) {
    Write-Host "Rawr Decryptor - AES-256 File Decryption"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\rawr-decryptor.ps1 -GUI                                    # Launch GUI"
    Write-Host "  .\rawr-decryptor.ps1 -InputFile <file> -OutputFile <file> -Password <pass>"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\rawr-decryptor.ps1 -GUI"
    Write-Host "  .\rawr-decryptor.ps1 -InputFile bot.encrypted -OutputFile bot.exe -Password MySecretKey"
    exit 1
  }
    
  if (-not (Test-Path $InputFile)) {
    Write-Error "Encrypted file not found: $InputFile"
    exit 1
  }
    
  Write-Host "Decrypting $InputFile..."
  $result = Decrypt-FileAES -FilePath $InputFile -OutPath $OutputFile -Key $Password
    
  if ($result) {
    Write-Host "✅ Decryption successful!" -ForegroundColor Green
    Write-Host "Output: $OutputFile"
  }
  else {
    Write-Host "❌ Decryption failed" -ForegroundColor Red
    exit 1
  }
}
