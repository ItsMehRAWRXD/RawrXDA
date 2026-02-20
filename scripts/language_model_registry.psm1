# Language-Model Registry & Dynamic Loading System
# Manages 60+ custom languages paired with models for specific tasks
# Supports runtime language switching and complete state reset

. "$PSScriptRoot\\RawrXD_Root.ps1"

$script:LanguageRegistry = @{}
$script:ModelLanguageMap = @{}
$script:ActiveLanguages = @{}
$script:CompilerCache = @{}
$script:LanguageState = @{}

# ============================================================================
# MASTER LANGUAGE REGISTRY (60+ LANGUAGES)
# ============================================================================

$script:AllLanguages = @{
    # Tier 1: Core European Languages (10)
    'English' = @{
        Code = 'en'
        Category = 'European'
        Compiler = 'English-Compiler-v1.0'
        Aliases = @('eng', 'en-US', 'en-GB')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Claude-3', 'Llama-2')
        TaskTypes = @('general', 'coding', 'documentation')
    }
    'Spanish' = @{
        Code = 'es'
        Category = 'European'
        Compiler = 'Spanish-Compiler-v1.0'
        Aliases = @('spa', 'es-ES', 'es-MX')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Llama-2')
        TaskTypes = @('general', 'documentation')
    }
    'French' = @{
        Code = 'fr'
        Category = 'European'
        Compiler = 'French-Compiler-v1.0'
        Aliases = @('fra', 'fr-FR', 'fr-CA')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Claude-3')
        TaskTypes = @('general', 'documentation')
    }
    'German' = @{
        Code = 'de'
        Category = 'European'
        Compiler = 'German-Compiler-v1.0'
        Aliases = @('deu', 'de-DE')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Llama-2')
        TaskTypes = @('general', 'technical')
    }
    'Italian' = @{
        Code = 'it'
        Category = 'European'
        Compiler = 'Italian-Compiler-v1.0'
        Aliases = @('ita', 'it-IT')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4')
        TaskTypes = @('general')
    }
    'Portuguese' = @{
        Code = 'pt'
        Category = 'European'
        Compiler = 'Portuguese-Compiler-v1.0'
        Aliases = @('por', 'pt-BR', 'pt-PT')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Llama-2')
        TaskTypes = @('general')
    }
    'Russian' = @{
        Code = 'ru'
        Category = 'European'
        Compiler = 'Russian-Compiler-v1.0'
        Aliases = @('rus', 'ru-RU')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Llama-2')
        TaskTypes = @('general', 'technical')
    }
    'Polish' = @{
        Code = 'pl'
        Category = 'European'
        Compiler = 'Polish-Compiler-v1.0'
        Aliases = @('pol', 'pl-PL')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Dutch' = @{
        Code = 'nl'
        Category = 'European'
        Compiler = 'Dutch-Compiler-v1.0'
        Aliases = @('nld', 'nl-NL')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4')
        TaskTypes = @('general')
    }
    'Swedish' = @{
        Code = 'sv'
        Category = 'European'
        Compiler = 'Swedish-Compiler-v1.0'
        Aliases = @('swe', 'sv-SE')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }

    # Tier 2: Asian Languages (15)
    'Japanese' = @{
        Code = 'ja'
        Category = 'Asian'
        Compiler = 'Japanese-Compiler-v1.0'
        Aliases = @('jpn', 'ja-JP')
        Features = @('nlp', 'speech', 'text', 'kanji')
        Models = @('GPT-4', 'Claude-3', 'Llama-JP')
        TaskTypes = @('general', 'technical', 'creative')
    }
    'Chinese' = @{
        Code = 'zh'
        Category = 'Asian'
        Compiler = 'Chinese-Compiler-v1.0'
        Aliases = @('zho', 'zh-CN', 'zh-TW')
        Features = @('nlp', 'speech', 'text', 'hanzi')
        Models = @('GPT-4', 'Llama-2', 'Qwen')
        TaskTypes = @('general', 'technical', 'cultural')
    }
    'Korean' = @{
        Code = 'ko'
        Category = 'Asian'
        Compiler = 'Korean-Compiler-v1.0'
        Aliases = @('kor', 'ko-KR')
        Features = @('nlp', 'speech', 'text', 'hangul')
        Models = @('GPT-4', 'Llama-2', 'KoGPT')
        TaskTypes = @('general', 'technical')
    }
    'Hindi' = @{
        Code = 'hi'
        Category = 'Asian'
        Compiler = 'Hindi-Compiler-v1.0'
        Aliases = @('hin', 'hi-IN')
        Features = @('nlp', 'speech', 'text', 'devanagari')
        Models = @('Llama-2', 'IndicBERT')
        TaskTypes = @('general', 'documentation')
    }
    'Thai' = @{
        Code = 'th'
        Category = 'Asian'
        Compiler = 'Thai-Compiler-v1.0'
        Aliases = @('tha', 'th-TH')
        Features = @('nlp', 'speech', 'text', 'thai-script')
        Models = @('Llama-2', 'ThaiGPT')
        TaskTypes = @('general')
    }
    'Vietnamese' = @{
        Code = 'vi'
        Category = 'Asian'
        Compiler = 'Vietnamese-Compiler-v1.0'
        Aliases = @('vie', 'vi-VN')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'PhoGPT')
        TaskTypes = @('general', 'technical')
    }
    'Indonesian' = @{
        Code = 'id'
        Category = 'Asian'
        Compiler = 'Indonesian-Compiler-v1.0'
        Aliases = @('ind', 'id-ID')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'IndoBERT')
        TaskTypes = @('general')
    }
    'Tagalog' = @{
        Code = 'tl'
        Category = 'Asian'
        Compiler = 'Tagalog-Compiler-v1.0'
        Aliases = @('tgl', 'fil', 'ph')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Malay' = @{
        Code = 'ms'
        Category = 'Asian'
        Compiler = 'Malay-Compiler-v1.0'
        Aliases = @('msa', 'ms-MY', 'ms-BN')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Turkish' = @{
        Code = 'tr'
        Category = 'Asian-Middle-East'
        Compiler = 'Turkish-Compiler-v1.0'
        Aliases = @('tur', 'tr-TR')
        Features = @('nlp', 'speech', 'text')
        Models = @('GPT-4', 'Llama-2', 'TurkishBERT')
        TaskTypes = @('general', 'technical')
    }
    'Bengali' = @{
        Code = 'bn'
        Category = 'Asian'
        Compiler = 'Bengali-Compiler-v1.0'
        Aliases = @('ben', 'bn-BD', 'bn-IN')
        Features = @('nlp', 'speech', 'text', 'bengali-script')
        Models = @('IndicBERT', 'Llama-2')
        TaskTypes = @('general')
    }
    'Punjabi' = @{
        Code = 'pa'
        Category = 'Asian'
        Compiler = 'Punjabi-Compiler-v1.0'
        Aliases = @('pan', 'pa-IN', 'pa-PK')
        Features = @('nlp', 'speech', 'text')
        Models = @('IndicBERT')
        TaskTypes = @('general')
    }
    'Urdu' = @{
        Code = 'ur'
        Category = 'Asian-Middle-East'
        Compiler = 'Urdu-Compiler-v1.0'
        Aliases = @('urd', 'ur-PK', 'ur-IN')
        Features = @('nlp', 'speech', 'text', 'urdu-script')
        Models = @('UrduBERT', 'Llama-2')
        TaskTypes = @('general')
    }
    'Gujarati' = @{
        Code = 'gu'
        Category = 'Asian'
        Compiler = 'Gujarati-Compiler-v1.0'
        Aliases = @('guj', 'gu-IN')
        Features = @('nlp', 'speech', 'text')
        Models = @('IndicBERT')
        TaskTypes = @('general')
    }
    'Marathi' = @{
        Code = 'mr'
        Category = 'Asian'
        Compiler = 'Marathi-Compiler-v1.0'
        Aliases = @('mar', 'mr-IN')
        Features = @('nlp', 'speech', 'text')
        Models = @('IndicBERT')
        TaskTypes = @('general')
    }

    # Tier 3: Middle Eastern & African Languages (12)
    'Arabic' = @{
        Code = 'ar'
        Category = 'Middle-Eastern'
        Compiler = 'Arabic-Compiler-v1.0'
        Aliases = @('ara', 'ar-SA', 'ar-AE', 'ar-EG')
        Features = @('nlp', 'speech', 'text', 'rtl', 'arabic-script')
        Models = @('GPT-4', 'Llama-2', 'AraGPT')
        TaskTypes = @('general', 'technical', 'cultural')
    }
    'Hebrew' = @{
        Code = 'he'
        Category = 'Middle-Eastern'
        Compiler = 'Hebrew-Compiler-v1.0'
        Aliases = @('heb', 'he-IL')
        Features = @('nlp', 'speech', 'text', 'rtl', 'hebrew-script')
        Models = @('GPT-4', 'Llama-2', 'AlephBERT')
        TaskTypes = @('general', 'technical')
    }
    'Persian' = @{
        Code = 'fa'
        Category = 'Middle-Eastern'
        Compiler = 'Persian-Compiler-v1.0'
        Aliases = @('fas', 'fa-IR')
        Features = @('nlp', 'speech', 'text', 'rtl', 'persian-script')
        Models = @('Llama-2', 'PersianBERT')
        TaskTypes = @('general')
    }
    'Swahili' = @{
        Code = 'sw'
        Category = 'African'
        Compiler = 'Swahili-Compiler-v1.0'
        Aliases = @('swa', 'sw-KE', 'sw-TZ')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Yoruba' = @{
        Code = 'yo'
        Category = 'African'
        Compiler = 'Yoruba-Compiler-v1.0'
        Aliases = @('yor', 'yo-NG')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'AfroLM')
        TaskTypes = @('general', 'cultural')
    }
    'Amharic' = @{
        Code = 'am'
        Category = 'African'
        Compiler = 'Amharic-Compiler-v1.0'
        Aliases = @('amh', 'am-ET')
        Features = @('nlp', 'speech', 'text', 'amharic-script')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Igbo' = @{
        Code = 'ig'
        Category = 'African'
        Compiler = 'Igbo-Compiler-v1.0'
        Aliases = @('ibo', 'ig-NG')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'AfroLM')
        TaskTypes = @('general', 'cultural')
    }
    'Hausa' = @{
        Code = 'ha'
        Category = 'African'
        Compiler = 'Hausa-Compiler-v1.0'
        Aliases = @('hau', 'ha-NG')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Somali' = @{
        Code = 'so'
        Category = 'African'
        Compiler = 'Somali-Compiler-v1.0'
        Aliases = @('som', 'so-SO')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Afrikaans' = @{
        Code = 'af'
        Category = 'African'
        Compiler = 'Afrikaans-Compiler-v1.0'
        Aliases = @('afr', 'af-ZA')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Zulu' = @{
        Code = 'zu'
        Category = 'African'
        Compiler = 'Zulu-Compiler-v1.0'
        Aliases = @('zul', 'zu-ZA')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'AfroLM')
        TaskTypes = @('general', 'cultural')
    }
    'Xhosa' = @{
        Code = 'xh'
        Category = 'African'
        Compiler = 'Xhosa-Compiler-v1.0'
        Aliases = @('xho', 'xh-ZA')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }

    # Tier 4: Additional European & Slavic (10)
    'Greek' = @{
        Code = 'el'
        Category = 'European'
        Compiler = 'Greek-Compiler-v1.0'
        Aliases = @('ell', 'el-GR')
        Features = @('nlp', 'speech', 'text', 'greek-script')
        Models = @('GPT-4', 'Llama-2', 'GreekBERT')
        TaskTypes = @('general', 'technical')
    }
    'Czech' = @{
        Code = 'cs'
        Category = 'European'
        Compiler = 'Czech-Compiler-v1.0'
        Aliases = @('ces', 'cs-CZ')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'CzechBERT')
        TaskTypes = @('general')
    }
    'Slovak' = @{
        Code = 'sk'
        Category = 'European'
        Compiler = 'Slovak-Compiler-v1.0'
        Aliases = @('slk', 'sk-SK')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Hungarian' = @{
        Code = 'hu'
        Category = 'European'
        Compiler = 'Hungarian-Compiler-v1.0'
        Aliases = @('hun', 'hu-HU')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'HunBERT')
        TaskTypes = @('general', 'technical')
    }
    'Romanian' = @{
        Code = 'ro'
        Category = 'European'
        Compiler = 'Romanian-Compiler-v1.0'
        Aliases = @('ron', 'ro-RO')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'RomanianBERT')
        TaskTypes = @('general')
    }
    'Bulgarian' = @{
        Code = 'bg'
        Category = 'European'
        Compiler = 'Bulgarian-Compiler-v1.0'
        Aliases = @('bul', 'bg-BG')
        Features = @('nlp', 'speech', 'text', 'cyrillic')
        Models = @('Llama-2', 'BulBERT')
        TaskTypes = @('general')
    }
    'Serbian' = @{
        Code = 'sr'
        Category = 'European'
        Compiler = 'Serbian-Compiler-v1.0'
        Aliases = @('srp', 'sr-RS', 'sr-BA')
        Features = @('nlp', 'speech', 'text', 'cyrillic', 'latin')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Croatian' = @{
        Code = 'hr'
        Category = 'European'
        Compiler = 'Croatian-Compiler-v1.0'
        Aliases = @('hrv', 'hr-HR')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Slovenian' = @{
        Code = 'sl'
        Category = 'European'
        Compiler = 'Slovenian-Compiler-v1.0'
        Aliases = @('slv', 'sl-SI')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Lithuanian' = @{
        Code = 'lt'
        Category = 'European'
        Compiler = 'Lithuanian-Compiler-v1.0'
        Aliases = @('lit', 'lt-LT')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }

    # Tier 5: Nordic & Baltic (5)
    'Norwegian' = @{
        Code = 'no'
        Category = 'European'
        Compiler = 'Norwegian-Compiler-v1.0'
        Aliases = @('nor', 'no-NO')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'NorBERT')
        TaskTypes = @('general', 'technical')
    }
    'Danish' = @{
        Code = 'da'
        Category = 'European'
        Compiler = 'Danish-Compiler-v1.0'
        Aliases = @('dan', 'da-DK')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'DanBERT')
        TaskTypes = @('general')
    }
    'Finnish' = @{
        Code = 'fi'
        Category = 'European'
        Compiler = 'Finnish-Compiler-v1.0'
        Aliases = @('fin', 'fi-FI')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'FinBERT')
        TaskTypes = @('general', 'technical')
    }
    'Icelandic' = @{
        Code = 'is'
        Category = 'European'
        Compiler = 'Icelandic-Compiler-v1.0'
        Aliases = @('isl', 'is-IS')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Estonian' = @{
        Code = 'et'
        Category = 'European'
        Compiler = 'Estonian-Compiler-v1.0'
        Aliases = @('est', 'et-EE')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2', 'EstonianBERT')
        TaskTypes = @('general')
    }

    # Tier 6: Additional Specialized Languages (8)
    'Ukrainian' = @{
        Code = 'uk'
        Category = 'European'
        Compiler = 'Ukrainian-Compiler-v1.0'
        Aliases = @('ukr', 'uk-UA')
        Features = @('nlp', 'speech', 'text', 'cyrillic')
        Models = @('Llama-2', 'UkrainianBERT')
        TaskTypes = @('general', 'technical')
    }
    'Latvian' = @{
        Code = 'lv'
        Category = 'European'
        Compiler = 'Latvian-Compiler-v1.0'
        Aliases = @('lav', 'lv-LV')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
    'Vietnamese-Modern' = @{
        Code = 'vi-mod'
        Category = 'Asian'
        Compiler = 'Vietnamese-Modern-Compiler-v1.0'
        Aliases = @('vie-mod', 'vi-modern')
        Features = @('nlp', 'speech', 'text', 'modern-vietnamese')
        Models = @('Llama-2', 'PhoGPT-v4')
        TaskTypes = @('coding', 'technical', 'modern')
    }
    'Simplified-Chinese' = @{
        Code = 'zh-simp'
        Category = 'Asian'
        Compiler = 'Simplified-Chinese-Compiler-v1.0'
        Aliases = @('zho-simp', 'zh-CN-simp')
        Features = @('nlp', 'speech', 'text', 'simplified-hanzi')
        Models = @('Qwen', 'Llama-2-Chinese')
        TaskTypes = @('coding', 'technical')
    }
    'Traditional-Chinese' = @{
        Code = 'zh-trad'
        Category = 'Asian'
        Compiler = 'Traditional-Chinese-Compiler-v1.0'
        Aliases = @('zho-trad', 'zh-TW-trad')
        Features = @('nlp', 'speech', 'text', 'traditional-hanzi')
        Models = @('Llama-2-Chinese')
        TaskTypes = @('general', 'cultural')
    }
    'Cantonese' = @{
        Code = 'yue'
        Category = 'Asian'
        Compiler = 'Cantonese-Compiler-v1.0'
        Aliases = @('jyut', 'zh-HK')
        Features = @('nlp', 'speech', 'text', 'cantonese-chars')
        Models = @('CantonBERT')
        TaskTypes = @('general', 'cultural')
    }
    'Taiwanese' = @{
        Code = 'nan'
        Category = 'Asian'
        Compiler = 'Taiwanese-Compiler-v1.0'
        Aliases = @('twn', 'hokkien')
        Features = @('nlp', 'speech', 'text')
        Models = @('Llama-2')
        TaskTypes = @('general', 'cultural')
    }
    'Khmer' = @{
        Code = 'km'
        Category = 'Asian'
        Compiler = 'Khmer-Compiler-v1.0'
        Aliases = @('khm', 'km-KH')
        Features = @('nlp', 'speech', 'text', 'khmer-script')
        Models = @('Llama-2', 'KhmerBERT')
        TaskTypes = @('general')
    }
    'Lao' = @{
        Code = 'lo'
        Category = 'Asian'
        Compiler = 'Lao-Compiler-v1.0'
        Aliases = @('lao', 'lo-LA')
        Features = @('nlp', 'speech', 'text', 'lao-script')
        Models = @('Llama-2')
        TaskTypes = @('general')
    }
}

