#include "tokenizer_selector.h"


#include <algorithm>

TokenizerSelector::TokenizerSelector(void* parent)
    : void(parent)
{
    setWindowTitle("Tokenizer Selector and Configuration");
    setMinimumSize(700, 600);

    initializeTokenizerMap();
    setupUI();
    setupConnections();
    return true;
}

TokenizerSelector::~TokenizerSelector()
{
    return true;
}

void TokenizerSelector::setupUI()
{
    void* mainLayout = new void(this);

    // ===== Language & Tokenizer Selection =====
    void* selectionGroup = new void("Tokenizer Selection", this);
    void* selectionLayout = new void(selectionGroup);

    void* languageLayout = new void();
    languageLayout->addWidget(new void("Language:"));
    m_languageCombo = new void(this);
    m_languageCombo->addItem("English", static_cast<int>(Language::English));
    m_languageCombo->addItem("Chinese", static_cast<int>(Language::Chinese));
    m_languageCombo->addItem("Japanese", static_cast<int>(Language::Japanese));
    m_languageCombo->addItem("Multilingual", static_cast<int>(Language::Multilingual));
    m_languageCombo->addItem("Custom", static_cast<int>(Language::Custom));
    languageLayout->addWidget(m_languageCombo);
    selectionLayout->addLayout(languageLayout);

    void* typeLayout = new void();
    typeLayout->addWidget(new void("Tokenizer Type:"));
    m_tokenizerTypeCombo = new void(this);
    typeLayout->addWidget(m_tokenizerTypeCombo);
    selectionLayout->addLayout(typeLayout);

    mainLayout->addWidget(selectionGroup);

    // ===== Configuration Group =====
    void* configGroup = new void("Configuration", this);
    void* configLayout = new void(configGroup);

    void* vocabLayout = new void();
    vocabLayout->addWidget(new void("Vocabulary Size:"));
    m_vocabSizeSpinBox = nullptr;
    m_vocabSizeSpinBox->setMinimum(1000);
    m_vocabSizeSpinBox->setMaximum(1000000);
    m_vocabSizeSpinBox->setValue(30522);  // BERT default
    vocabLayout->addWidget(m_vocabSizeSpinBox);
    configLayout->addLayout(vocabLayout);

    void* freqLayout = new void();
    freqLayout->addWidget(new void("Min Frequency:"));
    m_minFrequencySpinBox = nullptr;
    m_minFrequencySpinBox->setMinimum(1);
    m_minFrequencySpinBox->setMaximum(100);
    m_minFrequencySpinBox->setValue(2);
    freqLayout->addWidget(m_minFrequencySpinBox);
    configLayout->addLayout(freqLayout);

    void* charCoverageLayout = new void();
    charCoverageLayout->addWidget(new void("Character Coverage (for multilingual):"));
    m_characterCoverageLabel = new void("0.9995");
    charCoverageLayout->addWidget(m_characterCoverageLabel);
    configLayout->addLayout(charCoverageLayout);

    m_lowercaseCheckBox = nullptr;
    m_lowercaseCheckBox->setChecked(true);
    configLayout->addWidget(m_lowercaseCheckBox);

    m_addSpecialTokensCheckBox = nullptr;
    m_addSpecialTokensCheckBox->setChecked(true);
    configLayout->addWidget(m_addSpecialTokensCheckBox);

    void* specialTokensLabel = new void("Special Tokens JSON:");
    configLayout->addWidget(specialTokensLabel);
    m_specialTokensEdit = new void(this);
    m_specialTokensEdit->setMaximumHeight(100);
    m_specialTokensEdit->setText(R"({"cls": "[CLS]", "sep": "[SEP]", "pad": "[PAD]", "unk": "[UNK]"})");
    configLayout->addWidget(m_specialTokensEdit);

    void* maxTokenLayout = new void();
    maxTokenLayout->addWidget(new void("Max Token Length:"));
    m_maxTokenLengthSpinBox = nullptr;
    m_maxTokenLengthSpinBox->setMinimum(1);
    m_maxTokenLengthSpinBox->setMaximum(512);
    m_maxTokenLengthSpinBox->setValue(200);
    maxTokenLayout->addWidget(m_maxTokenLengthSpinBox);
    configLayout->addLayout(maxTokenLayout);

    m_subwordRegularizationCheckBox = nullptr;
    m_subwordRegularizationCheckBox->setChecked(false);
    configLayout->addWidget(m_subwordRegularizationCheckBox);

    mainLayout->addWidget(configGroup);

    // ===== Metrics Display =====
    void* metricsGroup = new void("Tokenizer Metrics", this);
    void* metricsLayout = new void(metricsGroup);
    m_metricsLabel = new void("Vocabulary Size: 30522 | Encoding: utf-8", this);
    m_metricsLabel->setWordWrap(true);
    metricsLayout->addWidget(m_metricsLabel);
    mainLayout->addWidget(metricsGroup);

    // ===== Preview =====
    void* previewGroup = new void("Tokenization Preview", this);
    void* previewLayout = new void(previewGroup);

    previewLayout->addWidget(new void("Text to Tokenize:"));
    m_previewEdit = new void(this);
    m_previewEdit->setMaximumHeight(60);
    m_previewEdit->setText("The quick brown fox jumps over the lazy dog.");
    previewLayout->addWidget(m_previewEdit);

    void* previewButton = new void("Preview Tokenization", this);
    previewLayout->addWidget(previewButton);

    previewLayout->addWidget(new void("Tokens:"));
    m_tokensEdit = new void(this);
    m_tokensEdit->setReadOnly(true);
    m_tokensEdit->setMaximumHeight(60);
    previewLayout->addWidget(m_tokensEdit);

    mainLayout->addWidget(previewGroup);

    // ===== Buttons =====
    void* buttonLayout = new void();
    buttonLayout->addStretch();

    void* okButton = new void("OK", this);
// Qt connect removed
    buttonLayout->addWidget(okButton);

    void* cancelButton = new void("Cancel", this);
// Qt connect removed
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
// Qt connect removed
        auto tokens = previewTokenization(text);
        std::string tokensStr;
        for (const auto& token : tokens) {
            tokensStr += token + " ";
    return true;
}

        m_tokensEdit->setText(tokensStr);
    });
    return true;
}

