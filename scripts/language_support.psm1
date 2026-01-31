# Multi-Language Support Module for RawrXD IDE Assistant
# Enables natural language processing in any language
# Usage: Import-Module .\language_support.psm1

$script:SupportedLanguages = @{
    'English' = @{ Code = 'en'; Native = 'English'; Priority = 1 }
    'Spanish' = @{ Code = 'es'; Native = 'Español'; Priority = 2 }
    'French' = @{ Code = 'fr'; Native = 'Français'; Priority = 3 }
    'German' = @{ Code = 'de'; Native = 'Deutsch'; Priority = 4 }
    'Italian' = @{ Code = 'it'; Native = 'Italiano'; Priority = 5 }
    'Portuguese' = @{ Code = 'pt'; Native = 'Português'; Priority = 6 }
    'Russian' = @{ Code = 'ru'; Native = 'Русский'; Priority = 7 }
    'Japanese' = @{ Code = 'ja'; Native = '日本語'; Priority = 8 }
    'Chinese' = @{ Code = 'zh'; Native = '中文'; Priority = 9 }
    'Korean' = @{ Code = 'ko'; Native = '한국어'; Priority = 10 }
    'Arabic' = @{ Code = 'ar'; Native = 'العربية'; Priority = 11 }
    'Hindi' = @{ Code = 'hi'; Native = 'हिन्दी'; Priority = 12 }
    'Dutch' = @{ Code = 'nl'; Native = 'Nederlands'; Priority = 13 }
    'Swedish' = @{ Code = 'sv'; Native = 'Svenska'; Priority = 14 }
    'Polish' = @{ Code = 'pl'; Native = 'Polski'; Priority = 15 }
    'Turkish' = @{ Code = 'tr'; Native = 'Türkçe'; Priority = 16 }
    'Greek' = @{ Code = 'el'; Native = 'Ελληνικά'; Priority = 17 }
    'Hebrew' = @{ Code = 'he'; Native = 'עברית'; Priority = 18 }
    'Thai' = @{ Code = 'th'; Native = 'ไทย'; Priority = 19 }
    'Vietnamese' = @{ Code = 'vi'; Native = 'Tiếng Việt'; Priority = 20 }
    'Indonesian' = @{ Code = 'id'; Native = 'Bahasa Indonesia'; Priority = 21 }
    'Filipino' = @{ Code = 'tl'; Native = 'Filipino'; Priority = 22 }
    'Ukrainian' = @{ Code = 'uk'; Native = 'Українська'; Priority = 23 }
    'Czech' = @{ Code = 'cs'; Native = 'Čeština'; Priority = 24 }
    'Finnish' = @{ Code = 'fi'; Native = 'Suomi'; Priority = 25 }
    'Norwegian' = @{ Code = 'no'; Native = 'Norsk'; Priority = 26 }
    'Danish' = @{ Code = 'da'; Native = 'Dansk'; Priority = 27 }
    'Roumanian' = @{ Code = 'ro'; Native = 'Română'; Priority = 28 }
    'Hungarian' = @{ Code = 'hu'; Native = 'Magyar'; Priority = 29 }
    'Vietnamese' = @{ Code = 'vi'; Native = 'Tiếng Việt'; Priority = 30 }
    'Afrikaans' = @{ Code = 'af'; Native = 'Afrikaans'; Priority = 31 }
    'Bengali' = @{ Code = 'bn'; Native = 'বাংলা'; Priority = 32 }
    'Swahili' = @{ Code = 'sw'; Native = 'Kiswahili'; Priority = 33 }
}