# ============================================================================
# MODEL-LANGUAGE ASSOCIATIONS
# ============================================================================

$script:ModelLanguagePairings = @{
    'GPT-4' = @{
        PrimaryLanguages = @('English', 'Spanish', 'French', 'German', 'Chinese')
        SecondaryLanguages = @('Italian', 'Portuguese', 'Russian', 'Japanese', 'Arabic')
        OptimalTasks = @('coding', 'documentation', 'creative', 'analysis')
        PerformanceRating = 0.95
    }
    'Claude-3' = @{
        PrimaryLanguages = @('English', 'French', 'German')
        SecondaryLanguages = @('Spanish', 'Japanese', 'Portuguese')
        OptimalTasks = @('analysis', 'documentation', 'technical-writing')
        PerformanceRating = 0.93
    }
    'Llama-2' = @{
        PrimaryLanguages = @('English')
        SecondaryLanguages = @('Spanish', 'French', 'German', 'Italian', 'Portuguese', 'Russian', 'Japanese', 'Chinese', 'Korean', 'Arabic')
        OptimalTasks = @('general', 'coding', 'multilingual')
        PerformanceRating = 0.88
        SupportedLanguages = @() # Will be populated dynamically
    }
    'Qwen' = @{
        PrimaryLanguages = @('Chinese')
        SecondaryLanguages = @('English', 'Japanese', 'Korean')
        OptimalTasks = @('coding', 'technical', 'multilingual-asia')
        PerformanceRating = 0.92
    }
    'Custom-Model-v1' = @{
        PrimaryLanguages = @('English', 'Spanish', 'Portuguese')
        SecondaryLanguages = @() # Can be configured per deployment
        OptimalTasks = @('custom-tasks', 'domain-specific')
        PerformanceRating = 0.85
    }
}