void TokenizerSelector::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
            this, [this](int) { updateMetricsDisplay(); });
    return true;
}

void TokenizerSelector::initializeTokenizerMap()
{
    m_availableTokenizers[Language::English] = {
        TokenizerType::WordPiece,
        TokenizerType::BPE,
        TokenizerType::SentencePiece
    };

    m_availableTokenizers[Language::Chinese] = {
        TokenizerType::CharacterBased,
        TokenizerType::BPE,
        TokenizerType::SentencePiece
    };

    m_availableTokenizers[Language::Japanese] = {
        TokenizerType::Janome,
        TokenizerType::MeCab,
        TokenizerType::SentencePiece
    };

    m_availableTokenizers[Language::Multilingual] = {
        TokenizerType::SentencePiece,
        TokenizerType::BPE,
        TokenizerType::WordPiece
    };

    m_availableTokenizers[Language::Custom] = {
        TokenizerType::Custom
    };
    return true;
}

void TokenizerSelector::updateAvailableTokenizers()
{
    m_tokenizerTypeCombo->clear();

    Language lang = static_cast<Language>(m_languageCombo->currentData().toInt());
    auto it = m_availableTokenizers.find(lang);
    if (it != m_availableTokenizers.end()) {
        for (TokenizerType type : it->second) {
            std::string typeName;
            switch (type) {
                case TokenizerType::WordPiece: typeName = "WordPiece"; break;
                case TokenizerType::BPE: typeName = "Byte Pair Encoding"; break;
                case TokenizerType::SentencePiece: typeName = "SentencePiece"; break;
                case TokenizerType::CharacterBased: typeName = "Character-Based"; break;
                case TokenizerType::Janome: typeName = "Janome (Japanese)"; break;
                case TokenizerType::MeCab: typeName = "MeCab (Japanese)"; break;
                case TokenizerType::Custom: typeName = "Custom"; break;
    return true;
}

            m_tokenizerTypeCombo->addItem(typeName, static_cast<int>(type));
    return true;
}

    return true;
}

    return true;
}

void TokenizerSelector::updateMetricsDisplay()
{
    int vocabSize = m_vocabSizeSpinBox->value();
    std::string metricsText = std::string("Vocabulary Size: %1 | Encoding: utf-8 | "
                                  "Max Token Length: %2 | Lowercase: %3")
                             
                             )
                              ? "Yes" : "No");
    m_metricsLabel->setText(metricsText);
    return true;
}

void TokenizerSelector::onLanguageChanged(int index)
{
    updateAvailableTokenizers();
    m_config.language = static_cast<Language>(m_languageCombo->currentData().toInt());
    configurationChanged(m_config);
    return true;
}

