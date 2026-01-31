#include "tokenizer_selector.h"


#include <algorithm>
#include <cctype>

// Constructor
TokenizerSelector::TokenizerSelector(void* parent)
    : void(parent),
      m_languageCombo(nullptr),
      m_tokenizerTypeCombo(nullptr),
      m_vocabSizeSpinBox(nullptr),
      m_minFrequencySpinBox(nullptr),
      m_characterCoverageLabel(nullptr),
      m_lowercaseCheckBox(nullptr),
      m_addSpecialTokensCheckBox(nullptr),
      m_specialTokensEdit(nullptr),
      m_maxTokenLengthSpinBox(nullptr),
      m_subwordRegularizationCheckBox(nullptr),
      m_metricsLabel(nullptr),
      m_previewEdit(nullptr),
      m_tokensEdit(nullptr)
{
    // Lightweight constructor - defer Qt widget creation
    
    // Initialize default config
    m_config.language = Language::English;
    m_config.tokenizerType = TokenizerType::WordPiece;
    m_config.name = "Default";
    m_config.vocabSize = 30522;
    m_config.minFrequency = 2;
    m_config.characterCoverage = 0.9995f;
    m_config.lowercaseTokens = true;
    m_config.addSpecialTokens = true;
    m_config.specialTokens = R"({"cls": "[CLS]", "sep": "[SEP]", "pad": "[PAD]", "unk": "[UNK]"})";
    m_config.maxTokenLength = 200;
    m_config.enableSubwordRegularization = false;
    m_config.subwordRegularizationAlpha = 0.1f;
    
    initializeTokenizerMap();
}

void TokenizerSelector::initialize() {
    if (m_languageCombo) return;  // Already initialized
    
    setupUI();
    setupConnections();
}

// Destructor
TokenizerSelector::~TokenizerSelector()
{
}