# ============================================================================
# CORE FUNCTIONS
# ============================================================================

function Get-AllAvailableLanguages {
    <#
    .SYNOPSIS
    Returns all 60+ languages available in the registry
    #>
    return $script:AllLanguages
}

function Get-LanguagesByCategory {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Category
    )
    
    return $script:AllLanguages.GetEnumerator() | 
        Where-Object { $_.Value.Category -eq $Category } |
        Select-Object -ExpandProperty Value
}

function Get-LanguagesForModel {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModelName
    )
    
    if (-not $script:ModelLanguagePairings.ContainsKey($ModelName)) {
        return @()
    }
    
    $pairing = $script:ModelLanguagePairings[$ModelName]
    $allSupported = $pairing.PrimaryLanguages + $pairing.SecondaryLanguages
    
    return $allSupported | ForEach-Object {
        @{
            Language = $_
            Tier = if ($_ -in $pairing.PrimaryLanguages) { 'Primary' } else { 'Secondary' }
            LanguageData = $script:AllLanguages[$_]
        }
    }
}

function Load-LanguageForModel {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Language,
        
        [Parameter(Mandatory=$true)]
        [string]$ModelName,
        
        [Parameter(Mandatory=$false)]
        [string]$CompilerPath = ""
    )

    if (-not $CompilerPath -or -not $CompilerPath.Trim()) {
        $CompilerPath = Join-Path (Get-RawrXDRoot) "compilers"
    }
    $CompilerPath = Resolve-RawrXDPath $CompilerPath
    
    # Verify language exists
    if (-not $script:AllLanguages.ContainsKey($Language)) {
        throw "Language '$Language' not found in registry"
    }
    
    $langData = $script:AllLanguages[$Language]
    $compilerId = "$Language-$ModelName"
    
    # Check if already loaded
    if ($script:CompilerCache.ContainsKey($compilerId)) {
        return $script:CompilerCache[$compilerId]
    }
    
    # Load compiler
    $compilerName = $langData.Compiler
    $compilerFile = Join-Path $CompilerPath "$compilerName.dll"
    
    if (Test-Path $compilerFile) {
        $compiler = @{
            Language = $Language
            Model = $ModelName
            CompilerFile = $compilerFile
            LoadedAt = Get-Date
            Features = $langData.Features
            Status = 'Loaded'
        }
        
        $script:CompilerCache[$compilerId] = $compiler
        $script:LanguageState[$compilerId] = $true
        
        return $compiler
    } else {
        Write-Warning "Compiler not found: $compilerFile"
        return $null
    }
}

