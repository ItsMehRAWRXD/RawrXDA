# RawrXD.Tests.ps1
# Pester tests for RawrXD modules

Import-Module "D:/lazy init ide/RawrXD.Logging.psm1" -Force
Import-Module "D:/lazy init ide/RawrXD.Config.psm1" -Force

Describe 'Write-StructuredLog' {
    It 'Writes a log entry without error' {
        { Write-StructuredLog -Level 'INFO' -Message 'Test log' -Function 'TestFunc' } | Should -Not -Throw
    }
}

Describe 'Import-RawrXDConfig' {
    It 'Returns a hashtable or object' {
        $result = Import-RawrXDConfig -Path 'D:/lazy init ide/config/RawrXD.config.json'
        $result | Should -Not -BeNullOrEmpty
    }
}

Import-Module "D:/lazy init ide/RawrXD.Core.psm1" -Force
Import-Module "D:/lazy init ide/RawrXD.UI.psm1" -Force

Describe 'RawrXD.Core.psm1 Exported Functions' {
    It 'Send-OllamaRequest returns null or response' {
        $result = Send-OllamaRequest -Prompt 'Test' -Model 'llama3' -OllamaHost 'http://localhost:11434' -EnforceJSON $false
        $result | Should -BeNullOrHaveCount 1
    }
    It 'Register-AgentTool registers a tool' {
        { Register-AgentTool -Name 'test' -Description 'desc' -Handler { param($x) $x } } | Should -Not -Throw
    }
    It 'Parse-AgentCommand returns null or object' {
        $result = Parse-AgentCommand -AIResponse '{"tool":"test","args":{}}'
        $result | Should -BeNullOrOfType Hashtable
    }
    It 'Test-InputSafety returns boolean' {
        $result = Test-InputSafety -InputText 'safe text'
        $result | Should -BeOfType Boolean
    }
}

Describe 'RawrXD.UI.psm1 Exported Functions' {
    It 'Start-OllamaChatAsync returns a sync object' {
        $form = New-Object System.Windows.Forms.Form
        $chatBox = New-Object System.Windows.Forms.TextBox
        $result = Start-OllamaChatAsync -Prompt 'Test' -Form $form -ChatBox $chatBox
        $result | Should -BeOfType Hashtable
    }
    It 'Invoke-UIUpdate executes action' {
        $form = New-Object System.Windows.Forms.Form
        $called = $false
        Invoke-UIUpdate -Control $form -Action { $script:called = $true }
        $script:called | Should -Be $true
    }
    It 'Initialize-WebView2Safe returns a control' {
        $form = New-Object System.Windows.Forms.Form
        $result = Initialize-WebView2Safe -Container $form
        $result | Should -Not -BeNullOrEmpty
    }
    It 'Update-ChatBoxThreadSafe appends text' {
        $form = New-Object System.Windows.Forms.Form
        $chatBox = New-Object System.Windows.Forms.TextBox
        Update-ChatBoxThreadSafe -ChatBox $chatBox -Text 'hi' -Form $form
        $chatBox.Text | Should -Match 'hi'
    }
}
