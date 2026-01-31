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
}

TokenizerSelector::~TokenizerSelector()
{
}

void TokenizerSelector::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== Language & Tokenizer Selection =====
    QGroupBox* selectionGroup = new QGroupBox("Tokenizer Selection", this);
    QVBoxLayout* selectionLayout = new QVBoxLayout(selectionGroup);

    QHBoxLayout* languageLayout = new QHBoxLayout();
    languageLayout->addWidget(new QLabel("Language:"));
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem("English", static_cast<int>(Language::English));
    m_languageCombo->addItem("Chinese", static_cast<int>(Language::Chinese));
    m_languageCombo->addItem("Japanese", static_cast<int>(Language::Japanese));
    m_languageCombo->addItem("Multilingual", static_cast<int>(Language::Multilingual));
    m_languageCombo->addItem("Custom", static_cast<int>(Language::Custom));
    languageLayout->addWidget(m_languageCombo);
    selectionLayout->addLayout(languageLayout);

    QHBoxLayout* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("Tokenizer Type:"));
    m_tokenizerTypeCombo = new QComboBox(this);
    typeLayout->addWidget(m_tokenizerTypeCombo);
    selectionLayout->addLayout(typeLayout);

    mainLayout->addWidget(selectionGroup);

    // ===== Configuration Group =====
    QGroupBox* configGroup = new QGroupBox("Configuration", this);
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);

    QHBoxLayout* vocabLayout = new QHBoxLayout();
    vocabLayout->addWidget(new QLabel("Vocabulary Size:"));
    m_vocabSizeSpinBox = new QSpinBox(this);
    m_vocabSizeSpinBox->setMinimum(1000);
    m_vocabSizeSpinBox->setMaximum(1000000);
    m_vocabSizeSpinBox->setValue(30522);  // BERT default
    vocabLayout->addWidget(m_vocabSizeSpinBox);
    configLayout->addLayout(vocabLayout);

    QHBoxLayout* freqLayout = new QHBoxLayout();
    freqLayout->addWidget(new QLabel("Min Frequency:"));
    m_minFrequencySpinBox = new QSpinBox(this);
    m_minFrequencySpinBox->setMinimum(1);
    m_minFrequencySpinBox->setMaximum(100);
    m_minFrequencySpinBox->setValue(2);
    freqLayout->addWidget(m_minFrequencySpinBox);
    configLayout->addLayout(freqLayout);

    QHBoxLayout* charCoverageLayout = new QHBoxLayout();
    charCoverageLayout->addWidget(new QLabel("Character Coverage (for multilingual):"));
    m_characterCoverageLabel = new QLabel("0.9995");
    charCoverageLayout->addWidget(m_characterCoverageLabel);
    configLayout->addLayout(charCoverageLayout);

    m_lowercaseCheckBox = new QCheckBox("Lowercase Tokens", this);
    m_lowercaseCheckBox->setChecked(true);
    configLayout->addWidget(m_lowercaseCheckBox);

    m_addSpecialTokensCheckBox = new QCheckBox("Add Special Tokens", this);
    m_addSpecialTokensCheckBox->setChecked(true);
    configLayout->addWidget(m_addSpecialTokensCheckBox);

    QLabel* specialTokensLabel = new QLabel("Special Tokens JSON:");
    configLayout->addWidget(specialTokensLabel);
    m_specialTokensEdit = new QTextEdit(this);
    m_specialTokensEdit->setMaximumHeight(100);
    m_specialTokensEdit->setText(R"({"cls": "[CLS]", "sep": "[SEP]", "pad": "[PAD]", "unk": "[UNK]"})");
    configLayout->addWidget(m_specialTokensEdit);

    QHBoxLayout* maxTokenLayout = new QHBoxLayout();
    maxTokenLayout->addWidget(new QLabel("Max Token Length:"));
    m_maxTokenLengthSpinBox = new QSpinBox(this);
    m_maxTokenLengthSpinBox->setMinimum(1);
    m_maxTokenLengthSpinBox->setMaximum(512);
    m_maxTokenLengthSpinBox->setValue(200);
    maxTokenLayout->addWidget(m_maxTokenLengthSpinBox);
    configLayout->addLayout(maxTokenLayout);

    m_subwordRegularizationCheckBox = new QCheckBox("Enable Subword Regularization", this);
    m_subwordRegularizationCheckBox->setChecked(false);
    configLayout->addWidget(m_subwordRegularizationCheckBox);

    mainLayout->addWidget(configGroup);

    // ===== Metrics Display =====
    QGroupBox* metricsGroup = new QGroupBox("Tokenizer Metrics", this);
    QVBoxLayout* metricsLayout = new QVBoxLayout(metricsGroup);
    m_metricsLabel = new QLabel("Vocabulary Size: 30522 | Encoding: utf-8", this);
    m_metricsLabel->setWordWrap(true);
    metricsLayout->addWidget(m_metricsLabel);
    mainLayout->addWidget(metricsGroup);

    // ===== Preview =====
    QGroupBox* previewGroup = new QGroupBox("Tokenization Preview", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    previewLayout->addWidget(new QLabel("Text to Tokenize:"));
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setMaximumHeight(60);
    m_previewEdit->setText("The quick brown fox jumps over the lazy dog.");
    previewLayout->addWidget(m_previewEdit);

    QPushButton* previewButton = new QPushButton("Preview Tokenization", this);
    previewLayout->addWidget(previewButton);

    previewLayout->addWidget(new QLabel("Tokens:"));
    m_tokensEdit = new QTextEdit(this);
    m_tokensEdit->setReadOnly(true);
    m_tokensEdit->setMaximumHeight(60);
    previewLayout->addWidget(m_tokensEdit);

    mainLayout->addWidget(previewGroup);

    // ===== Buttons =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("OK", this);
// Qt connect removed
    buttonLayout->addWidget(okButton);

    QPushButton* cancelButton = new QPushButton("Cancel", this);
// Qt connect removed
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
// Qt connect removed
        auto tokens = previewTokenization(text);
        std::string tokensStr;
        for (const auto& token : tokens) {
            tokensStr += token + " ";
        }
        m_tokensEdit->setText(tokensStr);
    });
}