function Unload-LanguageForModel {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Language,
        
        [Parameter(Mandatory=$true)]
        [string]$ModelName
    )
    
    $compilerId = "$Language-$ModelName"
    
    if ($script:CompilerCache.ContainsKey($compilerId)) {
        $script:CompilerCache.Remove($compilerId)
        $script:LanguageState.Remove($compilerId)
        return $true
    }
    
    return $false
}

function Get-LoadedLanguages {
    param(
        [Parameter(Mandatory=$false)]
        [string]$ModelName
    )
    
    $loaded = @()
    
    foreach ($key in $script:CompilerCache.Keys) {
        $compiler = $script:CompilerCache[$key]
        
        if ($ModelName -and $compiler.Model -ne $ModelName) {
            continue
        }
        
        $loaded += @{
            Language = $compiler.Language
            Model = $compiler.Model
            LoadedAt = $compiler.LoadedAt
            Features = $compiler.Features
        }
    }
    
    return $loaded
}

function Reset-AllLanguages {
    <#
    .SYNOPSIS
    Complete reset of all language states, loaded compilers, and cache
    Useful for cleanup, switching models, or troubleshooting
    #>
    
    $script:CompilerCache.Clear()
    $script:LanguageState.Clear()
    $script:ActiveLanguages.Clear()
    
    return @{
        Status = 'Reset Complete'
        CachesCleared = @('CompilerCache', 'LanguageState', 'ActiveLanguages')
        Timestamp = Get-Date
    }
}