void TokenizerSelector::setConfiguration(const TokenizerConfig& config)
{
    m_config = config;
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const
{
    return m_config;
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath)
{
    
    try {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        std::vector<uint8_t> data = file.readAll();
        file.close();
        
        void* doc = void*::fromJson(data);
        if (!doc.isObject()) {
            return false;
        }
        
        return fromJson(doc.object());
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool TokenizerSelector::saveTokenizer(const std::string& filePath) const
{
    
    try {
        void* obj = const_cast<TokenizerSelector*>(this)->toJson();
        void* doc(obj);
        
        std::fstream file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

TokenizerSelector::TokenizerMetrics TokenizerSelector::getTokenizerMetrics() const
{
    TokenizerMetrics metrics;
    metrics.vocabularySize = m_config.vocabSize;
    metrics.uniqueTokens = m_config.vocabSize;
    metrics.averageTokensPerSentence = 15.5f;
    metrics.oovRate = 0.02f;
    metrics.encoding = "utf-8";
    return metrics;
}

std::vector<std::string> TokenizerSelector::previewTokenization(const std::string& text) const
{
    std::vector<std::string> tokens;
    
    // Simple tokenization preview
    std::string current;
    for (const QChar& c : text) {
        if (c.isSpace()) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (c.isPunct()) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(c));
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        tokens.push_back(current);
    }
    
    return tokens;
}

void* TokenizerSelector::toJson() const
{
    void* obj;
    obj["language"] = static_cast<int>(m_config.language);
    obj["tokenizerType"] = static_cast<int>(m_config.tokenizerType);
    obj["name"] = m_config.name;
    obj["vocabSize"] = m_config.vocabSize;
    obj["minFrequency"] = m_config.minFrequency;
    obj["characterCoverage"] = m_config.characterCoverage;
    obj["lowercaseTokens"] = m_config.lowercaseTokens;
    obj["addSpecialTokens"] = m_config.addSpecialTokens;
    obj["specialTokens"] = m_config.specialTokens;
    obj["maxTokenLength"] = m_config.maxTokenLength;
    obj["enableSubwordRegularization"] = m_config.enableSubwordRegularization;
    obj["subwordRegularizationAlpha"] = m_config.subwordRegularizationAlpha;
    return obj;
}

bool TokenizerSelector::fromJson(const void*& config)
{
    try {
        m_config.language = static_cast<Language>(config["language"].toInt());
        m_config.tokenizerType = static_cast<TokenizerType>(config["tokenizerType"].toInt());
        m_config.name = config["name"].toString();
        m_config.vocabSize = config["vocabSize"].toInt();
        m_config.minFrequency = config["minFrequency"].toInt();
        m_config.characterCoverage = static_cast<float>(config["characterCoverage"].toDouble());
        m_config.lowercaseTokens = config["lowercaseTokens"].toBool();
        m_config.addSpecialTokens = config["addSpecialTokens"].toBool();
        m_config.specialTokens = config["specialTokens"].toString();
        m_config.maxTokenLength = config["maxTokenLength"].toInt();
        m_config.enableSubwordRegularization = config["enableSubwordRegularization"].toBool();
        m_config.subwordRegularizationAlpha = static_cast<float>(config["subwordRegularizationAlpha"].toDouble());
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

void TokenizerSelector::accept()
{
    tokenizerSelected(m_config);
    void::accept();
}

void TokenizerSelector::setupUI()
{
    setWindowTitle("Tokenizer Selector");
    setMinimumWidth(700);
    setMinimumHeight(600);
    
    void* mainLayout = new void(this);
    
    // Language selection
    void* langLayout = new void();
    void* langLabel = new void("Language:");
    m_languageCombo = new void();
    m_languageCombo->addItem("English", static_cast<int>(Language::English));
    m_languageCombo->addItem("Chinese", static_cast<int>(Language::Chinese));
    m_languageCombo->addItem("Japanese", static_cast<int>(Language::Japanese));
    m_languageCombo->addItem("Multilingual", static_cast<int>(Language::Multilingual));
    m_languageCombo->addItem("Custom", static_cast<int>(Language::Custom));
    
    langLayout->addWidget(langLabel);
    langLayout->addWidget(m_languageCombo);
    langLayout->addStretch();
    mainLayout->addLayout(langLayout);
    
    // Tokenizer type selection
    void* tokLayout = new void();
    void* tokLabel = new void("Tokenizer Type:");
    m_tokenizerTypeCombo = new void();
    m_tokenizerTypeCombo->addItem("WordPiece (BERT)", static_cast<int>(TokenizerType::WordPiece));
    m_tokenizerTypeCombo->addItem("BPE (GPT)", static_cast<int>(TokenizerType::BPE));
    m_tokenizerTypeCombo->addItem("SentencePiece", static_cast<int>(TokenizerType::SentencePiece));
    m_tokenizerTypeCombo->addItem("Character-based", static_cast<int>(TokenizerType::CharacterBased));
    m_tokenizerTypeCombo->addItem("Janome", static_cast<int>(TokenizerType::Janome));
    m_tokenizerTypeCombo->addItem("MeCab", static_cast<int>(TokenizerType::MeCab));
    m_tokenizerTypeCombo->addItem("Custom", static_cast<int>(TokenizerType::Custom));
    
    tokLayout->addWidget(tokLabel);
    tokLayout->addWidget(m_tokenizerTypeCombo);
    tokLayout->addStretch();
    mainLayout->addLayout(tokLayout);
    
    // Vocabulary size
    void* vocabLayout = new void();
    void* vocabLabel = new void("Vocabulary Size:");
    m_vocabSizeSpinBox = nullptr;
    m_vocabSizeSpinBox->setRange(1000, 1000000);
    m_vocabSizeSpinBox->setValue(30522);
    m_vocabSizeSpinBox->setSingleStep(1000);
    
    vocabLayout->addWidget(vocabLabel);
    vocabLayout->addWidget(m_vocabSizeSpinBox);
    vocabLayout->addStretch();
    mainLayout->addLayout(vocabLayout);
    
    // Min frequency
    void* freqLayout = new void();
    void* freqLabel = new void("Minimum Frequency:");
    m_minFrequencySpinBox = nullptr;
    m_minFrequencySpinBox->setRange(1, 100);
    m_minFrequencySpinBox->setValue(2);
    
    freqLayout->addWidget(freqLabel);
    freqLayout->addWidget(m_minFrequencySpinBox);
    freqLayout->addStretch();
    mainLayout->addLayout(freqLayout);
    
    // Character coverage
    void* coverageLayout = new void();
    m_characterCoverageLabel = new void("Character Coverage: 99.95%");
    
    void* coverageSpinBox = nullptr;
    coverageSpinBox->setRange(0, 100);
    coverageSpinBox->setValue(99);
    coverageSpinBox->setSuffix("%");
// Qt connect removed
                m_characterCoverageLabel->setText(std::string::asprintf("Character Coverage: %.2f%%", val * 100));
                m_config.characterCoverage = val;
            });
    
    coverageLayout->addWidget(m_characterCoverageLabel);
    coverageLayout->addWidget(coverageSpinBox);
    coverageLayout->addStretch();
    mainLayout->addLayout(coverageLayout);
    
    // Special options
    void* optionsLayout = new void();
    m_lowercaseCheckBox = nullptr;
    m_lowercaseCheckBox->setChecked(true);
    m_addSpecialTokensCheckBox = nullptr;
    m_addSpecialTokensCheckBox->setChecked(true);
    
    optionsLayout->addWidget(m_lowercaseCheckBox);
    optionsLayout->addWidget(m_addSpecialTokensCheckBox);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);
    
    // Special tokens
    void* specialLayout = new void();
    void* specialLabel = new void("Special Tokens (JSON):");
    specialLayout->addWidget(specialLabel);
    mainLayout->addLayout(specialLayout);
    
    m_specialTokensEdit = new void();
    m_specialTokensEdit->setPlainText(R"({"cls": "[CLS]", "sep": "[SEP]", "pad": "[PAD]", "unk": "[UNK]"})");
    m_specialTokensEdit->setMaximumHeight(80);
    mainLayout->addWidget(m_specialTokensEdit);
    
    // Max token length
    void* maxLenLayout = new void();
    void* maxLenLabel = new void("Max Token Length:");
    m_maxTokenLengthSpinBox = nullptr;
    m_maxTokenLengthSpinBox->setRange(10, 500);
    m_maxTokenLengthSpinBox->setValue(200);
    
    maxLenLayout->addWidget(maxLenLabel);
    maxLenLayout->addWidget(m_maxTokenLengthSpinBox);
    maxLenLayout->addStretch();
    mainLayout->addLayout(maxLenLayout);
    
    // Subword regularization
    m_subwordRegularizationCheckBox = nullptr;
    mainLayout->addWidget(m_subwordRegularizationCheckBox);
    
    // Metrics display
    m_metricsLabel = new void("Metrics: Ready");
    mainLayout->addWidget(m_metricsLabel);
    
    // Preview section
    void* previewLabel = new void("Tokenization Preview:");
    mainLayout->addWidget(previewLabel);
    
    m_previewEdit = new void();
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(100);
    mainLayout->addWidget(m_previewEdit);
    
    m_tokensEdit = new void();
    m_tokensEdit->setPlaceholderText("Enter text to preview tokenization...");
    m_tokensEdit->setMaximumHeight(80);
    mainLayout->addWidget(m_tokensEdit);
    
    // Preview button
    void* previewBtn = new void("Preview Tokenization");
// Qt connect removed
        std::vector<std::string> tokens = previewTokenization(text);
        
        std::string preview;
        for (size_t i = 0; i < tokens.size(); ++i) {
            preview += std::string::number(i) + ": " + tokens[i] + "\n";
        }
        
        m_previewEdit->setText(preview);
    });
    mainLayout->addWidget(previewBtn);
    
    // Buttons
    QDialogButtonBox* buttonBox = nullptr;
// Qt connect removed
// Qt connect removed
    mainLayout->addWidget(buttonBox);
    
    setLayout(mainLayout);
}

void TokenizerSelector::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
            this, [this](int val) { m_config.vocabSize = val; });