void TokenizerSelector::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
            this, [this](int) { updateMetricsDisplay(); });
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
            }
            m_tokenizerTypeCombo->addItem(typeName, static_cast<int>(type));
        }
    }
}

void TokenizerSelector::updateMetricsDisplay()
{
    int vocabSize = m_vocabSizeSpinBox->value();
    std::string metricsText = std::string("Vocabulary Size: %1 | Encoding: utf-8 | "
                                  "Max Token Length: %2 | Lowercase: %3")
                             
                             )
                              ? "Yes" : "No");
    m_metricsLabel->setText(metricsText);
}

void TokenizerSelector::onLanguageChanged(int index)
{
    updateAvailableTokenizers();
    m_config.language = static_cast<Language>(m_languageCombo->currentData().toInt());
    configurationChanged(m_config);
}

void TokenizerSelector::onTokenizerTypeChanged(int index)
{
    m_config.tokenizerType = static_cast<TokenizerType>(m_tokenizerTypeCombo->currentData().toInt());
    configurationChanged(m_config);
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
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        tokenizeError("Failed to open file: " + filePath);
        return false;
    }

    void* doc = void*::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        tokenizeError("Invalid tokenizer file format");
        return false;
    }

    return fromJson(doc.object());
}

bool TokenizerSelector::saveTokenizer(const std::string& filePath) const
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    void* doc(toJson());
    file.write(doc.toJson(void*::Indented));
    file.close();
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

        if (!cleanWord.isEmpty()) {
            tokens.push_back(cleanWord);
        }
    }

    return tokens;
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
}

bool TokenizerSelector::fromJson(const void*& config)
{
    setConfiguration(m_config);
    return true;
}

void TokenizerSelector::accept()
{
    m_config = getConfiguration();
    tokenizerSelected(m_config);
    void::accept();
}