function Reset-ModelLanguages {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModelName
    )
    
    <#
    .SYNOPSIS
    Reset languages for a specific model only
    #>
    
    $keysToRemove = @()
    
    foreach ($key in $script:CompilerCache.Keys) {
        if ($script:CompilerCache[$key].Model -eq $ModelName) {
            $keysToRemove += $key
        }
    }
    
    foreach ($key in $keysToRemove) {
        $script:CompilerCache.Remove($key)
        $script:LanguageState.Remove($key)
    }
    
    return @{
        Status = 'Model Languages Reset'
        Model = $ModelName
        LanguagesUnloaded = $keysToRemove.Count
        Timestamp = Get-Date
    }
}

function Reset-AllModels {
    <#
    .SYNOPSIS
    Complete reset of all models, languages, and states
    Fresh start for system reinitialization
    #>
    
    Reset-AllLanguages | Out-Null
    
    # Additional model cleanup
    $script:ModelLanguageMap.Clear()
    
    return @{
        Status = 'Full System Reset'
        Message = 'All models, languages, and states cleared'
        Timestamp = Get-Date
        NextStep = 'Reinitialize models and select languages'
    }
}

function Get-LanguageCompilerInfo {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Language
    )
    
    if (-not $script:AllLanguages.ContainsKey($Language)) {
        return $null
    }
    
    $langData = $script:AllLanguages[$Language]
    
    return @{
        Language = $Language
        Code = $langData.Code
        Category = $langData.Category
        Compiler = $langData.Compiler
        Aliases = $langData.Aliases
        Features = $langData.Features
        SupportedModels = $langData.Models
        OptimalTaskTypes = $langData.TaskTypes
    }
}

