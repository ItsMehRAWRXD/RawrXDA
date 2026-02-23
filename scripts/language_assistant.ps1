# Multi-Language IDE Assistant Launcher
# Supports 20+ languages with automatic detection

param(
    [string]$Language = 'English',
    [switch]$ListLanguages,
    [switch]$AutoDetect
)

# Import language support module
Import-Module "$PSScriptRoot\language_support.psm1" -Force

function Show-LanguageMenu {
    Clear-Host
    Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║     🌍 RawrXD IDE Multi-Language Assistant Setup       ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    if ($ListLanguages) {
        $languages = Get-AvailableLanguages
        Write-Host "📚 Available Languages:" -ForegroundColor Green
        Write-Host ""
        
        $count = 0
        foreach ($lang in $languages) {
            $count++
            $padding = "".PadRight(2 - $count.ToString().Length)
            Write-Host "  [$padding$count] $($lang.Native.PadRight(20)) ($($lang.Language))" -ForegroundColor Yellow
        }
        
        Write-Host ""
        Write-Host "═" * 54
        Write-Host "💡 Total: $($languages.Count) languages supported" -ForegroundColor Cyan
        return
    }

    $session = Start-LanguageLearner -UserPreferredLanguage $Language
    
    Write-Host "🎯 Configuration:" -ForegroundColor Green
    Write-Host "  Language: $Language" -ForegroundColor Yellow
    Write-Host "  Auto-Detect: $(if ($AutoDetect) { 'Enabled' } else { 'Disabled' })" -ForegroundColor Yellow
    Write-Host "  Greeting: $(Get-LanguageGreeting -Language $Language | Select-Object -First 1)" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "🚀 Next Steps:" -ForegroundColor Green
    Write-Host "  1. Open IDE Browser:" -ForegroundColor Yellow
    Write-Host "     Start-Process 'D:\lazy init ide\gui\ide_chatbot.html'" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  2. Select your language from the 🌍 dropdown" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  3. Start chatting (20+ languages automatically detected)" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "📝 Language Help:" -ForegroundColor Green
    Write-Host $(Get-LanguageHelpMenu)
    Write-Host ""
    
    Write-Host "💬 Language Statistics:" -ForegroundColor Green
    $stats = Get-LanguageStats -Session $session
    Write-Host "  Supported Languages: $($stats.SupportedLanguages)" -ForegroundColor Yellow
    Write-Host "  Preferred Language: $($stats.PreferredLanguage)" -ForegroundColor Yellow
    Write-Host "  Detected Language: $($stats.DetectedLanguage)" -ForegroundColor Yellow
    Write-Host ""
}

function Launch-MultiLanguageChat {
    param(
        [string]$Language = 'English'
    )
    
    Write-Host "🚀 Launching RawrXD IDE Assistant with language support..." -ForegroundColor Cyan
    
    # Check if chatbot exists
    $chatbotPath = "$PSScriptRoot\..\gui\ide_chatbot.html"
    if (Test-Path $chatbotPath) {
        Start-Process $chatbotPath
        Write-Host "✅ IDE Browser launched! Language: $Language" -ForegroundColor Green
        Write-Host "💡 Tip: Click 🌍 Language button to switch languages" -ForegroundColor Yellow
    }
    else {
        Write-Host "❌ IDE Chatbot not found at: $chatbotPath" -ForegroundColor Red
        Write-Host "📁 Please ensure you're running from the correct directory" -ForegroundColor Yellow
    }
}

function Test-LanguageDetection {
    Write-Host "🧪 Testing Language Detection..." -ForegroundColor Cyan
    Write-Host ""
    
    $testCases = @(
        "Hello, how can you help me?",
        "Hola, ¿cómo puedes ayudarme?",
        "Bonjour, comment pouvez-vous m'aider?",
        "Hallo, wie kannst du mir helfen?",
        "Ciao, come puoi aiutarmi?",
        "こんにちは、どのようにお手伝いできますか？",
        "你好，你怎样能帮我？",
        "안녕하세요, 어떻게 도와드릴까요?",
        "مرحبا، كيف يمكنك مساعدتي؟"
    )

    foreach ($test in $testCases) {
        $detected = Detect-Language -Text $test
        $preview = if ($test.Length -gt 40) { $test.Substring(0, 40) + "..." } else { $test }
        Write-Host "  Text: $preview" -ForegroundColor Gray
        Write-Host "  Detected: $detected" -ForegroundColor Green
        Write-Host ""
    }
}

# Main execution
if ($ListLanguages) {
    Show-LanguageMenu
}
elseif ($Language -eq 'Test') {
    Test-LanguageDetection
}
else {
    Show-LanguageMenu
    Launch-MultiLanguageChat -Language $Language
}

Write-Host ""
Write-Host "📖 For more information, check the documentation:" -ForegroundColor Cyan
Write-Host "  • Multi-Language Support Module: language_support.psm1" -ForegroundColor Yellow
Write-Host "  • IDE Chatbot: gui/ide_chatbot.html" -ForegroundColor Yellow
Write-Host "  • Examples in the help menu" -ForegroundColor Yellow
Write-Host ""