// Qt connect removed
            this, [this](int val) { m_config.minFrequency = val; });
// Qt connect removed
            this, [this](bool checked) { m_config.lowercaseTokens = checked; });
// Qt connect removed
            this, [this](bool checked) { m_config.addSpecialTokens = checked; });
// Qt connect removed
            this, [this](int val) { m_config.maxTokenLength = val; });
// Qt connect removed
            this, [this](bool checked) { m_config.enableSubwordRegularization = checked; });
}

void TokenizerSelector::updateAvailableTokenizers()
{
}

void TokenizerSelector::updateMetricsDisplay()
{
    TokenizerMetrics metrics = getTokenizerMetrics();
    std::string metricsStr = std::string("Vocab: %1 | Tokens: %2 | OOV: %3% | Encoding: %4")


                            )
                            ;
    m_metricsLabel->setText(metricsStr);
}

void TokenizerSelector::onLanguageChanged(int index)
{
    m_config.language = static_cast<Language>(index);
    updateAvailableTokenizers();
}

void TokenizerSelector::onTokenizerTypeChanged(int index)
{
    m_config.tokenizerType = static_cast<TokenizerType>(index);
}

void TokenizerSelector::initializeTokenizerMap()
{
    // English tokenizers
    m_availableTokenizers[Language::English] = {
        TokenizerType::WordPiece,
        TokenizerType::BPE,
        TokenizerType::SentencePiece
    };
    
    // Chinese tokenizers
    m_availableTokenizers[Language::Chinese] = {
        TokenizerType::CharacterBased,
        TokenizerType::SentencePiece
    };
    
    // Japanese tokenizers
    m_availableTokenizers[Language::Japanese] = {
        TokenizerType::Janome,
        TokenizerType::MeCab,
        TokenizerType::SentencePiece
    };
    
    // Multilingual tokenizers
    m_availableTokenizers[Language::Multilingual] = {
        TokenizerType::SentencePiece,
        TokenizerType::BPE
    };
    
    // Custom tokenizers
    m_availableTokenizers[Language::Custom] = {
        TokenizerType::Custom
    };
}


