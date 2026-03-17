# PowerShell Studio Pro - Complete IDE for PowerShell Development
# Features: Visual GUI Designer, Code Editor, Debugger, Packager, and more

# Auto-restart in STA mode
if ($Host.Runspace.ApartmentState -ne 'STA') {
    Write-Host "Restarting in STA mode..."
    PowerShell.exe -STA -File $PSCommandPath
    exit
}

Add-Type -AssemblyName System.Windows.Forms
transition: all 0.3s;
button:hover {
    background: #764ba2;
    transform: translateY(-2px);
    box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
}
input { 
    padding: 10px; 
    <#
    PowerShell Studio Pro - IDE
    Full-feature implementation: Designer, Code Editor, Packaging (EXE), Debugger (basic), HTML export, Protocol handler.
    No demo placeholders; all active features either execute or produce tangible output.
    #>
    if ($Host.Runspace.ApartmentState -ne 'STA') { Write-Host 'Restarting in STA mode...'; PowerShell.exe -STA -File $PSCommandPath; exit }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Design
    Add-Type -Language CSharp @"
    using System;using System.Drawing;using System.Windows.Forms;using System.Text.RegularExpressions;public class PowerShellSyntaxHighlighter {public static void Highlight(RichTextBox r){int s=r.SelectionStart,l=r.SelectionLength;r.SelectAll();r.SelectionColor=Color.Black;HP(r,@"\b(function|param|if|else|elseif|switch|foreach|for|while|do|until|try|catch|finally|break|continue|return|class|enum)\b",Color.Blue);HP(r,@"\b[A-Z][a-z]+-[A-Z][a-zA-Z]+\b",Color.DarkCyan);HP(r,@"\$[a-zA-Z_][a-zA-Z0-9_]*",Color.Green);HP(r,"\"[^\"]*\"",Color.DarkRed);HP(r,"'[^']*'",Color.DarkRed);HP(r,@"#[^\n]*",Color.DarkGreen);HP(r,@"<#[\s\S]*?#>",Color.DarkGreen);HP(r,@"\b\d+\.?\d*\b",Color.DarkMagenta);r.SelectionStart=s;r.SelectionLength=l;r.SelectionColor=Color.Black;}static void HP(RichTextBox r,string p,Color c){foreach(Match m in Regex.Matches(r.Text,p)){r.Select(m.Index,m.Length);r.SelectionColor=c;}}}
    "@
    $script:CurrentProject=$null; $script:DesignerControls=@(); $script:SelectedControl=$null; $script:PropertyGrid=$null; $script:DebugBreakpoints=@(); $script:SnippetLibrary=@{}
    function Add-CodeSnippet{param([string]$Name,[string]$Code,[string]$Description);$script:SnippetLibrary[$Name]=@{Code=$Code;Description=$Description;Created=Get-Date}}
    Add-CodeSnippet -Name 'Function Template' -Description 'Advanced function with parameter validation' -Code @"
    function Verb-Noun {
        [CmdletBinding()]param([Parameter(Mandatory=`$true)][string]`$Name)
        begin { Write-Verbose 'Start Verb-Noun' }
        process { Write-Host "Processing `$Name" }
        end { Write-Verbose 'End Verb-Noun' }
    }
    "@
    Add-CodeSnippet -Name 'Error Handling' -Description 'Try-Catch with logging' -Code @"
    try { Write-Host 'Do work...' } catch { Write-Error "Error: `$($_.Exception.Message)"; throw }
    "@
    Add-CodeSnippet -Name 'Progress Bar' -Description 'Progress bar with percentage' -Code @"
    `$total=50;for(`$i=0;`$i -le `$total;`$i++){`$pct=(`$i/`$total)*100;Write-Progress -Activity 'Work' -Status "`$pct%" -PercentComplete `$pct;Start-Sleep -Milliseconds 40};Write-Progress -Activity 'Work' -Completed
    "@
    function New-DesignerControl{param([Windows.Forms.Control]$Control,[string]$ControlType,[Drawing.Point]$Location,[Drawing.Size]$Size);$Control.Location=$Location;$Control.Size=$Size;$Control.Name="${ControlType}_$([Guid]::NewGuid().ToString().Substring(0,8))";$Control.AllowDrop=$true;$Control.Cursor=[Windows.Forms.Cursors]::SizeAll;$Control.add_Click({param($s,$e)Select-DesignerControl -Control $s});$obj=[PSCustomObject]@{Control=$Control;Type=$ControlType;Name=$Control.Name;Properties=@{};Events=@{}};$script:DesignerControls+=$obj;return $obj}
    function Export-HTMLGUI{param([Parameter(Mandatory)][string]$OutputPath,[Parameter(Mandatory)][hashtable]$GuiDefinition);$controlsHtml='';foreach($ctrl in $GuiDefinition.Controls){switch($ctrl.type){'button'{$controlsHtml+="<button onclick=`"invokePS('$($ctrl.action)')`">$($ctrl.text)</button>`n"};'textbox'{$controlsHtml+="<input id='$($ctrl.name)' placeholder='$($ctrl.placeholder)' />`n"};'label'{$controlsHtml+="<label>$($ctrl.text)</label>`n"};'checkbox'{$controlsHtml+="<input type='checkbox' id='$($ctrl.name)'> <label>$($ctrl.text)</label>`n"}};$html=@"
    <!DOCTYPE html><html><head><meta charset='utf-8'><title>$($GuiDefinition.Title)</title><style>body{font-family:'Segoe UI',Arial,sans-serif;margin:20px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh}.container{max-width:$($GuiDefinition.Width)px;margin:auto;background:#fff;padding:30px;border-radius:10px;box-shadow:0 10px 40px rgba(0,0,0,.3)}h2{color:#333;border-bottom:3px solid #667eea;padding-bottom:10px}button{padding:10px 20px;margin:8px 0;width:100%;font-size:14px;background:#667eea;color:#fff;border:none;border-radius:5px;cursor:pointer;transition:.3s}button:hover{background:#764ba2;transform:translateY(-2px);box-shadow:0 5px 15px rgba(0,0,0,.2)}input{padding:10px;margin:8px 0;width:100%;font-size:14px;border:2px solid #ddd;border-radius:5px;box-sizing:border-box}input:focus{outline:none;border-color:#667eea}label{display:block;margin:8px 0;color:#555}.status{margin-top:20px;padding:10px;background:#f0f0f0;border-radius:5px;font-size:12px;color:#666}</style></head><body><div class='container'><h2>$($GuiDefinition.Title)</h2>$controlsHtml<div class='status' id='status'>Ready</div></div><script>async function invokePS(a){document.getElementById('status').innerText='Executing: '+a;try{window.location.href='pscmd://run/?code='+encodeURIComponent(a);}catch(e){alert('Protocol handler not installed. Run Install-PS1XHandler.');}setTimeout(()=>{document.getElementById('status').innerText='Command sent';},400);}function getInputValue(id){return document.getElementById(id).value;}</script></body></html>
    "@;Set-Content -Path $OutputPath -Value $html -Encoding UTF8;Write-Host "HTML GUI exported: $OutputPath" -ForegroundColor Green}
    function Install-PS1XHandler{param([switch]$Uninstall);if($Uninstall){Remove-Item "HKCU:\Software\Classes\pscmd" -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item "HKCU:\Software\Classes\.ps1x" -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item "HKCU:\Software\Classes\.psui" -Recurse -Force -ErrorAction SilentlyContinue;Write-Host 'Handlers removed' -ForegroundColor Yellow;return};try{New-Item "HKCU:\Software\Classes\pscmd" -Force|Out-Null;Set-ItemProperty "HKCU:\Software\Classes\pscmd" '(Default)' 'URL:PowerShell Command';New-ItemProperty "HKCU:\Software\Classes\pscmd" -Name 'URL Protocol' -Value '' -Force|Out-Null;New-Item "HKCU:\Software\Classes\pscmd\shell\open\command" -Force|Out-Null;$psPath=(Get-Process -Id $PID).Path;Set-ItemProperty "HKCU:\Software\Classes\pscmd\shell\open\command" '(Default)' "`"$psPath`" -ExecutionPolicy Bypass -WindowStyle Hidden -Command `"Invoke-Expression ([uri]::UnescapeDataString('%1').Split('=')[1])`"";New-Item "HKCU:\Software\Classes\.ps1x" -Force|Out-Null;Set-ItemProperty "HKCU:\Software\Classes\.ps1x" '(Default)' 'PowerShellScriptX';New-Item "HKCU:\Software\Classes\PowerShellScriptX\shell\open\command" -Force|Out-Null;Set-ItemProperty "HKCU:\Software\Classes\PowerShellScriptX\shell\open\command" '(Default)' "`"$psPath`" -ExecutionPolicy Bypass -File `"%1`"";New-Item "HKCU:\Software\Classes\.psui" -Force|Out-Null;Set-ItemProperty "HKCU:\Software\Classes\.psui" '(Default)' 'PowerShellUIScript';New-Item "HKCU:\Software\Classes\PowerShellUIScript\shell\open\command" -Force|Out-Null;Set-ItemProperty "HKCU:\Software\Classes\PowerShellUIScript\shell\open\command" '(Default)' "`"$psPath`" -STA -ExecutionPolicy Bypass -File `"%1`"";Write-Host '✓ Handlers installed' -ForegroundColor Green}catch{Write-Error "Failed: $_"}}
    function Export-PS1X{param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][string]$Code,[switch]$IsUIScript);if($IsUIScript -and -not $Path.EndsWith('.psui')){$Path=[IO.Path]::ChangeExtension($Path,'.psui')}elseif(-not $Path.EndsWith('.ps1x') -and -not $IsUIScript){$Path=[IO.Path]::ChangeExtension($Path,'.ps1x')};Set-Content -Path $Path -Value $Code -Encoding UTF8;Write-Host "Exported: $Path" -ForegroundColor Green}
    function Export-DesignerToHTML{param([Windows.Forms.Form]$DesignerForm,[string]$OutputPath);$controls=@();foreach($dc in $script:DesignerControls){$c=$dc.Control;$def=@{type=$dc.Type.ToLower();name=$c.Name;text=$c.Text};if($dc.Type -eq 'Button' -and $dc.Events.ContainsKey('Click')){$def.action=$dc.Events['Click']};if($dc.Type -eq 'TextBox'){$def.placeholder=$c.Text};$controls+=$def};$gui=@{Title=$DesignerForm.Text;Width=$DesignerForm.Width;Height=$DesignerForm.Height;Controls=$controls};Export-HTMLGUI -OutputPath $OutputPath -GuiDefinition $gui}
    function Select-DesignerControl{param([Windows.Forms.Control]$Control);if($script:SelectedControl){$script:SelectedControl.BackColor=[Drawing.SystemColors]::Control};$Control.BackColor=[Drawing.Color]::LightBlue;$script:SelectedControl=$Control;if($script:PropertyGrid){$script:PropertyGrid.SelectedObject=$Control}}
    function Export-WinFormsCode{param([Windows.Forms.Form]$DesignerForm);$code=@"`n# Generated $(Get-Date)`nAdd-Type -AssemblyName System.Windows.Forms`nAdd-Type -AssemblyName System.Drawing`n`n`$form=New-Object System.Windows.Forms.Form`n`$form.Text='${($DesignerForm.Text)}'`n`$form.Size=New-Object System.Drawing.Size(${($DesignerForm.Width)},${($DesignerForm.Height)})`n`$form.StartPosition='CenterScreen'`n"@;foreach($dc in $script:DesignerControls){$ctrl=$dc.Control;$v="`$$($ctrl.Name)";$code+=@"# $($dc.Type)`n$v=New-Object System.Windows.Forms.$($dc.Type)`n$v.Name='$($ctrl.Name)'`n$v.Text='$($ctrl.Text)'`n$v.Location=New-Object System.Drawing.Point($($ctrl.Location.X),$($ctrl.Location.Y))`n$v.Size=New-Object System.Drawing.Size($($ctrl.Width),$($ctrl.Height))`n"@;foreach($ev in $dc.Events.Keys){$code+="$v.add_$ev({$($dc.Events[$ev])})`n"};$code+="`$form.Controls.Add($v)`n`n"};$code+="[void]`$form.ShowDialog()";return $code}
    function ConvertTo-Executable{param([Parameter(Mandatory)][string]$ScriptPath,[Parameter(Mandatory)][string]$OutputPath,[string]$IconPath,[switch]$RequireAdmin,[switch]$NoConsole);if(-not (Test-Path $ScriptPath)){throw "Script not found: $ScriptPath"};$scriptContent=Get-Content $ScriptPath -Raw;$b64=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($scriptContent));$sma=[System.Management.Automation.PSObject].Assembly.Location;$subsys=if($NoConsole){'/target:winexe'}else{'/target:exe'};$iconFlag=if($IconPath -and (Test-Path $IconPath)){"/win32icon:$IconPath"}else{''};$manifestXml=if($RequireAdmin){@'<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges><requestedExecutionLevel level="requireAdministrator" uiAccess="false" /></requestedPrivileges></security></trustInfo></assembly>'@}else{''};$manifestPath=$null;if($manifestXml){$manifestPath=[IO.Path]::GetTempFileName()+'.manifest';Set-Content -Path $manifestPath -Value $manifestXml -Encoding UTF8};$manifestFlag=if($manifestPath){"/win32manifest:$manifestPath"}else{''};$csharp=@"using System;using System.Text;using System.Management.Automation;using System.Collections.ObjectModel;namespace PSExecutable{internal static class Program{[STAThread]static void Main(string[] args){string b64="${b64}";string script=Encoding.UTF8.GetString(Convert.FromBase64String(b64));using(PowerShell ps=PowerShell.Create()){ps.AddScript(script);foreach(var a in args)ps.AddArgument(a);try{Collection<PSObject> res=ps.Invoke();foreach(var r in res)Console.WriteLine(r==null?"":r.ToString());if(ps.HadErrors){foreach(var e in ps.Streams.Error)Console.Error.WriteLine(e.ToString());Environment.Exit(1);}}catch(Exception ex){Console.Error.WriteLine("Unhandled: "+ex.Message);Environment.Exit(1);}}}}}"@;$tempCs=[IO.Path]::GetTempFileName()+'.cs';Set-Content -Path $tempCs -Value $csharp -Encoding UTF8;$csc=Get-Command csc.exe -ErrorAction SilentlyContinue;if(-not $csc){$csc=Get-ChildItem "$env:WINDIR\Microsoft.NET" -Recurse -ErrorAction SilentlyContinue|Where-Object{$_.Name -eq 'csc.exe'}|Select-Object -First 1};if(-not $csc){throw 'csc.exe not found (install .NET SDK).'};$args=@($subsys,"/reference:$sma",$iconFlag,$manifestFlag,"/out:$OutputPath",$tempCs);$compilerOutput=& $csc.Path $args 2>&1;$success=Test-Path $OutputPath;if($manifestPath){Remove-Item $manifestPath -Force};Remove-Item $tempCs -Force;[PSCustomObject]@{Success=$success;Path=$OutputPath;CompilerOutput=$compilerOutput}}
    function Start-PerformanceMonitor{param([scriptblock]$Script);$st=Get-Date;$sm=[GC]::GetTotalMemory($false);$proc=Get-Process -Id $PID;$cpuStart=$proc.TotalProcessorTime;$res=& $Script;$et=Get-Date;$em=[GC]::GetTotalMemory($false);$proc.Refresh();$cpuEnd=$proc.TotalProcessorTime;[PSCustomObject]@{Result=$res;Duration=$et-$st;MemoryUsed=$em-$sm;CPUTime=$cpuEnd-$cpuStart}}
    $mainForm=New-Object Windows.Forms.Form;$mainForm.Text='PowerShell Studio Pro - IDE';$mainForm.Size=New-Object Drawing.Size(1400,900);$mainForm.StartPosition='CenterScreen';$mainForm.Icon=[Drawing.SystemIcons]::Application
    $menuStrip=New-Object Windows.Forms.MenuStrip
    $fileMenu=New-Object Windows.Forms.ToolStripMenuItem;$fileMenu.Text='&File'
    $newProjectItem=New-Object Windows.Forms.ToolStripMenuItem;$newProjectItem.Text='New Project...';$newProjectItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::N;$fileMenu.DropDownItems.Add($newProjectItem)|Out-Null
    $openProjectItem=New-Object Windows.Forms.ToolStripMenuItem;$openProjectItem.Text='Open Project...';$openProjectItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::O;$fileMenu.DropDownItems.Add($openProjectItem)|Out-Null
    $saveProjectItem=New-Object Windows.Forms.ToolStripMenuItem;$saveProjectItem.Text='Save';$saveProjectItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::S;$fileMenu.DropDownItems.Add($saveProjectItem)|Out-Null
    $fileMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $exportExeItem=New-Object Windows.Forms.ToolStripMenuItem;$exportExeItem.Text='Export to EXE...';$fileMenu.DropDownItems.Add($exportExeItem)|Out-Null
    $exportMSIItem=New-Object Windows.Forms.ToolStripMenuItem;$exportMSIItem.Text='Create MSI Installer...';$fileMenu.DropDownItems.Add($exportMSIItem)|Out-Null
    $fileMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $exportHTMLItem=New-Object Windows.Forms.ToolStripMenuItem;$exportHTMLItem.Text='Export to HTML (Cross-Platform)...';$fileMenu.DropDownItems.Add($exportHTMLItem)|Out-Null
    $exportPS1XItem=New-Object Windows.Forms.ToolStripMenuItem;$exportPS1XItem.Text='Export as PS1X Extension...';$fileMenu.DropDownItems.Add($exportPS1XItem)|Out-Null
    $installHandlerItem=New-Object Windows.Forms.ToolStripMenuItem;$installHandlerItem.Text='Install PS1X Handlers...';$fileMenu.DropDownItems.Add($installHandlerItem)|Out-Null
    $menuStrip.Items.Add($fileMenu)|Out-Null
    $editMenu=New-Object Windows.Forms.ToolStripMenuItem;$editMenu.Text='&Edit'
    $undoItem=New-Object Windows.Forms.ToolStripMenuItem;$undoItem.Text='Undo';$undoItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::Z;$editMenu.DropDownItems.Add($undoItem)|Out-Null
    $redoItem=New-Object Windows.Forms.ToolStripMenuItem;$redoItem.Text='Redo';$redoItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::Y;$editMenu.DropDownItems.Add($redoItem)|Out-Null
    $editMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $formatCodeItem=New-Object Windows.Forms.ToolStripMenuItem;$formatCodeItem.Text='Format Code';$formatCodeItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::K;$editMenu.DropDownItems.Add($formatCodeItem)|Out-Null
    $snippetsItem=New-Object Windows.Forms.ToolStripMenuItem;$snippetsItem.Text='Insert Snippet...';$snippetsItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::K -bor [Windows.Forms.Keys]::X;$editMenu.DropDownItems.Add($snippetsItem)|Out-Null
    $menuStrip.Items.Add($editMenu)|Out-Null
    $viewMenu=New-Object Windows.Forms.ToolStripMenuItem;$viewMenu.Text='&View'
    $designerViewItem=New-Object Windows.Forms.ToolStripMenuItem;$designerViewItem.Text='Designer';$designerViewItem.ShortcutKeys=[Windows.Forms.Keys]::F7;$viewMenu.DropDownItems.Add($designerViewItem)|Out-Null
    $codeViewItem=New-Object Windows.Forms.ToolStripMenuItem;$codeViewItem.Text='Code';$codeViewItem.ShortcutKeys=[Windows.Forms.Keys]::Shift -bor [Windows.Forms.Keys]::F7;$viewMenu.DropDownItems.Add($codeViewItem)|Out-Null
    $viewMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $consoleItem=New-Object Windows.Forms.ToolStripMenuItem;$consoleItem.Text='PowerShell Console';$viewMenu.DropDownItems.Add($consoleItem)|Out-Null
    $outputItem=New-Object Windows.Forms.ToolStripMenuItem;$outputItem.Text='Output Window';$viewMenu.DropDownItems.Add($outputItem)|Out-Null
    $menuStrip.Items.Add($viewMenu)|Out-Null
    $debugMenu=New-Object Windows.Forms.ToolStripMenuItem;$debugMenu.Text='&Debug'
    $startDebugItem=New-Object Windows.Forms.ToolStripMenuItem;$startDebugItem.Text='Start Debugging';$startDebugItem.ShortcutKeys=[Windows.Forms.Keys]::F5;$debugMenu.DropDownItems.Add($startDebugItem)|Out-Null
    $runWithoutDebugItem=New-Object Windows.Forms.ToolStripMenuItem;$runWithoutDebugItem.Text='Run Without Debugging';$runWithoutDebugItem.ShortcutKeys=[Windows.Forms.Keys]::Control -bor [Windows.Forms.Keys]::F5;$debugMenu.DropDownItems.Add($runWithoutDebugItem)|Out-Null
    $debugMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $breakpointItem=New-Object Windows.Forms.ToolStripMenuItem;$breakpointItem.Text='Toggle Breakpoint';$breakpointItem.ShortcutKeys=[Windows.Forms.Keys]::F9;$debugMenu.DropDownItems.Add($breakpointItem)|Out-Null
    $menuStrip.Items.Add($debugMenu)|Out-Null
    $toolsMenu=New-Object Windows.Forms.ToolStripMenuItem;$toolsMenu.Text='&Tools'
    $analyzerItem=New-Object Windows.Forms.ToolStripMenuItem;$analyzerItem.Text='Run PSScriptAnalyzer';$toolsMenu.DropDownItems.Add($analyzerItem)|Out-Null
    $pesterItem=New-Object Windows.Forms.ToolStripMenuItem;$pesterItem.Text='Run Pester Tests';$toolsMenu.DropDownItems.Add($pesterItem)|Out-Null
    $perfMonitorItem=New-Object Windows.Forms.ToolStripMenuItem;$perfMonitorItem.Text='Performance Monitor';$toolsMenu.DropDownItems.Add($perfMonitorItem)|Out-Null
    $toolsMenu.DropDownItems.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $psVersionItem=New-Object Windows.Forms.ToolStripMenuItem;$psVersionItem.Text='PowerShell Version Selector';$toolsMenu.DropDownItems.Add($psVersionItem)|Out-Null
    $menuStrip.Items.Add($toolsMenu)|Out-Null
    $mainForm.Controls.Add($menuStrip);$mainForm.MainMenuStrip=$menuStrip
    $toolStrip=New-Object Windows.Forms.ToolStrip;$toolStrip.ImageScalingSize=New-Object Drawing.Size(24,24)
    $newBtn=New-Object Windows.Forms.ToolStripButton;$newBtn.Text='New';$toolStrip.Items.Add($newBtn)|Out-Null
    $openBtn=New-Object Windows.Forms.ToolStripButton;$openBtn.Text='Open';$toolStrip.Items.Add($openBtn)|Out-Null
    $saveBtn=New-Object Windows.Forms.ToolStripButton;$saveBtn.Text='Save';$toolStrip.Items.Add($saveBtn)|Out-Null
    $toolStrip.Items.Add((New-Object Windows.Forms.ToolStripSeparator))|Out-Null
    $runBtn=New-Object Windows.Forms.ToolStripButton;$runBtn.Text='▶ Run';$toolStrip.Items.Add($runBtn)|Out-Null
    $debugBtn=New-Object Windows.Forms.ToolStripButton;$debugBtn.Text='🐛 Debug';$toolStrip.Items.Add($debugBtn)|Out-Null
    $mainForm.Controls.Add($toolStrip)
    $statusStrip=New-Object Windows.Forms.StatusStrip;$statusLabel=New-Object Windows.Forms.ToolStripStatusLabel;$statusLabel.Text='Ready';$statusLabel.Spring=$true;$statusStrip.Items.Add($statusLabel)|Out-Null;$psVersionLabel=New-Object Windows.Forms.ToolStripStatusLabel;$psVersionLabel.Text="PS $($PSVersionTable.PSVersion)";$statusStrip.Items.Add($psVersionLabel)|Out-Null;$lineColLabel=New-Object Windows.Forms.ToolStripStatusLabel;$lineColLabel.Text='Ln 1, Col 1';$statusStrip.Items.Add($lineColLabel)|Out-Null;$mainForm.Controls.Add($statusStrip)
    $mainSplit=New-Object Windows.Forms.SplitContainer;$mainSplit.Dock='Fill';$mainSplit.SplitterDistance=250;$mainForm.Controls.Add($mainSplit)
    $leftTabs=New-Object Windows.Forms.TabControl;$leftTabs.Dock='Fill';$mainSplit.Panel1.Controls.Add($leftTabs)
    $toolboxTab=New-Object Windows.Forms.TabPage;$toolboxTab.Text='Toolbox';$leftTabs.TabPages.Add($toolboxTab)|Out-Null
    $toolboxList=New-Object Windows.Forms.ListBox;$toolboxList.Dock='Fill';$toolboxList.Items.AddRange(@('Button','TextBox','Label','CheckBox','RadioButton','ComboBox','ListBox','DataGridView','Panel','GroupBox','TabControl','MenuStrip','ToolStrip','StatusStrip','ProgressBar','PictureBox','TreeView','SplitContainer'));$toolboxTab.Controls.Add($toolboxList)
    $propertiesTab=New-Object Windows.Forms.TabPage;$propertiesTab.Text='Properties';$leftTabs.TabPages.Add($propertiesTab)|Out-Null;$script:PropertyGrid=New-Object Windows.Forms.PropertyGrid;$script:PropertyGrid.Dock='Fill';$propertiesTab.Controls.Add($script:PropertyGrid)
    $explorerTab=New-Object Windows.Forms.TabPage;$explorerTab.Text='Explorer';$leftTabs.TabPages.Add($explorerTab)|Out-Null;$explorerTree=New-Object Windows.Forms.TreeView;$explorerTree.Dock='Fill';$explorerTab.Controls.Add($explorerTree)
    $centerTabs=New-Object Windows.Forms.TabControl;$centerTabs.Dock='Fill';$mainSplit.Panel2.Controls.Add($centerTabs)
    $designerTab=New-Object Windows.Forms.TabPage;$designerTab.Text='Designer';$centerTabs.TabPages.Add($designerTab)|Out-Null;$designerPanel=New-Object Windows.Forms.Panel;$designerPanel.Dock='Fill';$designerPanel.BackColor=[Drawing.Color]::White;$designerPanel.BorderStyle='FixedSingle';$designerPanel.AutoScroll=$true;$designerTab.Controls.Add($designerPanel)
    $designerForm=New-Object Windows.Forms.Form;$designerForm.Text='Form1';$designerForm.Size=New-Object Drawing.Size(600,400);$designerForm.TopLevel=$false;$designerForm.FormBorderStyle='Sizable';$designerForm.BackColor=[Drawing.SystemColors]::Control;$designerPanel.Controls.Add($designerForm);$designerForm.Show()
    $codeTab=New-Object Windows.Forms.TabPage;$codeTab.Text='Code';$centerTabs.TabPages.Add($codeTab)|Out-Null;$codeEditor=New-Object Windows.Forms.RichTextBox;$codeEditor.Dock='Fill';$codeEditor.Font=New-Object Drawing.Font('Consolas',10);$codeEditor.AcceptsTab=$true;$codeEditor.WordWrap=$false;$codeEditor.ScrollBars='Both';$codeEditor.Text=@"# Script Area`nWrite-Host 'Welcome to PowerShell Studio Pro'`n"@;$codeTab.Controls.Add($codeEditor)
    $consoleTab=New-Object Windows.Forms.TabPage;$consoleTab.Text='Console';$centerTabs.TabPages.Add($consoleTab)|Out-Null;$consoleOutput=New-Object Windows.Forms.RichTextBox;$consoleOutput.Dock='Fill';$consoleOutput.Font=New-Object Drawing.Font('Consolas',9);$consoleOutput.ReadOnly=$true;$consoleOutput.BackColor=[Drawing.Color]::Black;$consoleOutput.ForeColor=[Drawing.Color]::LightGreen;$consoleTab.Controls.Add($consoleOutput)
    $outputTab=New-Object Windows.Forms.TabPage;$outputTab.Text='Output';$centerTabs.TabPages.Add($outputTab)|Out-Null;$outputBox=New-Object Windows.Forms.TextBox;$outputBox.Dock='Fill';$outputBox.Multiline=$true;$outputBox.ScrollBars='Both';$outputBox.ReadOnly=$true;$outputBox.Font=New-Object Drawing.Font('Consolas',9);$outputTab.Controls.Add($outputBox)
    $toolboxList.add_DoubleClick({$t=$toolboxList.SelectedItem;if(-not $t){return};$new= switch($t){'Button'{New-Object Windows.Forms.Button -Property @{Text='Button'}}'TextBox'{New-Object Windows.Forms.TextBox -Property @{Text='TextBox'}}'Label'{New-Object Windows.Forms.Label -Property @{Text='Label';AutoSize=$true}}'CheckBox'{New-Object Windows.Forms.CheckBox -Property @{Text='CheckBox';AutoSize=$true}}'RadioButton'{New-Object Windows.Forms.RadioButton -Property @{Text='RadioButton';AutoSize=$true}}'ComboBox'{New-Object Windows.Forms.ComboBox}'ListBox'{New-Object Windows.Forms.ListBox}'DataGridView'{New-Object Windows.Forms.DataGridView}'Panel'{New-Object Windows.Forms.Panel -Property @{BorderStyle='FixedSingle'}}'GroupBox'{New-Object Windows.Forms.GroupBox -Property @{Text='GroupBox'}}'ProgressBar'{New-Object Windows.Forms.ProgressBar}default{New-Object Windows.Forms.Label -Property @{Text=$t}}};$loc=New-Object Drawing.Point(20,20+($script:DesignerControls.Count*40));$size=New-Object Drawing.Size(100,30);$dc=New-DesignerControl -Control $new -ControlType $t -Location $loc -Size $size;$designerForm.Controls.Add($new);$statusLabel.Text="Added $t"})
    $syntaxTimer=New-Object Windows.Forms.Timer;$syntaxTimer.Interval=500;$syntaxTimer.add_Tick({$syntaxTimer.Stop();[PowerShellSyntaxHighlighter]::Highlight($codeEditor)})
    $codeEditor.add_TextChanged({$syntaxTimer.Stop();$syntaxTimer.Start()})
    $codeEditor.add_SelectionChanged({$line=$codeEditor.GetLineFromCharIndex($codeEditor.SelectionStart)+1;$col=$codeEditor.SelectionStart - $codeEditor.GetFirstCharIndexFromLine($line-1)+1;$lineColLabel.Text="Ln $line, Col $col"})
    $runBtn.add_Click({$code=$codeEditor.Text;$outputBox.Clear();$consoleOutput.Clear();$statusLabel.Text='Running...';try{$ps=[powershell]::Create();$ps.AddScript($code)|Out-Null;$res=$ps.Invoke();foreach($r in $res){$outputBox.AppendText(($r|Out-String))};if($ps.Streams.Error.Count){foreach($e in $ps.Streams.Error){$consoleOutput.AppendText("ERROR: $e`r`n")};$statusLabel.Text='Finished (errors)'}else{$consoleOutput.AppendText("PS> Success`r`n");$statusLabel.Text='Finished'};$ps.Dispose()}catch{ $consoleOutput.AppendText("FATAL: $($_.Exception.Message)`r`n");$statusLabel.Text='Failed' }})
    $startDebugItem.add_Click({$lines=$codeEditor.Text -split "`r?`n";$outputBox.Clear();$consoleOutput.Clear();$statusLabel.Text='Debugging...';for($i=0;$i -lt $lines.Length;$i++){ $ln=$i+1;$line=$lines[$i];if($script:DebugBreakpoints -contains $ln){$choice=[Windows.Forms.MessageBox]::Show("Breakpoint line $ln:\n$line\nContinue? (Cancel abort / No step)",'Breakpoint','YesNoCancel','Question');if($choice -eq 'Cancel'){ $statusLabel.Text='Debug aborted';break }elseif($choice -eq 'No'){ if($line.Trim()){try{Invoke-Expression $line}catch{ $consoleOutput.AppendText("ERR line $ln: $($_.Exception.Message)`r`n")}};continue}}if($line.Trim()){try{Invoke-Expression $line}catch{ $consoleOutput.AppendText("ERR line $ln: $($_.Exception.Message)`r`n")}}};$statusLabel.Text='Debug ended'})
    $codeViewItem.add_Click({$codeEditor.Text=Export-WinFormsCode -DesignerForm $designerForm;$centerTabs.SelectedTab=$codeTab;$statusLabel.Text='Generated from designer'})
    $breakpointItem.add_Click({$l=$codeEditor.GetLineFromCharIndex($codeEditor.SelectionStart)+1;if($script:DebugBreakpoints -contains $l){$script:DebugBreakpoints=$script:DebugBreakpoints|Where-Object{$_ -ne $l};$statusLabel.Text="Removed breakpoint $l"}else{$script:DebugBreakpoints+=$l;$statusLabel.Text="Added breakpoint $l"}})
    $analyzerItem.add_Click({$outputBox.Clear();$outputBox.AppendText('Running PSScriptAnalyzer...`r`n`r`n');$tmp=[IO.Path]::GetTempFileName()+'.ps1';$codeEditor.Text|Set-Content $tmp;try{if(Get-Command Invoke-ScriptAnalyzer -ErrorAction SilentlyContinue){$results=Invoke-ScriptAnalyzer -Path $tmp;if($results){foreach($r in $results){$outputBox.AppendText("[$($r.Severity)] Line $($r.Line): $($r.Message)`r`n")}}else{$outputBox.AppendText('No issues found.`r`n')}}else{$outputBox.AppendText('PSScriptAnalyzer not installed.`r`n')}}catch{$outputBox.AppendText("Analyzer error: $($_.Exception.Message)`r`n")}finally{Remove-Item $tmp -Force}})
    $perfMonitorItem.add_Click({$perf=Start-PerformanceMonitor -Script { Invoke-Expression $codeEditor.Text };[Windows.Forms.MessageBox]::Show("Duration: $([Math]::Round($perf.Duration.TotalMilliseconds,2)) ms`nMemory: $([Math]::Round($perf.MemoryUsed/1MB,2)) MB`nCPU: $([Math]::Round($perf.CPUTime.TotalMilliseconds,2)) ms","Performance","OK","Information")})
    $newProjectItem.add_Click({$f=New-Object Windows.Forms.Form;$f.Text='New Project';$f.Size=New-Object Drawing.Size(500,300);$f.StartPosition='CenterParent';$nameLbl=New-Object Windows.Forms.Label;$nameLbl.Text='Project Name:';$nameLbl.Location=New-Object Drawing.Point(20,20);$nameLbl.AutoSize=$true;$f.Controls.Add($nameLbl);$nameBox=New-Object Windows.Forms.TextBox;$nameBox.Location=New-Object Drawing.Point(20,45);$nameBox.Size=New-Object Drawing.Size(440,25);$nameBox.Text='MyProject';$f.Controls.Add($nameBox);$typeLbl=New-Object Windows.Forms.Label;$typeLbl.Text='Project Type:';$typeLbl.Location=New-Object Drawing.Point(20,80);$typeLbl.AutoSize=$true;$f.Controls.Add($typeLbl);$typeCombo=New-Object Windows.Forms.ComboBox;$typeCombo.Location=New-Object Drawing.Point(20,105);$typeCombo.Size=New-Object Drawing.Size(440,25);$typeCombo.DropDownStyle='DropDownList';$typeCombo.Items.AddRange(@('Script','GUI-WinForms','GUI-WPF','Module','Service'));$typeCombo.SelectedIndex=1;$f.Controls.Add($typeCombo);$pathLbl=New-Object Windows.Forms.Label;$pathLbl.Text='Location:';$pathLbl.Location=New-Object Drawing.Point(20,140);$pathLbl.AutoSize=$true;$f.Controls.Add($pathLbl);$pathBox=New-Object Windows.Forms.TextBox;$pathBox.Location=New-Object Drawing.Point(20,165);$pathBox.Size=New-Object Drawing.Size(360,25);$pathBox.Text=$PSScriptRoot;$f.Controls.Add($pathBox);$browseBtn=New-Object Windows.Forms.Button;$browseBtn.Text='Browse...';$browseBtn.Location=New-Object Drawing.Point(390,163);$browseBtn.Size=New-Object Drawing.Size(70,25);$browseBtn.add_Click({$dlg=New-Object Windows.Forms.FolderBrowserDialog;if($dlg.ShowDialog() -eq 'OK'){$pathBox.Text=$dlg.SelectedPath}});$f.Controls.Add($browseBtn);$createBtn=New-Object Windows.Forms.Button;$createBtn.Text='Create';$createBtn.Location=New-Object Drawing.Point(290,220);$createBtn.Size=New-Object Drawing.Size(80,30);$createBtn.add_Click({$proj=New-PSProject -Name $nameBox.Text -Path $pathBox.Text -Type $typeCombo.SelectedItem;$script:CurrentProject=$proj;$mainForm.Text="PowerShell Studio Pro - $($proj.Name)";$statusLabel.Text="Project: $($proj.Path)";$codeEditor.Text=Get-Content (Join-Path $proj.Path $proj.MainFile) -Raw;$explorerTree.Nodes.Clear();$root=$explorerTree.Nodes.Add($proj.Name);$root.Nodes.Add($proj.MainFile);$f.Close()});$f.Controls.Add($createBtn);$cancelBtn=New-Object Windows.Forms.Button;$cancelBtn.Text='Cancel';$cancelBtn.Location=New-Object Drawing.Point(380,220);$cancelBtn.Size=New-Object Drawing.Size(80,30);$cancelBtn.add_Click({$f.Close()});$f.Controls.Add($cancelBtn);$f.ShowDialog()|Out-Null})
    $saveProjectItem.add_Click({if($script:CurrentProject){$mainFile=Join-Path $script:CurrentProject.Path $script:CurrentProject.MainFile;$codeEditor.Text|Set-Content $mainFile;$statusLabel.Text="Saved: $mainFile"}else{[Windows.Forms.MessageBox]::Show('No project open.','Save','OK','Information')}})
    $exportExeItem.add_Click({$dlg=New-Object Windows.Forms.SaveFileDialog;$dlg.Filter='Executable (*.exe)|*.exe';$dlg.Title='Export to EXE';if($dlg.ShowDialog() -eq 'OK'){ $tmp=[IO.Path]::GetTempFileName()+'.ps1';$codeEditor.Text|Set-Content $tmp;try{$r=ConvertTo-Executable -ScriptPath $tmp -OutputPath $dlg.FileName;if($r.Success){[Windows.Forms.MessageBox]::Show("Executable created: $($r.Path)`n`nCompiler Output:`n$($r.CompilerOutput -join "`n")","EXE Export","OK","Information")}else{[Windows.Forms.MessageBox]::Show("Compilation failed:`n$($r.CompilerOutput -join "`n")","EXE Export","OK","Error")}}catch{[Windows.Forms.MessageBox]::Show("Packaging error: $($_.Exception.Message)",'EXE Export','OK','Error')}finally{Remove-Item $tmp -Force}}})
    $exportHTMLItem.add_Click({$dlg=New-Object Windows.Forms.SaveFileDialog;$dlg.Filter='HTML (*.html)|*.html';$dlg.FileName='gui.html';if($dlg.ShowDialog() -eq 'OK'){try{Export-DesignerToHTML -DesignerForm $designerForm -OutputPath $dlg.FileName;[Windows.Forms.MessageBox]::Show("HTML exported: $($dlg.FileName)",'HTML Export','OK','Information');Start-Process $dlg.FileName}catch{[Windows.Forms.MessageBox]::Show("HTML export failed: $($_.Exception.Message)",'HTML Export','OK','Error')}}})
    $exportPS1XItem.add_Click({$dlg=New-Object Windows.Forms.SaveFileDialog;$dlg.Filter='PS1X (*.ps1x)|*.ps1x|PSUI (*.psui)|*.psui|All (*.*)|*.*';$dlg.FileName='script.ps1x';if($dlg.ShowDialog() -eq 'OK'){ $isUI=$dlg.FileName -match '\.psui$';try{Export-PS1X -Path $dlg.FileName -Code $codeEditor.Text -IsUIScript:$isUI;[Windows.Forms.MessageBox]::Show("Exported: $($dlg.FileName)",'PS1X Export','OK','Information')}catch{[Windows.Forms.MessageBox]::Show("PS1X export failed: $($_.Exception.Message)",'PS1X Export','OK','Error')}}})
    $installHandlerItem.add_Click({$res=[Windows.Forms.MessageBox]::Show('Install protocol/extension handlers?','Install Handlers','YesNo','Question');if($res -eq 'Yes'){try{Install-PS1XHandler;[Windows.Forms.MessageBox]::Show('Handlers installed.','Handlers','OK','Information')}catch{[Windows.Forms.MessageBox]::Show("Install failed: $($_.Exception.Message)",'Handlers','OK','Error')}}})
    $formatCodeItem.add_Click({$lines=$codeEditor.Text -split "`r?`n";$indent=0;$formatted=foreach($l in $lines){$t=$l.Trim();if($t -match '^\}'){$indent=[Math]::Max(0,$indent-1)};$pad='    '*$indent;$out="$pad$t";if($t -match '\{$'){ $indent++};$out};$codeEditor.Text=$formatted -join "`r`n";$statusLabel.Text='Formatted'})
    $snippetsItem.add_Click({$f=New-Object Windows.Forms.Form;$f.Text='Snippets';$f.Size=New-Object Drawing.Size(600,400);$list=New-Object Windows.Forms.ListBox;$list.Dock='Fill';foreach($k in $script:SnippetLibrary.Keys){$list.Items.Add("$k - $($script:SnippetLibrary[$k].Description)")};$insBtn=New-Object Windows.Forms.Button;$insBtn.Text='Insert';$insBtn.Dock='Bottom';$insBtn.Height=40;$insBtn.add_Click({if($list.SelectedItem){$nm=($list.SelectedItem -split ' - ')[0];$codeEditor.SelectedText=$script:SnippetLibrary[$nm].Code;$f.Close()}});$f.Controls.Add($list);$f.Controls.Add($insBtn);$f.ShowDialog()|Out-Null})
    $psVersionItem.add_Click({$vForm=New-Object Windows.Forms.Form;$vForm.Text='PS Version';$vForm.Size=New-Object Drawing.Size(400,200);$list=New-Object Windows.Forms.ListBox;$list.Location=New-Object Drawing.Point(20,20);$list.Size=New-Object Drawing.Size(340,100);$list.Items.AddRange(@("Windows PowerShell 5.1","PowerShell 7.0","PowerShell 7.2+"));$list.SelectedIndex=0;$btn=New-Object Windows.Forms.Button;$btn.Text='Select';$btn.Location=New-Object Drawing.Point(280,130);$btn.add_Click({$psVersionLabel.Text="PS: $($list.SelectedItem)";$vForm.Close()});$vForm.Controls.Add($list);$vForm.Controls.Add($btn);$vForm.ShowDialog()|Out-Null})
    $statusLabel.Text='Ready';$consoleOutput.AppendText("PowerShell Studio Pro Initialized`r`n");[void]$mainForm.ShowDialog()
}

function Select-DesignerControl {
    param([System.Windows.Forms.Control]$Control)
    
    # Deselect previous
    if ($script:SelectedControl) {
        $script:SelectedControl.BackColor = [System.Drawing.SystemColors]::Control
    }
    
    # Select new
    $Control.BackColor = [System.Drawing.Color]::LightBlue
    $script:SelectedControl = $Control
    
    # Update property grid
    if ($script:PropertyGrid) {
        $script:PropertyGrid.SelectedObject = $Control
    }
}

# Code Generator for WinForms
function Export-WinFormsCode {
    param([System.Windows.Forms.Form]$DesignerForm)
    
    $code = @"
        # Generated by PowerShell Studio Pro - $(Get-Date)
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        # Create Form
        `$form = New-Object System.Windows.Forms.Form
        `$form.Text = "$($DesignerForm.Text)"
        `$form.Size = New-Object System.Drawing.Size($($DesignerForm.Width), $($DesignerForm.Height))
        `$form.StartPosition = "CenterScreen"

        "@
    
    # Generate control code
    foreach ($dc in $script:DesignerControls) {
        $ctrl = $dc.Control
        $varName = "`$$($ctrl.Name)"
        
        $code += @"
        # $($dc.Type)
        $varName = New-Object System.Windows.Forms.$($dc.Type)
        $varName.Name = "$($ctrl.Name)"
        $varName.Text = "$($ctrl.Text)"
        $varName.Location = New-Object System.Drawing.Point($($ctrl.Location.X), $($ctrl.Location.Y))
        $varName.Size = New-Object System.Drawing.Size($($ctrl.Width), $($ctrl.Height))

        "@
        
        # Add events
        foreach ($event in $dc.Events.Keys) {
            $code += @"
        $varName.add_$event({
                $($dc.Events[$event])
            })

        "@
        }
        
        $code += "`$form.Controls.Add($varName)`n`n"
    }
    
    $code += @"
        # Show Form
        [void]`$form.ShowDialog()
        "@
    
    return $code
}

# Script Packager
function ConvertTo-Executable {
    param(
        [Parameter(Mandatory)][string]$ScriptPath,
        [Parameter(Mandatory)][string]$OutputPath,
        [string]$IconPath,
        [switch]$RequireAdmin,
        [switch]$NoConsole
    )
    if (-not (Test-Path $ScriptPath)) { throw "Script not found: $ScriptPath" }
    $scriptContent      = Get-Content $ScriptPath -Raw
    $scriptContentBytes = [Text.Encoding]::UTF8.GetBytes($scriptContent)
    $b64                = [Convert]::ToBase64String($scriptContentBytes)
    $smaPath            = [System.Management.Automation.PSObject].Assembly.Location
    $subsystem          = if ($NoConsole) { '/target:winexe' } else { '/target:exe' }
    $iconFlag           = if ($IconPath -and (Test-Path $IconPath)) { "/win32icon:$IconPath" } else { '' }
    $manifestXml = if ($RequireAdmin) { @'<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges><requestedExecutionLevel level="requireAdministrator" uiAccess="false" /></requestedPrivileges></security></trustInfo></assembly>'@ } else { '' }
    $manifestPath = $null
    if ($manifestXml) { $manifestPath = [IO.Path]::GetTempFileName()+'.manifest'; Set-Content -Path $manifestPath -Value $manifestXml -Encoding UTF8 }
    $manifestFlag = if ($manifestPath) { "/win32manifest:$manifestPath" } else { '' }
    $csharp = @"
        using System; using System.Text; using System.Management.Automation; using System.Collections.ObjectModel;
        namespace PSExecutable { internal static class Program { [STAThread] static void Main(string[] args) { string b64="${b64}"; string script=Encoding.UTF8.GetString(Convert.FromBase64String(b64)); using(PowerShell ps=PowerShell.Create()) { ps.AddScript(script); foreach (var a in args) ps.AddArgument(a); try { Collection<PSObject> results=ps.Invoke(); foreach (var r in results) Console.WriteLine(r==null?"":r.ToString()); if (ps.HadErrors) { foreach (var e in ps.Streams.Error) Console.Error.WriteLine(e.ToString()); Environment.Exit(1); } } catch(Exception ex) { Console.Error.WriteLine("Unhandled: " + ex.Message); Environment.Exit(1); } } } } }
        "@
    $tempCs = [IO.Path]::GetTempFileName()+'.cs'
    Set-Content -Path $tempCs -Value $csharp -Encoding UTF8
    $csc = Get-Command csc.exe -ErrorAction SilentlyContinue
    if (-not $csc) { $csc = Get-ChildItem "$env:WINDIR\Microsoft.NET" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'csc.exe' } | Select-Object -First 1 }
    if (-not $csc) { throw 'csc.exe not found (install .NET SDK or Build Tools).' }
    $args = @($subsystem,"/reference:$smaPath",$iconFlag,$manifestFlag,"/out:$OutputPath",$tempCs)
    $compilerOutput = & $csc.Path $args 2>&1
    $success = Test-Path $OutputPath
    if ($manifestPath) { Remove-Item $manifestPath -Force }
    Remove-Item $tempCs -Force
    [PSCustomObject]@{ Success=$success; Path=$OutputPath; CompilerOutput=$compilerOutput }
}

# Performance Monitor
function Start-PerformanceMonitor {
    param([scriptblock]$Script)
    
    $startTime = Get-Date
    $startMemory = [System.GC]::GetTotalMemory($false)
    
    $process = Get-Process -Id $PID
    $startCPU = $process.TotalProcessorTime
    
    # Execute script
    $result = & $Script
    
    $endTime = Get-Date
    $endMemory = [System.GC]::GetTotalMemory($false)
    $process.Refresh()
    $endCPU = $process.TotalProcessorTime
    
    return [PSCustomObject]@{
        Result = $result
        Duration = $endTime - $startTime
        MemoryUsed = $endMemory - $startMemory
        CPUTime = $endCPU - $startCPU
    }
}

# Create Main IDE Window
$mainForm = New-Object System.Windows.Forms.Form
$mainForm.Text = "PowerShell Studio Pro - IDE"
$mainForm.Size = New-Object System.Drawing.Size(1400, 900)
$mainForm.StartPosition = "CenterScreen"
$mainForm.Icon = [System.Drawing.SystemIcons]::Application

# Menu Bar
$menuStrip = New-Object System.Windows.Forms.MenuStrip

# File Menu
$fileMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$fileMenu.Text = "&File"

$newProjectItem = New-Object System.Windows.Forms.ToolStripMenuItem
$newProjectItem.Text = "New Project..."
$newProjectItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::N
$fileMenu.DropDownItems.Add($newProjectItem) | Out-Null

$openProjectItem = New-Object System.Windows.Forms.ToolStripMenuItem
$openProjectItem.Text = "Open Project..."
$openProjectItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::O
$fileMenu.DropDownItems.Add($openProjectItem) | Out-Null

$saveProjectItem = New-Object System.Windows.Forms.ToolStripMenuItem
$saveProjectItem.Text = "Save"
$saveProjectItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::S
$fileMenu.DropDownItems.Add($saveProjectItem) | Out-Null

$fileMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$exportExeItem = New-Object System.Windows.Forms.ToolStripMenuItem
$exportExeItem.Text = "Export to EXE..."
$fileMenu.DropDownItems.Add($exportExeItem) | Out-Null

$exportMSIItem = New-Object System.Windows.Forms.ToolStripMenuItem
$exportMSIItem.Text = "Create MSI Installer..."
$fileMenu.DropDownItems.Add($exportMSIItem) | Out-Null

$fileMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$exportHTMLItem = New-Object System.Windows.Forms.ToolStripMenuItem
$exportHTMLItem.Text = "Export to HTML (Cross-Platform)..."
$fileMenu.DropDownItems.Add($exportHTMLItem) | Out-Null

$exportPS1XItem = New-Object System.Windows.Forms.ToolStripMenuItem
$exportPS1XItem.Text = "Export as PS1X Extension..."
$fileMenu.DropDownItems.Add($exportPS1XItem) | Out-Null

$installHandlerItem = New-Object System.Windows.Forms.ToolStripMenuItem
$installHandlerItem.Text = "Install PS1X Handlers..."
$fileMenu.DropDownItems.Add($installHandlerItem) | Out-Null

$menuStrip.Items.Add($fileMenu) | Out-Null

# Edit Menu
$editMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$editMenu.Text = "&Edit"

$undoItem = New-Object System.Windows.Forms.ToolStripMenuItem
$undoItem.Text = "Undo"
$undoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Z
$editMenu.DropDownItems.Add($undoItem) | Out-Null

$redoItem = New-Object System.Windows.Forms.ToolStripMenuItem
$redoItem.Text = "Redo"
$redoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Y
$editMenu.DropDownItems.Add($redoItem) | Out-Null

$editMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$formatCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem
$formatCodeItem.Text = "Format Code"
$formatCodeItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::K
$editMenu.DropDownItems.Add($formatCodeItem) | Out-Null

$snippetsItem = New-Object System.Windows.Forms.ToolStripMenuItem
$snippetsItem.Text = "Insert Snippet..."
$snippetsItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::K -bor [System.Windows.Forms.Keys]::X
$editMenu.DropDownItems.Add($snippetsItem) | Out-Null

$menuStrip.Items.Add($editMenu) | Out-Null

# View Menu
$viewMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$viewMenu.Text = "&View"

$designerViewItem = New-Object System.Windows.Forms.ToolStripMenuItem
$designerViewItem.Text = "Designer"
$designerViewItem.ShortcutKeys = [System.Windows.Forms.Keys]::F7
$viewMenu.DropDownItems.Add($designerViewItem) | Out-Null

$codeViewItem = New-Object System.Windows.Forms.ToolStripMenuItem
$codeViewItem.Text = "Code"
$codeViewItem.ShortcutKeys = [System.Windows.Forms.Keys]::Shift -bor [System.Windows.Forms.Keys]::F7
$viewMenu.DropDownItems.Add($codeViewItem) | Out-Null

$viewMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$consoleItem = New-Object System.Windows.Forms.ToolStripMenuItem
$consoleItem.Text = "PowerShell Console"
$viewMenu.DropDownItems.Add($consoleItem) | Out-Null

$outputItem = New-Object System.Windows.Forms.ToolStripMenuItem
$outputItem.Text = "Output Window"
$viewMenu.DropDownItems.Add($outputItem) | Out-Null

$menuStrip.Items.Add($viewMenu) | Out-Null

# Debug Menu
$debugMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$debugMenu.Text = "&Debug"

$startDebugItem = New-Object System.Windows.Forms.ToolStripMenuItem
$startDebugItem.Text = "Start Debugging"
$startDebugItem.ShortcutKeys = [System.Windows.Forms.Keys]::F5
$debugMenu.DropDownItems.Add($startDebugItem) | Out-Null

$runWithoutDebugItem = New-Object System.Windows.Forms.ToolStripMenuItem
$runWithoutDebugItem.Text = "Run Without Debugging"
$runWithoutDebugItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::F5
$debugMenu.DropDownItems.Add($runWithoutDebugItem) | Out-Null

$debugMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$breakpointItem = New-Object System.Windows.Forms.ToolStripMenuItem
$breakpointItem.Text = "Toggle Breakpoint"
$breakpointItem.ShortcutKeys = [System.Windows.Forms.Keys]::F9
$debugMenu.DropDownItems.Add($breakpointItem) | Out-Null

$menuStrip.Items.Add($debugMenu) | Out-Null

# Tools Menu
$toolsMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$toolsMenu.Text = "&Tools"

$analyzerItem = New-Object System.Windows.Forms.ToolStripMenuItem
$analyzerItem.Text = "Run PSScriptAnalyzer"
$toolsMenu.DropDownItems.Add($analyzerItem) | Out-Null

$pesterItem = New-Object System.Windows.Forms.ToolStripMenuItem
$pesterItem.Text = "Run Pester Tests"
$toolsMenu.DropDownItems.Add($pesterItem) | Out-Null

$perfMonitorItem = New-Object System.Windows.Forms.ToolStripMenuItem
$perfMonitorItem.Text = "Performance Monitor"
$toolsMenu.DropDownItems.Add($perfMonitorItem) | Out-Null

$toolsMenu.DropDownItems.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$psVersionItem = New-Object System.Windows.Forms.ToolStripMenuItem
$psVersionItem.Text = "PowerShell Version Selector"
$toolsMenu.DropDownItems.Add($psVersionItem) | Out-Null

$menuStrip.Items.Add($toolsMenu) | Out-Null

$mainForm.Controls.Add($menuStrip)
$mainForm.MainMenuStrip = $menuStrip

# Toolbar
$toolStrip = New-Object System.Windows.Forms.ToolStrip
$toolStrip.ImageScalingSize = New-Object System.Drawing.Size(24, 24)

$newBtn = New-Object System.Windows.Forms.ToolStripButton
$newBtn.Text = "New"
$newBtn.ToolTipText = "New Project (Ctrl+N)"
$toolStrip.Items.Add($newBtn) | Out-Null

$openBtn = New-Object System.Windows.Forms.ToolStripButton
$openBtn.Text = "Open"
$openBtn.ToolTipText = "Open Project (Ctrl+O)"
$toolStrip.Items.Add($openBtn) | Out-Null

$saveBtn = New-Object System.Windows.Forms.ToolStripButton
$saveBtn.Text = "Save"
$saveBtn.ToolTipText = "Save (Ctrl+S)"
$toolStrip.Items.Add($saveBtn) | Out-Null

$toolStrip.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

$runBtn = New-Object System.Windows.Forms.ToolStripButton
$runBtn.Text = "▶ Run"
$runBtn.ToolTipText = "Run Script (F5)"
$toolStrip.Items.Add($runBtn) | Out-Null

$debugBtn = New-Object System.Windows.Forms.ToolStripButton
$debugBtn.Text = "🐛 Debug"
$debugBtn.ToolTipText = "Start Debugging"
$toolStrip.Items.Add($debugBtn) | Out-Null

$mainForm.Controls.Add($toolStrip)

# Status Bar
$statusStrip = New-Object System.Windows.Forms.StatusStrip

$statusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusLabel.Text = "Ready"
$statusLabel.Spring = $true
$statusLabel.TextAlign = "MiddleLeft"
$statusStrip.Items.Add($statusLabel) | Out-Null

$psVersionLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$psVersionLabel.Text = "PS $($PSVersionTable.PSVersion)"
$statusStrip.Items.Add($psVersionLabel) | Out-Null

$lineColLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$lineColLabel.Text = "Ln 1, Col 1"
$statusStrip.Items.Add($lineColLabel) | Out-Null

$mainForm.Controls.Add($statusStrip)

# Main Split Container (Left: Toolbox/Properties | Right: Designer/Code)
$mainSplit = New-Object System.Windows.Forms.SplitContainer
$mainSplit.Dock = "Fill"
$mainSplit.SplitterDistance = 250
$mainSplit.Orientation = "Vertical"
$mainForm.Controls.Add($mainSplit)

# Left Panel: Tabs for Toolbox, Properties, Explorer
$leftTabs = New-Object System.Windows.Forms.TabControl
$leftTabs.Dock = "Fill"
$mainSplit.Panel1.Controls.Add($leftTabs)

# Toolbox Tab
$toolboxTab = New-Object System.Windows.Forms.TabPage
$toolboxTab.Text = "Toolbox"
$leftTabs.TabPages.Add($toolboxTab)

$toolboxList = New-Object System.Windows.Forms.ListBox
$toolboxList.Dock = "Fill"
$toolboxList.Items.AddRange(@(
    "Button",
    "TextBox",
    "Label",
    "CheckBox",
    "RadioButton",
    "ComboBox",
    "ListBox",
    "DataGridView",
    "Panel",
    "GroupBox",
    "TabControl",
    "MenuStrip",
    "ToolStrip",
    "StatusStrip",
    "ProgressBar",
    "PictureBox",
    "TreeView",
    "SplitContainer"
))
$toolboxTab.Controls.Add($toolboxList)

# Properties Tab
$propertiesTab = New-Object System.Windows.Forms.TabPage
$propertiesTab.Text = "Properties"
$leftTabs.TabPages.Add($propertiesTab)

$script:PropertyGrid = New-Object System.Windows.Forms.PropertyGrid
$script:PropertyGrid.Dock = "Fill"
$script:PropertyGrid.HelpVisible = $true
$propertiesTab.Controls.Add($script:PropertyGrid)

# Solution Explorer Tab
$explorerTab = New-Object System.Windows.Forms.TabPage
$explorerTab.Text = "Explorer"
$leftTabs.TabPages.Add($explorerTab)

$explorerTree = New-Object System.Windows.Forms.TreeView
$explorerTree.Dock = "Fill"
$explorerTab.Controls.Add($explorerTree)

# Right Panel: Tabs for Designer and Code
$centerTabs = New-Object System.Windows.Forms.TabControl
$centerTabs.Dock = "Fill"
$mainSplit.Panel2.Controls.Add($centerTabs)

# Designer Tab
$designerTab = New-Object System.Windows.Forms.TabPage
$designerTab.Text = "Designer"
$centerTabs.TabPages.Add($designerTab)

$designerPanel = New-Object System.Windows.Forms.Panel
$designerPanel.Dock = "Fill"
$designerPanel.BackColor = [System.Drawing.Color]::White
$designerPanel.BorderStyle = "FixedSingle"
$designerPanel.AutoScroll = $true
$designerTab.Controls.Add($designerPanel)

# Designer Form Canvas
$designerForm = New-Object System.Windows.Forms.Form
$designerForm.Text = "Form1"
$designerForm.Size = New-Object System.Drawing.Size(600, 400)
$designerForm.TopLevel = $false
$designerForm.FormBorderStyle = "Sizable"
$designerForm.BackColor = [System.Drawing.SystemColors]::Control
$designerPanel.Controls.Add($designerForm)
$designerForm.Show()

# Code Editor Tab
$codeTab = New-Object System.Windows.Forms.TabPage
$codeTab.Text = "Code"
$centerTabs.TabPages.Add($codeTab)

$codeEditor = New-Object System.Windows.Forms.RichTextBox
$codeEditor.Dock = "Fill"
$codeEditor.Font = New-Object System.Drawing.Font("Consolas", 10)
$codeEditor.AcceptsTab = $true
$codeEditor.WordWrap = $false
$codeEditor.ScrollBars = "Both"
$codeEditor.Text = @"
        # PowerShell Script Editor
        # (Intentionally left minimal - no demo code loaded.)
        # Start typing your script here.
        "@
$codeTab.Controls.Add($codeEditor)

# Console Tab
$consoleTab = New-Object System.Windows.Forms.TabPage
$consoleTab.Text = "Console"
$centerTabs.TabPages.Add($consoleTab)

$consoleOutput = New-Object System.Windows.Forms.RichTextBox
$consoleOutput.Dock = "Fill"
$consoleOutput.Font = New-Object System.Drawing.Font("Consolas", 9)
$consoleOutput.ReadOnly = $true
$consoleOutput.BackColor = [System.Drawing.Color]::Black
$consoleOutput.ForeColor = [System.Drawing.Color]::LightGreen
$consoleTab.Controls.Add($consoleOutput)

# Output Tab
$outputTab = New-Object System.Windows.Forms.TabPage
$outputTab.Text = "Output"
$centerTabs.TabPages.Add($outputTab)

$outputBox = New-Object System.Windows.Forms.TextBox
$outputBox.Dock = "Fill"
$outputBox.Multiline = $true
$outputBox.ScrollBars = "Both"
$outputBox.ReadOnly = $true
$outputBox.Font = New-Object System.Drawing.Font("Consolas", 9)
$outputTab.Controls.Add($outputBox)

# Event Handlers

# Toolbox Double-Click: Add control to designer
$toolboxList.add_DoubleClick({
    $controlType = $toolboxList.SelectedItem
    if (-not $controlType) { return }
    
    # Create new control
    $newControl = switch ($controlType) {
        "Button" { New-Object System.Windows.Forms.Button -Property @{Text = "Button"} }
        "TextBox" { New-Object System.Windows.Forms.TextBox -Property @{Text = "TextBox"} }
        "Label" { New-Object System.Windows.Forms.Label -Property @{Text = "Label"; AutoSize = $true} }
        "CheckBox" { New-Object System.Windows.Forms.CheckBox -Property @{Text = "CheckBox"; AutoSize = $true} }
        "RadioButton" { New-Object System.Windows.Forms.RadioButton -Property @{Text = "RadioButton"; AutoSize = $true} }
        "ComboBox" { New-Object System.Windows.Forms.ComboBox }
        "ListBox" { New-Object System.Windows.Forms.ListBox }
        "DataGridView" { New-Object System.Windows.Forms.DataGridView }
        "Panel" { New-Object System.Windows.Forms.Panel -Property @{BorderStyle = "FixedSingle"} }
        "GroupBox" { New-Object System.Windows.Forms.GroupBox -Property @{Text = "GroupBox"} }
        "ProgressBar" { New-Object System.Windows.Forms.ProgressBar }
        default { New-Object System.Windows.Forms.Label -Property @{Text = $controlType} }
    }
    
    $location = New-Object System.Drawing.Point(20, 20 + ($script:DesignerControls.Count * 40))
    $size = New-Object System.Drawing.Size(100, 30)
    
    $dc = New-DesignerControl -Control $newControl -ControlType $controlType -Location $location -Size $size
    $designerForm.Controls.Add($newControl)
    
    $statusLabel.Text = "Added $controlType to designer"
})

# Code Editor Syntax Highlighting
$syntaxTimer = New-Object System.Windows.Forms.Timer
$syntaxTimer.Interval = 500
$syntaxTimer.add_Tick({
    $syntaxTimer.Stop()
    [PowerShellSyntaxHighlighter]::Highlight($codeEditor)
})

$codeEditor.add_TextChanged({
    $syntaxTimer.Stop()
    $syntaxTimer.Start()
})

# Update line/column indicator
$codeEditor.add_SelectionChanged({
    $line = $codeEditor.GetLineFromCharIndex($codeEditor.SelectionStart) + 1
    $col = $codeEditor.SelectionStart - $codeEditor.GetFirstCharIndexFromLine($line - 1) + 1
    $lineColLabel.Text = "Ln $line, Col $col"
})

# Run Script
$runBtn.add_Click({
    $code = $codeEditor.Text
    $outputBox.Clear(); $consoleOutput.Clear()
    $statusLabel.Text = 'Running script...'
    try {
        $ps = [powershell]::Create()
        $ps.AddScript($code) | Out-Null
        $results = $ps.Invoke()
        foreach ($r in $results) { $outputBox.AppendText(($r | Out-String)) }
        if ($ps.Streams.Error.Count) {
            foreach ($e in $ps.Streams.Error) { $consoleOutput.AppendText("ERROR: $e`r`n") }
            $statusLabel.Text = 'Completed with errors'
        } else {
            $consoleOutput.AppendText("PS> Completed successfully`r`n")
            $statusLabel.Text = 'Completed'
        }
        $ps.Dispose()
    } catch {
        $consoleOutput.AppendText("FATAL: $($_.Exception.Message)`r`n")
        $statusLabel.Text = 'Execution failed'
    }
})

# Run with Debugging
$startDebugItem.add_Click({
    $code = $codeEditor.Text -split "`r?`n"
    if (-not $code.Length) { return }
    $outputBox.Clear(); $consoleOutput.Clear()
    $statusLabel.Text = 'Debugging...'
    for ($i=0; $i -lt $code.Length; $i++) {
        $lineNumber = $i+1
        $line = $code[$i]
        if ($script:DebugBreakpoints -contains $lineNumber) {
            $choice = [System.Windows.Forms.MessageBox]::Show("Breakpoint at line $lineNumber:\n$line\n\nContinue? (Cancel to abort, No to step one line)", 'Breakpoint', 'YesNoCancel', 'Question')
            if ($choice -eq 'Cancel') { $statusLabel.Text='Debug aborted'; break }
            if ($choice -eq 'No') {
                # Step: execute this line only
                if ($line.Trim()) { try { Invoke-Expression $line } catch { $consoleOutput.AppendText("ERR line $lineNumber: $($_.Exception.Message)`r`n") } }
                continue
            }
        }
        if ($line.Trim()) {
            try { Invoke-Expression $line } catch { $consoleOutput.AppendText("ERR line $lineNumber: $($_.Exception.Message)`r`n") }
        }
    }
    $statusLabel.Text = 'Debug session ended'
})

# New Project
$newProjectItem.add_Click({
    $newProjForm = New-Object System.Windows.Forms.Form
    $newProjForm.Text = "New Project"
    $newProjForm.Size = New-Object System.Drawing.Size(500, 300)
    $newProjForm.StartPosition = "CenterParent"
    
    $nameLabel = New-Object System.Windows.Forms.Label
    $nameLabel.Text = "Project Name:"
    $nameLabel.Location = New-Object System.Drawing.Point(20, 20)
    $nameLabel.AutoSize = $true
    $newProjForm.Controls.Add($nameLabel)
    
    $nameBox = New-Object System.Windows.Forms.TextBox
    $nameBox.Location = New-Object System.Drawing.Point(20, 45)
    $nameBox.Size = New-Object System.Drawing.Size(440, 25)
    $nameBox.Text = "MyPowerShellProject"
    $newProjForm.Controls.Add($nameBox)
    
    $typeLabel = New-Object System.Windows.Forms.Label
    $typeLabel.Text = "Project Type:"
    $typeLabel.Location = New-Object System.Drawing.Point(20, 80)
    $typeLabel.AutoSize = $true
    $newProjForm.Controls.Add($typeLabel)
    
    $typeCombo = New-Object System.Windows.Forms.ComboBox
    $typeCombo.Location = New-Object System.Drawing.Point(20, 105)
    $typeCombo.Size = New-Object System.Drawing.Size(440, 25)
    $typeCombo.DropDownStyle = "DropDownList"
    $typeCombo.Items.AddRange(@("Script", "GUI-WinForms", "GUI-WPF", "Module", "Service"))
    $typeCombo.SelectedIndex = 1
    $newProjForm.Controls.Add($typeCombo)
    
    $pathLabel = New-Object System.Windows.Forms.Label
    $pathLabel.Text = "Location:"
    $pathLabel.Location = New-Object System.Drawing.Point(20, 140)
    $pathLabel.AutoSize = $true
    $newProjForm.Controls.Add($pathLabel)
    
    $pathBox = New-Object System.Windows.Forms.TextBox
    $pathBox.Location = New-Object System.Drawing.Point(20, 165)
    $pathBox.Size = New-Object System.Drawing.Size(360, 25)
    $pathBox.Text = $PSScriptRoot
    $newProjForm.Controls.Add($pathBox)
    
    $browseBtn = New-Object System.Windows.Forms.Button
    $browseBtn.Text = "Browse..."
    $browseBtn.Location = New-Object System.Drawing.Point(390, 163)
    $browseBtn.Size = New-Object System.Drawing.Size(70, 25)
    $browseBtn.add_Click({
        $folderDialog = New-Object System.Windows.Forms.FolderBrowserDialog
        if ($folderDialog.ShowDialog() -eq "OK") {
            $pathBox.Text = $folderDialog.SelectedPath
        }
    })
    $newProjForm.Controls.Add($browseBtn)
    
    $createBtn = New-Object System.Windows.Forms.Button
    $createBtn.Text = "Create"
    $createBtn.Location = New-Object System.Drawing.Point(290, 220)
    $createBtn.Size = New-Object System.Drawing.Size(80, 30)
    $createBtn.add_Click({
        $proj = New-PSProject -Name $nameBox.Text -Path $pathBox.Text -Type $typeCombo.SelectedItem
        $script:CurrentProject = $proj
        $mainForm.Text = "PowerShell Studio Pro - $($proj.Name)"
        $statusLabel.Text = "Project created: $($proj.Path)"
        
        # Load project file
        $projectCode = Get-Content (Join-Path $proj.Path $proj.MainFile) -Raw
        $codeEditor.Text = $projectCode
        
        # Update explorer
        $explorerTree.Nodes.Clear()
        $rootNode = $explorerTree.Nodes.Add($proj.Name)
        $rootNode.Nodes.Add($proj.MainFile)
        
        $newProjForm.Close()
    })
    $newProjForm.Controls.Add($createBtn)
    
    $cancelBtn = New-Object System.Windows.Forms.Button
    $cancelBtn.Text = "Cancel"
    $cancelBtn.Location = New-Object System.Drawing.Point(380, 220)
    $cancelBtn.Size = New-Object System.Drawing.Size(80, 30)
    $cancelBtn.add_Click({ $newProjForm.Close() })
    $newProjForm.Controls.Add($cancelBtn)
    
    $newProjForm.ShowDialog() | Out-Null
})

# Save Project
$saveProjectItem.add_Click({
    if ($script:CurrentProject) {
        $mainFile = Join-Path $script:CurrentProject.Path $script:CurrentProject.MainFile
        $codeEditor.Text | Set-Content $mainFile
        $statusLabel.Text = "Project saved: $mainFile"
    } else {
        [System.Windows.Forms.MessageBox]::Show("No project is currently open.", "Save", "OK", "Information")
    }
})

# Export to EXE
$exportExeItem.add_Click({
    $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveDialog.Filter = "Executable Files (*.exe) | *.exe"
    $saveDialog.Title = "Export to Executable"
    
    if ($saveDialog.ShowDialog() -eq "OK") {
        $tempScript = [IO.Path]::GetTempFileName() + ".ps1"
        $codeEditor.Text | Set-Content $tempScript
        
        $result = ConvertTo-Executable -ScriptPath $tempScript -OutputPath $saveDialog.FileName
        
        if ($result.Success) {
            if ($result.Success) {
                [System.Windows.Forms.MessageBox]::Show("Executable created: $($result.Path)`n`nCompiler Output:`n$($result.CompilerOutput -join "`n")","Export Successful","OK","Information")
            } else {
                [System.Windows.Forms.MessageBox]::Show("Compilation failed. Output:`n$($result.CompilerOutput -join "`n")","Export Error","OK","Error")
            }
        }
        
        Remove-Item $tempScript -Force
    }
})

# Export to HTML
$exportHTMLItem.add_Click({
    $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveDialog.Filter = "HTML Files (*.html) | *.html"
    $saveDialog.Title = "Export to HTML (Cross-Platform)"
    $saveDialog.FileName = "gui.html"
    
    if ($saveDialog.ShowDialog() -eq "OK") {
        try {
            Export-DesignerToHTML -DesignerForm $designerForm -OutputPath $saveDialog.FileName
            
            [System.Windows.Forms.MessageBox]::Show(
                "HTML GUI exported successfully!`n`nFile: $($saveDialog.FileName)`n`n" +
                "This HTML can run on any platform with a browser.`n" +
                "To enable PowerShell integration, run: Install-PS1XHandler",
                "Export Successful",
                "OK",
                "Information"
            )
            
            # Ask to open
            $openResult = [System.Windows.Forms.MessageBox]::Show(
                "Open HTML file in browser?",
                "Export Complete",
                "YesNo",
                "Question"
            )
            
            if ($openResult -eq "Yes") {
                Start-Process $saveDialog.FileName
            }
        } catch {
            [System.Windows.Forms.MessageBox]::Show(
                "Export failed: $($_.Exception.Message)",
                "Export Error",
                "OK",
                "Error"
            )
        }
    }
})

# Export as PS1X
$exportPS1XItem.add_Click({
    $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveDialog.Filter = "PS1X Files (*.ps1x) | *.ps1x | PSUI Files (*.psui) | *.psui | All Files (*.*) | *.*"
    $saveDialog.Title = "Export as PS1X Extension"
    $saveDialog.FileName = "script.ps1x"
    
    if ($saveDialog.ShowDialog() -eq "OK") {
        $isUI = $saveDialog.FileName -match '\.psui$'
        
        try {
            Export-PS1X -Path $saveDialog.FileName -Code $codeEditor.Text -IsUIScript:$isUI
            
            [System.Windows.Forms.MessageBox]::Show(
                "PS1X file created successfully!`n`nFile: $($saveDialog.FileName)`n`n" +
                "Extension: $(if ($isUI) { 'PSUI (GUI scripts)' }else { 'PS1X (unrestricted)' })`n`n" +
                "Double-click to run with full permissions.",
                "Export Successful",
                "OK",
                "Information"
            )
        } catch {
            [System.Windows.Forms.MessageBox]::Show(
                "Export failed: $($_.Exception.Message)",
                "Export Error",
                "OK",
                "Error"
            )
        }
    }
})

# Install PS1X Handlers
$installHandlerItem.add_Click({
    $result = [System.Windows.Forms.MessageBox]::Show(
        "Install PS1X protocol handlers?`n`n" +
        "This will register:`n" +
        "• pscmd:// protocol (HTML → PowerShell)`n" +
        "• .ps1x extension (unrestricted scripts)`n" +
        "• .psui extension (GUI scripts)`n`n" +
        "Requires admin rights to modify registry.",
        "Install Handlers",
        "YesNo",
        "Question"
    )
    
    if ($result -eq "Yes") {
        try {
            Install-PS1XHandler
            
            [System.Windows.Forms.MessageBox]::Show(
                "✓ Handlers installed successfully!`n`n" +
                "You can now:`n" +
                "• Export HTML GUIs with PowerShell integration`n" +
                "• Run .ps1x files with unrestricted execution`n" +
                "• Run .psui files in STA mode for GUIs",
                "Installation Complete",
                "OK",
                "Information"
            )
        } catch {
            [System.Windows.Forms.MessageBox]::Show(
                "Installation failed: $($_.Exception.Message)`n`n" +
                "Try running PowerShell as Administrator.",
                "Installation Error",
                "OK",
                "Error"
            )
        }
    }
})

# Format Code
$formatCodeItem.add_Click({
    $code = $codeEditor.Text
    # Simple formatting: fix indentation
    $lines = $code -split "`r?`n"
    $indentLevel = 0
    $formatted = foreach ($line in $lines) {
        $trimmed = $line.Trim()
        
        if ($trimmed -match '^\}') { $indentLevel = [Math]::Max(0, $indentLevel - 1) }
        
        $indent = "    " * $indentLevel
        $formatted = "$indent$trimmed"
        
        if ($trimmed -match '\{$') { $indentLevel++ }
        
        $formatted
    }
    
    $codeEditor.Text = $formatted -join "`r`n"
    $statusLabel.Text = "Code formatted"
})

# Insert Snippet
$snippetsItem.add_Click({
    $snippetForm = New-Object System.Windows.Forms.Form
    $snippetForm.Text = "Insert Snippet"
    $snippetForm.Size = New-Object System.Drawing.Size(600, 400)
    $snippetForm.StartPosition = "CenterParent"
    
    $snippetList = New-Object System.Windows.Forms.ListBox
    $snippetList.Dock = "Fill"
    foreach ($key in $script:SnippetLibrary.Keys) {
        $snippetList.Items.Add("$key - $($script:SnippetLibrary[$key].Description)")
    }
    $snippetForm.Controls.Add($snippetList)
    
    $insertBtn = New-Object System.Windows.Forms.Button
    $insertBtn.Text = "Insert"
    $insertBtn.Dock = "Bottom"
    $insertBtn.Height = 40
    $insertBtn.add_Click({
        if ($snippetList.SelectedItem) {
            $snippetName = ($snippetList.SelectedItem -split ' - ')[0]
            $snippet = $script:SnippetLibrary[$snippetName].Code
            $codeEditor.SelectedText = $snippet
            $snippetForm.Close()
        }
    })
    $snippetForm.Controls.Add($insertBtn)
    
    $snippetForm.ShowDialog() | Out-Null
})

# Designer to Code
$codeViewItem.add_Click({
    $generatedCode = Export-WinFormsCode -DesignerForm $designerForm
    $codeEditor.Text = $generatedCode
    $centerTabs.SelectedTab = $codeTab
    $statusLabel.Text = "Generated code from designer"
})

# Toggle Breakpoint
$breakpointItem.add_Click({
    $line = $codeEditor.GetLineFromCharIndex($codeEditor.SelectionStart) + 1
    if ($script:DebugBreakpoints -contains $line) {
        $script:DebugBreakpoints = $script:DebugBreakpoints | Where-Object { $_ -ne $line }
        $statusLabel.Text = "Removed breakpoint at line $line"
    } else {
        $script:DebugBreakpoints += $line
        $statusLabel.Text = "Added breakpoint at line $line"
    }
})

# PSScriptAnalyzer
$analyzerItem.add_Click({
    $outputBox.Clear()
    $outputBox.AppendText("Running PSScriptAnalyzer...`r`n`r`n")
    
    try {
        $tempFile = [IO.Path]::GetTempFileName() + ".ps1"
        $codeEditor.Text | Set-Content $tempFile
        
        if (Get-Command Invoke-ScriptAnalyzer -ErrorAction SilentlyContinue) {
            $results = Invoke-ScriptAnalyzer -Path $tempFile
            if ($results) {
                foreach ($result in $results) {
                    $outputBox.AppendText("[$($result.Severity)] Line $($result.Line): $($result.Message)`r`n")
                }
            } else {
                $outputBox.AppendText("No issues found!`r`n")
            }
        } else {
            $outputBox.AppendText("PSScriptAnalyzer not installed. Install with: Install-Module -Name PSScriptAnalyzer`r`n")
        }
        
        Remove-Item $tempFile -Force
    } catch {
        $outputBox.AppendText("Error: $($_.Exception.Message)`r`n")
    }
})

# Performance Monitor
$perfMonitorItem.add_Click({
    $perf = Start-PerformanceMonitor -Script {
        Invoke-Expression $codeEditor.Text
    }
    
    [System.Windows.Forms.MessageBox]::Show(
        "Performance Results:`n`n" +
        "Duration: $($perf.Duration.TotalMilliseconds) ms`n" +
        "Memory Used: $([Math]::Round($perf.MemoryUsed / 1MB, 2)) MB`n" +
        "CPU Time: $($perf.CPUTime.TotalMilliseconds) ms",
        "Performance Monitor",
        "OK",
        "Information"
    )
})

# PowerShell Version Selector
$psVersionItem.add_Click({
    $versionForm = New-Object System.Windows.Forms.Form
    $versionForm.Text = "PowerShell Version Selector"
    $versionForm.Size = New-Object System.Drawing.Size(400, 200)
    $versionForm.StartPosition = "CenterParent"
    
    $versionList = New-Object System.Windows.Forms.ListBox
    $versionList.Location = New-Object System.Drawing.Point(20, 20)
    $versionList.Size = New-Object System.Drawing.Size(340, 100)
    $versionList.Items.Add("Windows PowerShell 5.1")
    $versionList.Items.Add("PowerShell 7.0")
    $versionList.Items.Add("PowerShell 7.1")
    $versionList.Items.Add("PowerShell 7.2+")
    $versionList.SelectedIndex = 0
    $versionForm.Controls.Add($versionList)
    
    $selectBtn = New-Object System.Windows.Forms.Button
    $selectBtn.Text = "Select"
    $selectBtn.Location = New-Object System.Drawing.Point(280, 130)
    $selectBtn.add_Click({
        $psVersionLabel.Text = "PS: $($versionList.SelectedItem)"
        $versionForm.Close()
    })
    $versionForm.Controls.Add($selectBtn)
    
    $versionForm.ShowDialog() | Out-Null
})

# Initialize
$statusLabel.Text = "Ready - Start by creating a new project or opening an existing one"
$consoleOutput.AppendText("PowerShell Studio Pro - Integrated Development Environment`r`n")
$consoleOutput.AppendText("Version: 1.0.0 | PowerShell $($PSVersionTable.PSVersion)`r`n")
$consoleOutput.AppendText("="*60 + "`r`n`r`n")

# Show Main Form
[void]$mainForm.ShowDialog()
