# Rawr Encryptor - AES-256 File Encryption Tool
# Encrypts files with military-grade AES-256-CBC encryption

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

# AES-256 Encryption Function
function Encrypt-FileAES {
  param(
    [string]$FilePath,
    [string]$OutPath,
    [string]$Key
  )
    
  try {
    # Read file
    $fileBytes = [System.IO.File]::ReadAllBytes($FilePath)
        
    # Generate key and IV from password
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $keyBytes = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Key))
        
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $ivBytes = $md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Key))
        
    # Create AES encryptor
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.KeySize = 256
    $aes.BlockSize = 128
    $aes.Key = $keyBytes
    $aes.IV = $ivBytes
        
    # Encrypt
    $encryptor = $aes.CreateEncryptor()
    $encryptedBytes = $encryptor.TransformFinalBlock($fileBytes, 0, $fileBytes.Length)
        
    # Write encrypted file
    [System.IO.File]::WriteAllBytes($OutPath, $encryptedBytes)
        
    $aes.Dispose()
        
    return $true
  }
  catch {
    Write-Error "Encryption failed: $_"
    return $false
  }
}

# GUI Mode
if ($GUI) {
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
    
  $form = New-Object System.Windows.Forms.Form
  $form.Text = "Rawr Encryptor - AES-256"
  $form.Size = New-Object System.Drawing.Size(500, 300)
  $form.StartPosition = "CenterScreen"
  $form.FormBorderStyle = "FixedDialog"
  $form.MaximizeBox = $false
    
  # Input file
  $labelInput = New-Object System.Windows.Forms.Label
  $labelInput.Text = "Input File:"
  $labelInput.Location = New-Object System.Drawing.Point(10, 20)
  $labelInput.Size = New-Object System.Drawing.Size(80, 20)
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
      $openDialog.Filter = "All Files (*.*)|*.*"
      if ($openDialog.ShowDialog() -eq "OK") {
        $textInput.Text = $openDialog.FileName
        if ([string]::IsNullOrEmpty($textOutput.Text)) {
          $textOutput.Text = $openDialog.FileName + ".encrypted"
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
      $saveDialog.Filter = "Encrypted Files (*.encrypted)|*.encrypted|All Files (*.*)|*.*"
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
    
  # Confirm Password
  $labelConfirm = New-Object System.Windows.Forms.Label
  $labelConfirm.Text = "Confirm:"
  $labelConfirm.Location = New-Object System.Drawing.Point(10, 140)
  $labelConfirm.Size = New-Object System.Drawing.Size(80, 20)
  $form.Controls.Add($labelConfirm)
    
  $textConfirm = New-Object System.Windows.Forms.TextBox
  $textConfirm.Location = New-Object System.Drawing.Point(100, 140)
  $textConfirm.Size = New-Object System.Drawing.Size(300, 20)
  $textConfirm.PasswordChar = '*'
  $form.Controls.Add($textConfirm)
    
  # Progress
  $labelStatus = New-Object System.Windows.Forms.Label
  $labelStatus.Text = "Ready"
  $labelStatus.Location = New-Object System.Drawing.Point(10, 180)
  $labelStatus.Size = New-Object System.Drawing.Size(470, 20)
  $labelStatus.ForeColor = [System.Drawing.Color]::Green
  $form.Controls.Add($labelStatus)
    
  # Encrypt button
  $btnEncrypt = New-Object System.Windows.Forms.Button
  $btnEncrypt.Text = "🔒 Encrypt"
  $btnEncrypt.Location = New-Object System.Drawing.Point(100, 220)
  $btnEncrypt.Size = New-Object System.Drawing.Size(120, 30)
  $btnEncrypt.BackColor = [System.Drawing.Color]::LightGreen
  $btnEncrypt.Add_Click({
      if ([string]::IsNullOrEmpty($textInput.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Please select input file", "Error")
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
        
      if ($textPassword.Text -ne $textConfirm.Text) {
        [System.Windows.Forms.MessageBox]::Show("Passwords do not match", "Error")
        return
      }
        
      if (-not (Test-Path $textInput.Text)) {
        [System.Windows.Forms.MessageBox]::Show("Input file does not exist", "Error")
        return
      }
        
      $labelStatus.Text = "Encrypting..."
      $labelStatus.ForeColor = [System.Drawing.Color]::Orange
      $form.Refresh()
        
      $result = Encrypt-FileAES -FilePath $textInput.Text -OutPath $textOutput.Text -Key $textPassword.Text
        
      if ($result) {
        $labelStatus.Text = "✅ Encryption successful!"
        $labelStatus.ForeColor = [System.Drawing.Color]::Green
        [System.Windows.Forms.MessageBox]::Show("File encrypted successfully!`n`nOutput: $($textOutput.Text)", "Success", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
      }
      else {
        $labelStatus.Text = "❌ Encryption failed"
        $labelStatus.ForeColor = [System.Drawing.Color]::Red
      }
    })
  $form.Controls.Add($btnEncrypt)
    
  # Close button
  $btnClose = New-Object System.Windows.Forms.Button
  $btnClose.Text = "Close"
  $btnClose.Location = New-Object System.Drawing.Point(280, 220)
  $btnClose.Size = New-Object System.Drawing.Size(120, 30)
  $btnClose.Add_Click({ $form.Close() })
  $form.Controls.Add($btnClose)
    
  $form.ShowDialog() | Out-Null
}
# CLI Mode
else {
  if ([string]::IsNullOrEmpty($InputFile) -or [string]::IsNullOrEmpty($OutputFile) -or [string]::IsNullOrEmpty($Password)) {
    Write-Host "Rawr Encryptor - AES-256 File Encryption"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\rawr.ps1 -GUI                                    # Launch GUI"
    Write-Host "  .\rawr.ps1 -InputFile <file> -OutputFile <file> -Password <pass>"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\rawr.ps1 -GUI"
    Write-Host "  .\rawr.ps1 -InputFile bot.exe -OutputFile bot.encrypted -Password MySecretKey"
    exit 1
  }
    
  if (-not (Test-Path $InputFile)) {
    Write-Error "Input file not found: $InputFile"
    exit 1
  }
    
  Write-Host "Encrypting $InputFile..."
  $result = Encrypt-FileAES -FilePath $InputFile -OutPath $OutputFile -Key $Password
    
  if ($result) {
    Write-Host "✅ Encryption successful!" -ForegroundColor Green
    Write-Host "Output: $OutputFile"
  }
  else {
    Write-Host "❌ Encryption failed" -ForegroundColor Red
    exit 1
  }
}