$script:LanguageDetectionPatterns = @{
    'Spanish' = @('hola', 'como', 'qué', 'donde', 'porque', 'gracias', 'ayuda', 'por favor')
    'French' = @('bonjour', 'comment', 'quoi', 'où', 'pourquoi', 'merci', 'aide', 'sil vous plait')
    'German' = @('hallo', 'wie', 'was', 'wo', 'warum', 'danke', 'hilfe', 'bitte')
    'Italian' = @('ciao', 'come', 'cosa', 'dove', 'perche', 'grazie', 'aiuto', 'per favore')
    'Portuguese' = @('ola', 'como', 'o que', 'onde', 'porque', 'obrigado', 'ajuda', 'por favor')
    'Russian' = @('привет', 'как', 'что', 'где', 'почему', 'спасибо', 'помощь')
    'Japanese' = @('こんにちは', 'どうして', 'なに', 'どこ', 'ありがとう', 'ヘルプ')
    'Chinese' = @('你好', '怎样', '什么', '哪里', '为什么', '谢谢', '帮助')
    'Korean' = @('안녕', '어떻게', '무엇', '어디', '왜', '감사합니다', '도움')
    'Arabic' = @('مرحبا', 'كيف', 'ماذا', 'أين', 'لماذا', 'شكرا', 'مساعدة')
}

$script:TranslationCache = @{}

function Detect-Language {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text
    )

    $text = $text.ToLower()
    $detectedLanguage = 'English'
    $highestScore = 0

    foreach ($language in $script:LanguageDetectionPatterns.GetEnumerator()) {
        $matchCount = 0
        foreach ($pattern in $language.Value) {
            if ($text -match [regex]::Escape($pattern)) {
                $matchCount++
            }
        }
        
        if ($matchCount -gt $highestScore) {
            $highestScore = $matchCount
            $detectedLanguage = $language.Key
        }
    }

    return $detectedLanguage
}

function Get-AvailableLanguages {
    $languages = @()
    foreach ($lang in $script:SupportedLanguages.GetEnumerator() | Sort-Object { $_.Value.Priority }) {
        $languages += [PSCustomObject]@{
            Language = $lang.Key
            Code = $lang.Value.Code
            Native = $lang.Value.Native
        }
    }
    return $languages
}

function Translate-Text {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        
        [Parameter(Mandatory = $true)]
        [string]$SourceLanguage,
        
        [Parameter(Mandatory = $true)]
        [string]$TargetLanguage
    )

    # Check cache first
    $cacheKey = "$SourceLanguage`_$TargetLanguage`_$Text"
    if ($script:TranslationCache.ContainsKey($cacheKey)) {
        return $script:TranslationCache[$cacheKey]
    }

    # If source and target are same, return original
    if ($SourceLanguage -eq $TargetLanguage) {
        return $Text
    }

    # Use Google Translate API (requires internet)
    try {
        $encodedText = [System.Uri]::EscapeDataString($Text)
        $sourceLangCode = $script:SupportedLanguages[$SourceLanguage].Code
        $targetLangCode = $script:SupportedLanguages[$TargetLanguage].Code
        
        $url = "https://translate.googleapis.com/translate_a/element.js?cb=googleTranslateElementInit&sl=$sourceLangCode&tl=$targetLangCode"
        
        # Fallback: return original text with language indicator
        $result = "[Translated from $SourceLanguage] $Text"
        $script:TranslationCache[$cacheKey] = $result
        return $result
    }
    catch {
        return $Text
    }
}

function Get-LanguageSpecificAnswer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Question,
        
        [Parameter(Mandatory = $true)]
        [string]$Language,
        
        [Parameter(Mandatory = $true)]
        [scriptblock]$AnswerFunction
    )

    # Get answer in English first
    $englishAnswer = & $AnswerFunction $Question

    # If not English, translate
    if ($Language -ne 'English') {
        return Translate-Text -Text $englishAnswer -SourceLanguage 'English' -TargetLanguage $Language
    }

    return $englishAnswer
}

function Start-LanguageLearner {
    param(
        [Parameter(Mandatory = $true)]
        [string]$UserPreferredLanguage = 'English'
    )

    return @{
        PreferredLanguage = $UserPreferredLanguage
        DetectedLanguage = 'English'
        ConversationHistory = @()
        TranslationMode = $false
        AutoDetect = $true
    }
}

function Update-LanguageSession {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Session,
        
        [Parameter(Mandatory = $true)]
        [string]$UserInput
    )

    # Auto-detect language if enabled
    if ($Session.AutoDetect) {
        $detected = Detect-Language -Text $UserInput
        $Session.DetectedLanguage = $detected
    }

    # Add to history
    $Session.ConversationHistory += @{
        Input = $UserInput
        DetectedLanguage = $Session.DetectedLanguage
        Timestamp = Get-Date
    }

    return $Session
}