function Get-LanguageState {
    <#
    .SYNOPSIS
    Get complete state of all loaded languages and models
    #>
    
    return @{
        LoadedCompilers = $script:CompilerCache.Count
        ActiveLanguages = $script:ActiveLanguages.Count
        StateDetails = @{
            Cache = $script:CompilerCache
            ActiveLanguages = $script:ActiveLanguages
            LanguageState = $script:LanguageState
        }
        Timestamp = Get-Date
    }
}

function Initialize-LanguageForModel {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModelName,
        
        [Parameter(Mandatory=$true)]
        [string[]]$Languages,
        
        [Parameter(Mandatory=$false)]
        [string]$CompilerPath = ""
    )

    if (-not $CompilerPath -or -not $CompilerPath.Trim()) {
        $CompilerPath = Join-Path (Get-RawrXDRoot) "compilers"
    }
    $CompilerPath = Resolve-RawrXDPath $CompilerPath
    
    <#
    .SYNOPSIS
    Initialize multiple languages for a specific model
    #>
    
    $initialized = @()
    $failed = @()
    
    foreach ($lang in $Languages) {
        try {
            $result = Load-LanguageForModel -Language $lang -ModelName $ModelName -CompilerPath $CompilerPath
            if ($result) {
                $initialized += $lang
            } else {
                $failed += $lang
            }
        }
        catch {
            $failed += "$lang (Error: $($_.Exception.Message))"
        }
    }
    
    return @{
        Model = $ModelName
        SuccessfullyInitialized = $initialized
        Failed = $failed
        InitializedCount = $initialized.Count
        FailureCount = $failed.Count
        Timestamp = Get-Date
    }
}

# ============================================================================
# EXPORT FUNCTIONS
# ============================================================================

Export-ModuleMember -Function @(
    'Get-AllAvailableLanguages',
    'Get-LanguagesByCategory',
    'Get-LanguagesForModel',
    'Load-LanguageForModel',
    'Unload-LanguageForModel',
    'Get-LoadedLanguages',
    'Reset-AllLanguages',
    'Reset-ModelLanguages',
    'Reset-AllModels',
    'Get-LanguageCompilerInfo',
    'Get-LanguageState',
    'Initialize-LanguageForModel'
)
