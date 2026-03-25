# UIEnhancements module: theme, pane management, marketplace warmup, chat layout fix
if (-not $script:ThemeCatalog) {
    $script:ThemeCatalog = @{
        Dark = @{ Back = [System.Drawing.Color]::FromArgb(30,30,30); Fore = [System.Drawing.Color]::FromArgb(220,220,220); Accent = [System.Drawing.Color]::FromArgb(60,120,200) }
        Light = @{ Back = [System.Drawing.Color]::FromArgb(245,245,245); Fore = [System.Drawing.Color]::FromArgb(30,30,30); Accent = [System.Drawing.Color]::FromArgb(0,120,215) }
        Stealth = @{ Back = [System.Drawing.Color]::FromArgb(15,15,18); Fore = [System.Drawing.Color]::FromArgb(180,180,185); Accent = [System.Drawing.Color]::FromArgb(90,90,95) }
        HighContrast = @{ Back = [System.Drawing.Color]::Black; Fore = [System.Drawing.Color]::White; Accent = [System.Drawing.Color]::Yellow }
    }
    $script:CurrentTheme = 'Dark'
}
function Apply-Theme { param([string]$Name)
    if (-not $script:ThemeCatalog[$Name]) { if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Theme '$Name' not found" 'WARNING' } ; return }
    $t = $script:ThemeCatalog[$Name]; $script:CurrentTheme = $Name
    $controls = @(); foreach ($n in 'form','editor','explorer','chatTabControl','terminalOutput','devConsole') { $v = Get-Variable -Name $n -Scope Script -ErrorAction SilentlyContinue; if ($v -and $v.Value -is [System.Windows.Forms.Control]) { $controls += $v.Value } }
    foreach ($c in $controls) { try { $c.BackColor = $t.Back; $c.ForeColor = $t.Fore } catch {} }
    try { if ($script:editor) { Set-EditorTextColor -Color $t.Fore } } catch {}
    if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Applied theme $Name" 'INFO' }
}
if (-not $script:PaneRegistry) { $script:PaneRegistry = @{} }
function Register-Pane { param([string]$Key,[System.Windows.Forms.Control]$Control) if ($Control) { $script:PaneRegistry[$Key] = [PSCustomObject]@{ Control=$Control; Visible=$true; LastSize=$Control.Size } } }
function Collapse-Pane { param([string]$Key) $p=$script:PaneRegistry[$Key]; if ($p -and $p.Visible) { $p.LastSize=$p.Control.Size; $p.Control.Hide(); $p.Visible=$false; if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Pane $Key collapsed" 'INFO' } } }
function Restore-Pane { param([string]$Key) $p=$script:PaneRegistry[$Key]; if ($p -and -not $p.Visible) { $p.Control.Show(); if ($p.LastSize) { $p.Control.Size=$p.LastSize }; $p.Visible=$true; if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Pane $Key restored" 'INFO' } } }
function Toggle-Pane { param([string]$Key) $p=$script:PaneRegistry[$Key]; if ($p) { if ($p.Visible) { Collapse-Pane $Key } else { Restore-Pane $Key } } }
if (-not $script:MarketplaceCache) { $script:MarketplaceCache = @() }
function Warmup-Marketplace { if ($script:MarketplaceCache.Count -gt 0) { return }; if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole 'Warming marketplace cache...' 'INFO' }; try { $script:MarketplaceCache = @([PSCustomObject]@{ Id='sample.ext'; Name='Sample Extension'; Source='local'; Installed=$false }); if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Marketplace cache ready ($($script:MarketplaceCache.Count) items)" 'SUCCESS' } } catch { if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Marketplace warmup failed: $($_.Exception.Message)" 'ERROR' } } }
function Fix-ChatLayout { try { $tabs = Get-Variable -Name 'chatTabControl' -Scope Script -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Value; if ($tabs -and $tabs.TabPages.Count -gt 0) { foreach ($tp in $tabs.TabPages) { $boxes = $tp.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] }; foreach ($b in $boxes) { try { $b.ScrollToCaret() } catch {} } } } } catch { if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole "Chat layout fix failed: $($_.Exception.Message)" 'WARNING' } } }
Export-ModuleMember -Function Apply-Theme,Register-Pane,Collapse-Pane,Restore-Pane,Toggle-Pane,Warmup-Marketplace,Fix-ChatLayout