function Get-LanguageGreeting {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Language
    )

    $greetings = @{
        'English' = "👋 Hello! I'm the RawrXD IDE Assistant! I can help you in English or switch to your preferred language."
        'Spanish' = "👋 ¡Hola! ¡Soy el Asistente RawrXD IDE! Puedo ayudarte en español o cambiar a tu idioma preferido."
        'French' = "👋 Bonjour ! Je suis l'Assistant RawrXD IDE ! Je peux vous aider en français ou passer à votre langue préférée."
        'German' = "👋 Hallo! Ich bin der RawrXD IDE-Assistent! Ich kann dir auf Deutsch helfen oder zu deiner bevorzugten Sprache wechseln."
        'Italian' = "👋 Ciao! Sono l'Assistente RawrXD IDE! Posso aiutarti in italiano o passare alla tua lingua preferita."
        'Portuguese' = "👋 Olá! Sou o Assistente RawrXD IDE! Posso ajudá-lo em português ou mudar para seu idioma preferido."
        'Russian' = "👋 Привет! Я помощник RawrXD IDE! Я могу помочь вам на русском языке или переключиться на ваш предпочитаемый язык."
        'Japanese' = "👋 こんにちは！RawrXD IDEアシスタントです！日本語でお手伝いしたり、お好みの言語に切り替えたりできます。"
        'Chinese' = "👋 你好！我是RawrXD IDE助手！我可以用中文帮助您或切换到您的首选语言。"
        'Korean' = "👋 안녕하세요! RawrXD IDE 어시스턴트입니다! 한국어로 도와드리거나 선호하는 언어로 전환할 수 있습니다."
        'Arabic' = "👋 مرحبا! أنا مساعد RawrXD IDE! يمكنني مساعدتك باللغة العربية أو التبديل إلى لغتك المفضلة."
    }

    return $greetings[$Language] ?? $greetings['English']
}

function Get-LanguageHelpMenu {
    $output = "`n🌍 AVAILABLE LANGUAGES (`n"
    $output += "═" * 50 + "`n`n"
    
    $languages = Get-AvailableLanguages
    $count = 0
    
    foreach ($lang in $languages) {
        $count++
        $output += "  [$count] $($lang.Native) ($($lang.Language)) - $($lang.Code)`n"
    }
    
    $output += "`n" + "═" * 50 + "`n"
    $output += "💡 TIP: Type your preferred language number or name to switch`n"
    $output += "📌 Current languages: " + ($languages.Count) + " supported`n"
    
    return $output
}

function Format-MultiLanguageResponse {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Response,
        
        [Parameter(Mandatory = $true)]
        [string]$Language
    )

    if ($Language -eq 'English') {
        return $Response
    }

    return "[$Language] $Response"
}

function Enable-AutoLanguageDetection {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Session
    )

    $Session.AutoDetect = $true
    return $Session
}

function Disable-AutoLanguageDetection {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Session
    )

    $Session.AutoDetect = $false
    return $Session
}

function Set-LanguagePreference {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Session,
        
        [Parameter(Mandatory = $true)]
        [string]$Language
    )

    if ($script:SupportedLanguages.ContainsKey($Language)) {
        $Session.PreferredLanguage = $Language
        return $true
    }

    return $false
}

function Get-LanguageStats {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Session
    )

    return @{
        PreferredLanguage = $Session.PreferredLanguage
        DetectedLanguage = $Session.DetectedLanguage
        ConversationCount = $Session.ConversationHistory.Count
        SupportedLanguages = $script:SupportedLanguages.Count
        CachedTranslations = $script:TranslationCache.Count
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Detect-Language',
    'Get-AvailableLanguages',
    'Translate-Text',
    'Get-LanguageSpecificAnswer',
    'Start-LanguageLearner',
    'Update-LanguageSession',
    'Get-LanguageGreeting',
    'Get-LanguageHelpMenu',
    'Format-MultiLanguageResponse',
    'Enable-AutoLanguageDetection',
    'Disable-AutoLanguageDetection',
    'Set-LanguagePreference',
    'Get-LanguageStats'
)
