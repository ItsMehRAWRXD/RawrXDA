<#
WebView2Shim.ps1 - Lightweight WebView2-like shim for headless/CLI testing

This file implements a minimal, safe shim intended as a drop-in compatibility
layer for the RawrXD browser automation code when the Microsoft WebView2
runtime is not available or when running headless/CLI scenarios.

Limitations:
- This shim is NOT a binary replacement for Microsoft's WebView2 runtime.
- JS execution is simulated using HTML parsing (best-effort); not all scripts
  will run. The shim provides only the subset of behaviors RawrXD's
  BrowserAutomation.ps1 expects (Navigate, ExecuteScriptAsync, CapturePreview).
- For GUI playback it uses the system default browser (Start-Process).
#>

Set-StrictMode -Version Latest

function Initialize-WebView2Shim {
  [CmdletBinding(DefaultParameterSetName = 'Default')]
  param(
    [switch]$ForceEnable
  )

  # Global state for shim
  $script:webView2Shim = [PSCustomObject]@{}
  $script:webView2Shim.__currentUrl = $null
  $script:webView2Shim.__lastHtml = $null
  $script:webView2Shim.__available = $true

  # Add script methods to mimic a WebView2-like object
  $wb = New-Object PSObject
  $wb | Add-Member -MemberType ScriptMethod -Name "Navigate" -Value {
    param($url)
    $script:webView2Shim.__currentUrl = $url
    # For GUI mode, open in system browser; otherwise store
    if ($script:GuiMode -or $script:UseWebView2FallbackAsBrowser) {
      Start-Process -FilePath $url -WindowStyle Normal | Out-Null
    }
    # Try to request the page to cache it for ExecuteScriptAsync
    try {
      $response = Invoke-WebRequest -Uri $url -UseBasicParsing -Headers @{ 'User-Agent' = 'RawrXD-WebView2Shim/1.0' } -ErrorAction Stop
      $script:webView2Shim.__lastHtml = $response.Content
    }
    catch {
      Write-Verbose "[WebView2Shim] Navigate: could not fetch $url, $_"
      $script:webView2Shim.__lastHtml = $null
    }
    return $true
  }

  $wb | Add-Member -MemberType ScriptMethod -Name "ExecuteScriptAsync" -Value {
    param([string]$scriptJs)
    # Provide some safe, deterministic emulation for common commands that
    # BrowserAutomation uses (querying title, metadata, simple selectors,
    # or patterns like returning JSON strings).
    if (-not $script:webView2Shim.__lastHtml) {
      return '{"success":false,"error":"no_cached_page"}'
    }
    # Parse as HTML using simple heuristics
    $html = $script:webView2Shim.__lastHtml
    try {
      if ($scriptJs -match 'document.title') {
        if ($html -match '<title>(.*?)</title>') { $value = ([System.Web.HttpUtility]::HtmlDecode($matches[1])) ; return (New-Object PSObject -Property @{ Result = $value }) }
        return (New-Object PSObject -Property @{ Result = '' })
      }

      if ($scriptJs -match 'querySelector\(' -and $scriptJs -match 'innerText') {
        $idx = $scriptJs.IndexOf('querySelector(')
        if ($idx -ge 0) {
          $sub = $scriptJs.Substring($idx)
          $single = $sub.IndexOf("'")
          $double = $sub.IndexOf('"')
          $quoteChar = ''
          $open = -1
          if ($single -ge 0 -and ($double -lt 0 -or $single -lt $double)) { $open = $single; $quoteChar = "'" }
          elseif ($double -ge 0) { $open = $double; $quoteChar = '"' }
          if ($open -ge 0) {
            $close = $sub.IndexOf($quoteChar, $open + 1)
            if ($close -gt $open) {
              $sel = $sub.Substring($open + 1, $close - $open - 1)
              if ($sel -like 'meta*') { if ($html -match '<meta\s+name="(?<name>[^"]+)"\s+content="(?<content>[^"]+)"') { return $matches['content'] } }
              if ($sel.StartsWith('.')) { $class = $sel.TrimStart('.'); if ($html -match ('class=".*?\b' + [regex]::Escape($class) + '\b.*?".*?>(?<inner>.*?)<')) { return ([System.Web.HttpUtility]::HtmlDecode($matches['inner'])) } }
            }
          }
        }
        return ''
      }

      if ($scriptJs -match 'querySelectorAll\(') {
        $results = @()
        foreach ($m in [regex]::Matches($html, '(?:/watch\?v=|data-video-id=|data-id=)([A-Za-z0-9_-]{6,})')) {
          $id = $m.Groups[1].Value
          $urlCandidate = "https://www.youtube.com/watch?v=$id"
          $title = ''
          if ($html -match ('href=.*?' + [regex]::Escape($id) + '.*?title="(?<t>[^"]+)"')) { $title = $matches['t'] }
          $results += @{ id = $id; url = $urlCandidate; title = $title }
        }
        $json = (ConvertTo-Json $results -Compress)
        return (New-Object PSObject -Property @{ Result = $json })
      }

      # Default unsupported script: return failure JSON for the caller to handle
      $j = ('{"success":false,"error":"unsupported_script","script":' + (ConvertTo-Json $scriptJs -Compress) + '}')
      return (New-Object PSObject -Property @{ Result = $j })
    }
    catch {
      $j = ('{"success":false,"error":"exception","message":"' + ($_.Exception.Message -replace '"', '\"') + '"}')
      return (New-Object PSObject -Property @{ Result = $j })
    }
  }

  $wb | Add-Member -MemberType ScriptMethod -Name "CapturePreviewAsync" -Value {
    param([string]$outputPath)
    # If on GUI mode, attempt to use System.Drawing to capture the screen's active window.
    try {
      # Try a minimal screenshot via System.Windows.Forms and Bitmap
      Add-Type -AssemblyName System.Windows.Forms, System.Drawing
      $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
      $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
      $g = [System.Drawing.Graphics]::FromImage($bmp)
      $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
      $bmp.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
      $g.Dispose(); $bmp.Dispose()
      return $true
    }
    catch {
      # Create a 1x1 PNG placeholder if we can't capture
      try {
        $pngBytes = [System.Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGNgYAAAAAMAASsJTYQAAAAASUVORK5CYII=')
        [System.IO.File]::WriteAllBytes($outputPath, $pngBytes)
        return $true
      }
      catch {
        return $false
      }
    }
  }

  $wb | Add-Member -MemberType ScriptMethod -Name "IsAvailable" -Value { return $script:webView2Shim.__available }
  $wb | Add-Member -MemberType ScriptMethod -Name "Close" -Value { $script:webView2Shim.__available = $false; return $true }

  # Create a CoreWebView2-like object for backwards compatibility with existing code that
  # uses $script:webBrowser.CoreWebView2.*. The CoreWebView2 object provides DocumentTitle,
  # Source, ExecuteScriptAsync and minimal event/host object methods.
  $core = New-Object PSObject
  $core | Add-Member -MemberType ScriptMethod -Name 'ExecuteScriptAsync' -Value {
    param($s)
    return $wb.ExecuteScriptAsync($s)
  }
  $core | Add-Member -MemberType ScriptProperty -Name 'DocumentTitle' -Value { if ($script:webView2Shim.__lastHtml -and $script:webView2Shim.__lastHtml -match '<title>(.*?)</title>') { return ([System.Web.HttpUtility]::HtmlDecode($matches[1])) } else { return '' } }
  $core | Add-Member -MemberType ScriptProperty -Name 'Source' -Value { return $script:webView2Shim.__currentUrl }
  $core | Add-Member -MemberType ScriptMethod -Name 'AddHostObjectToScript' -Value { param($name, $obj) Write-Verbose "[WebView2Shim] AddHostObjectToScript called for $name" }
  $core | Add-Member -MemberType ScriptMethod -Name 'Add_NavigationStarting' -Value { param($sb) Write-Verbose '[WebView2Shim] Add_NavigationStarting (no-op)' }
  $core | Add-Member -MemberType ScriptMethod -Name 'Add_NavigationCompleted' -Value { param($sb) Write-Verbose '[WebView2Shim] Add_NavigationCompleted (no-op)' }
  $wb | Add-Member -MemberType NoteProperty -Name 'CoreWebView2' -Value $core

  $script:webView2Shim.WebView = $wb
  return $wb
}

function Enable-WebView2ShimForRawrXD {
  param()
  if (-not (Get-Variable -Name 'webView2Shim' -Scope Script -ErrorAction SilentlyContinue)) {
    Initialize-WebView2Shim -ForceEnable | Out-Null
  }
  # Expose as $script:webBrowser if none exists
  if (-not (Get-Variable -Name 'webBrowser' -Scope Script -ErrorAction SilentlyContinue)) {
    Set-Variable -Name 'webBrowser' -Value $script:webView2Shim.WebView -Scope Script -Force
  }
  return $script:webView2Shim.WebView
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Export-ModuleMember -Function Initialize-WebView2Shim, Enable-WebView2ShimForRawrXD
