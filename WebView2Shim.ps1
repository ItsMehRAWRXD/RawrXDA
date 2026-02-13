<#
WebView2Shim.ps1 - Lightweight WebView2-like shim for headless/CLI testing
(Shadow copy placed in Desktop Powershield folder for RawrXD.ps1 auto-load)

Original source duplicated from development copy to satisfy fallback loader.
Adds minimal browser automation compatibility when real WebView2 is unavailable.
#>

Set-StrictMode -Version Latest

function Initialize-WebView2Shim {
  [CmdletBinding(DefaultParameterSetName = 'Default')]
  param([switch]$ForceEnable)
  $script:webView2Shim = [PSCustomObject]@{}
  $script:webView2Shim.__currentUrl = $null
  $script:webView2Shim.__lastHtml = $null
  $script:webView2Shim.__available = $true
  $wb = New-Object PSObject
  $wb | Add-Member ScriptMethod Navigate { param($url)
    $script:webView2Shim.__currentUrl = $url
    if ($script:GuiMode -or $script:UseWebView2FallbackAsBrowser) {
      Start-Process -FilePath $url -WindowStyle Normal | Out-Null
    }
    try {
      $response = Invoke-WebRequest -Uri $url -UseBasicParsing -Headers @{ 'User-Agent' = 'RawrXD-WebView2Shim/1.0' } -ErrorAction Stop
      $script:webView2Shim.__lastHtml = $response.Content
    } catch {
      Write-Verbose "[WebView2Shim] Navigate fetch failed: $_"
      $script:webView2Shim.__lastHtml = $null
    }
    $true
  }
  $wb | Add-Member ScriptMethod ExecuteScriptAsync { param([string]$scriptJs)
    if (-not $script:webView2Shim.__lastHtml) { return (New-Object PSObject -Property @{ Result = '{"success":false,"error":"no_cached_page"' }) }
    $html = $script:webView2Shim.__lastHtml
    try {
      if ($scriptJs -match 'document.title') {
        if ($html -match '<title>(.*?)</title>') { return (New-Object PSObject -Property @{ Result = ([System.Web.HttpUtility]::HtmlDecode($matches[1])) }) }
        return (New-Object PSObject -Property @{ Result = '' })
      }
      $j = '{"success":false,"error":"unsupported_script"}'
      return (New-Object PSObject -Property @{ Result = $j })
    } catch {
      $j = '{"success":false,"error":"exception"}'
      return (New-Object PSObject -Property @{ Result = $j })
    }
  }
  $wb | Add-Member ScriptMethod CapturePreviewAsync { param([string]$outputPath)
    try {
      Add-Type -AssemblyName System.Windows.Forms, System.Drawing
      $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
      $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
      $g = [System.Drawing.Graphics]::FromImage($bmp)
      $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
      $bmp.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
      $g.Dispose(); $bmp.Dispose(); return $true
    } catch {
      try {
        $pngBytes = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGNgYAAAAAMAASsJTYQAAAAASUVORK5CYII=')
        [IO.File]::WriteAllBytes($outputPath, $pngBytes); return $true
      } catch { return $false }
    }
  }
  $wb | Add-Member ScriptMethod IsAvailable { $script:webView2Shim.__available }
  $wb | Add-Member ScriptMethod Close { $script:webView2Shim.__available = $false; $true }
  $core = New-Object PSObject
  $core | Add-Member ScriptMethod ExecuteScriptAsync { param($s) $wb.ExecuteScriptAsync($s) }
  $core | Add-Member ScriptProperty DocumentTitle { if ($script:webView2Shim.__lastHtml -and $script:webView2Shim.__lastHtml -match '<title>(.*?)</title>'){ ([System.Web.HttpUtility]::HtmlDecode($matches[1])) } else { '' } }
  $core | Add-Member ScriptProperty Source { $script:webView2Shim.__currentUrl }
  $wb | Add-Member NoteProperty CoreWebView2 $core
  $script:webView2Shim.WebView = $wb
  $wb
}
function Enable-WebView2ShimForRawrXD { if (-not (Get-Variable -Name 'webView2Shim' -Scope Script -ErrorAction SilentlyContinue)) { Initialize-WebView2Shim -ForceEnable | Out-Null }
  if (-not (Get-Variable -Name 'webBrowser' -Scope Script -ErrorAction SilentlyContinue)) { Set-Variable -Name 'webBrowser' -Value $script:webView2Shim.WebView -Scope Script -Force }
  $script:webView2Shim.WebView }
# Dot-sourced (no Export-ModuleMember)