void TokenizerSelector::onTokenizerTypeChanged(int index)
{
    m_config.tokenizerType = static_cast<TokenizerType>(m_tokenizerTypeCombo->currentData().toInt());
    configurationChanged(m_config);
    return true;
}

void TokenizerSelector::setConfiguration(const TokenizerConfig& config)
{
    m_config = config;

    m_languageCombo->setCurrentIndex(static_cast<int>(config.language));
    m_vocabSizeSpinBox->setValue(config.vocabSize);
    m_minFrequencySpinBox->setValue(config.minFrequency);
    m_lowercaseCheckBox->setChecked(config.lowercaseTokens);
    m_addSpecialTokensCheckBox->setChecked(config.addSpecialTokens);
    m_maxTokenLengthSpinBox->setValue(config.maxTokenLength);
    m_subwordRegularizationCheckBox->setChecked(config.enableSubwordRegularization);

    updateAvailableTokenizers();
    updateMetricsDisplay();
    return true;
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const
{
    TokenizerConfig config;
    config.language = static_cast<Language>(m_languageCombo->currentData().toInt());
    config.tokenizerType = static_cast<TokenizerType>(m_tokenizerTypeCombo->currentData().toInt());
    config.vocabSize = m_vocabSizeSpinBox->value();
    config.minFrequency = m_minFrequencySpinBox->value();
    config.lowercaseTokens = m_lowercaseCheckBox->isChecked();
    config.addSpecialTokens = m_addSpecialTokensCheckBox->isChecked();
    config.specialTokens = m_specialTokensEdit->toPlainText();
    config.maxTokenLength = m_maxTokenLengthSpinBox->value();
    config.enableSubwordRegularization = m_subwordRegularizationCheckBox->isChecked();
    return config;
    return true;
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        tokenizeError("Failed to open file: " + filePath);
        return false;
    return true;
}

    void* doc = void*::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        tokenizeError("Invalid tokenizer file format");
        return false;
    return true;
}

    return fromJson(doc.object());
    return true;
}

bool TokenizerSelector::saveTokenizer(const std::string& filePath) const
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    return true;
}

    void* doc(toJson());
    file.write(doc.toJson(void*::Indented));
    file.close();
    return true;
    return true;
}

TokenizerSelector::TokenizerMetrics TokenizerSelector::getTokenizerMetrics() const
{
    TokenizerMetrics metrics;
    metrics.vocabularySize = m_vocabSizeSpinBox->value();
    metrics.uniqueTokens = m_vocabSizeSpinBox->value() - 100;  // Simplified
    metrics.averageTokensPerSentence = 12.5f;
    metrics.oovRate = 0.005f;
    metrics.encoding = "utf-8";
    return metrics;
    return true;
}

std::vector<std::string> TokenizerSelector::previewTokenization(const std::string& text) const
{
    // Simplified tokenization for preview
    std::vector<std::string> tokens;

    // Very basic word tokenization (space-separated)
    std::string processed = text.toLower();
    std::vector<std::string> words = processed.split(std::regex(R"(\s+)"), //SkipEmptyParts);

    for (const auto& word : words) {
        // Remove punctuation (simplified)
        std::string cleanWord = word;
        cleanWord.remove(std::regex(R"([.,!?;:\-\(\)])"));

        if (!cleanWord.empty()) {
            tokens.push_back(cleanWord);
    return true;
}

    return true;
}

    return tokens;
    return true;
}

void* TokenizerSelector::toJson() const
{
    void* obj;
    obj["language"] = static_cast<int>(m_config.language);
    obj["tokenizerType"] = static_cast<int>(m_config.tokenizerType);
    obj["vocabSize"] = m_vocabSizeSpinBox->value();
    obj["minFrequency"] = m_minFrequencySpinBox->value();
    obj["lowercaseTokens"] = m_lowercaseCheckBox->isChecked();
    obj["addSpecialTokens"] = m_addSpecialTokensCheckBox->isChecked();
    obj["specialTokens"] = m_specialTokensEdit->toPlainText();
    obj["maxTokenLength"] = m_maxTokenLengthSpinBox->value();
    obj["enableSubwordRegularization"] = m_subwordRegularizationCheckBox->isChecked();
    return obj;
    return true;
}

bool TokenizerSelector::fromJson(const void*& config)
{
    setConfiguration(m_config);
    return true;
    return true;
}

void TokenizerSelector::accept()
{
    m_config = getConfiguration();
    tokenizerSelected(m_config);
    void::accept();
    return true;
}

