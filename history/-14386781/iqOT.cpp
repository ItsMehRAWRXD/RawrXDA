#include "tokenizer_selector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <algorithm>
#include <cctype>

// Constructor
TokenizerSelector::TokenizerSelector(QWidget* parent)
    : QDialog(parent),
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
    
    qDebug() << "[TokenizerSelector] Initializing tokenizer selector";
    setupUI();
    setupConnections();
}

// Destructor
TokenizerSelector::~TokenizerSelector()
{
    qDebug() << "[TokenizerSelector] Tokenizer selector destroyed";
}

void TokenizerSelector::setConfiguration(const TokenizerConfig& config)
{
    m_config = config;
    qDebug() << "[TokenizerSelector] Configuration set";
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const
{
    return m_config;
}

bool TokenizerSelector::loadTokenizer(const QString& filePath)
{
    qDebug() << "[TokenizerSelector] Loading tokenizer from" << filePath;
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCritical() << "[TokenizerSelector] Cannot open tokenizer file";
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qCritical() << "[TokenizerSelector] Invalid tokenizer file format";
            return false;
        }
        
        return fromJson(doc.object());
    }
    catch (const std::exception& e) {
        qCritical() << "[TokenizerSelector] Error loading tokenizer:" << e.what();
        return false;
    }
}

bool TokenizerSelector::saveTokenizer(const QString& filePath) const
{
    qDebug() << "[TokenizerSelector] Saving tokenizer to" << filePath;
    
    try {
        QJsonObject obj = const_cast<TokenizerSelector*>(this)->toJson();
        QJsonDocument doc(obj);
        
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qCritical() << "[TokenizerSelector] Cannot open file for writing";
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        qDebug() << "[TokenizerSelector] Tokenizer saved successfully";
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[TokenizerSelector] Error saving tokenizer:" << e.what();
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

std::vector<QString> TokenizerSelector::previewTokenization(const QString& text) const
{
    std::vector<QString> tokens;
    
    // Simple tokenization preview
    QString current;
    for (const QChar& c : text) {
        if (c.isSpace()) {
            if (!current.isEmpty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (c.isPunct()) {
            if (!current.isEmpty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(QString(c));
        } else {
            current += c;
        }
    }
    
    if (!current.isEmpty()) {
        tokens.push_back(current);
    }
    
    return tokens;
}

QJsonObject TokenizerSelector::toJson() const
{
    QJsonObject obj;
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

bool TokenizerSelector::fromJson(const QJsonObject& config)
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
        qCritical() << "[TokenizerSelector] Error loading from JSON:" << e.what();
        return false;
    }
}

void TokenizerSelector::accept()
{
    qDebug() << "[TokenizerSelector] Configuration accepted";
    emit tokenizerSelected(m_config);
    QDialog::accept();
}

void TokenizerSelector::setupUI()
{
    setWindowTitle("Tokenizer Selector");
    setMinimumWidth(700);
    setMinimumHeight(600);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Language selection
    QHBoxLayout* langLayout = new QHBoxLayout();
    QLabel* langLabel = new QLabel("Language:");
    m_languageCombo = new QComboBox();
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
    QHBoxLayout* tokLayout = new QHBoxLayout();
    QLabel* tokLabel = new QLabel("Tokenizer Type:");
    m_tokenizerTypeCombo = new QComboBox();
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
    QHBoxLayout* vocabLayout = new QHBoxLayout();
    QLabel* vocabLabel = new QLabel("Vocabulary Size:");
    m_vocabSizeSpinBox = new QSpinBox();
    m_vocabSizeSpinBox->setRange(1000, 1000000);
    m_vocabSizeSpinBox->setValue(30522);
    m_vocabSizeSpinBox->setSingleStep(1000);
    
    vocabLayout->addWidget(vocabLabel);
    vocabLayout->addWidget(m_vocabSizeSpinBox);
    vocabLayout->addStretch();
    mainLayout->addLayout(vocabLayout);
    
    // Min frequency
    QHBoxLayout* freqLayout = new QHBoxLayout();
    QLabel* freqLabel = new QLabel("Minimum Frequency:");
    m_minFrequencySpinBox = new QSpinBox();
    m_minFrequencySpinBox->setRange(1, 100);
    m_minFrequencySpinBox->setValue(2);
    
    freqLayout->addWidget(freqLabel);
    freqLayout->addWidget(m_minFrequencySpinBox);
    freqLayout->addStretch();
    mainLayout->addLayout(freqLayout);
    
    // Character coverage
    QHBoxLayout* coverageLayout = new QHBoxLayout();
    m_characterCoverageLabel = new QLabel("Character Coverage: 99.95%");
    
    QSpinBox* coverageSpinBox = new QSpinBox();
    coverageSpinBox->setRange(0, 100);
    coverageSpinBox->setValue(99);
    coverageSpinBox->setSuffix("%");
    
    connect(coverageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, coverageSpinBox]() {
                float val = coverageSpinBox->value() / 100.0f;
                m_characterCoverageLabel->setText(QString::asprintf("Character Coverage: %.2f%%", val * 100));
                m_config.characterCoverage = val;
            });
    
    coverageLayout->addWidget(m_characterCoverageLabel);
    coverageLayout->addWidget(coverageSpinBox);
    coverageLayout->addStretch();
    mainLayout->addLayout(coverageLayout);
    
    // Special options
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    m_lowercaseCheckBox = new QCheckBox("Lowercase");
    m_lowercaseCheckBox->setChecked(true);
    m_addSpecialTokensCheckBox = new QCheckBox("Add Special Tokens");
    m_addSpecialTokensCheckBox->setChecked(true);
    
    optionsLayout->addWidget(m_lowercaseCheckBox);
    optionsLayout->addWidget(m_addSpecialTokensCheckBox);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);
    
    // Special tokens
    QHBoxLayout* specialLayout = new QHBoxLayout();
    QLabel* specialLabel = new QLabel("Special Tokens (JSON):");
    specialLayout->addWidget(specialLabel);
    mainLayout->addLayout(specialLayout);
    
    m_specialTokensEdit = new QTextEdit();
    m_specialTokensEdit->setPlainText(R"({"cls": "[CLS]", "sep": "[SEP]", "pad": "[PAD]", "unk": "[UNK]"})");
    m_specialTokensEdit->setMaximumHeight(80);
    mainLayout->addWidget(m_specialTokensEdit);
    
    // Max token length
    QHBoxLayout* maxLenLayout = new QHBoxLayout();
    QLabel* maxLenLabel = new QLabel("Max Token Length:");
    m_maxTokenLengthSpinBox = new QSpinBox();
    m_maxTokenLengthSpinBox->setRange(10, 500);
    m_maxTokenLengthSpinBox->setValue(200);
    
    maxLenLayout->addWidget(maxLenLabel);
    maxLenLayout->addWidget(m_maxTokenLengthSpinBox);
    maxLenLayout->addStretch();
    mainLayout->addLayout(maxLenLayout);
    
    // Subword regularization
    m_subwordRegularizationCheckBox = new QCheckBox("Enable Subword Regularization");
    mainLayout->addWidget(m_subwordRegularizationCheckBox);
    
    // Metrics display
    m_metricsLabel = new QLabel("Metrics: Ready");
    mainLayout->addWidget(m_metricsLabel);
    
    // Preview section
    QLabel* previewLabel = new QLabel("Tokenization Preview:");
    mainLayout->addWidget(previewLabel);
    
    m_previewEdit = new QTextEdit();
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(100);
    mainLayout->addWidget(m_previewEdit);
    
    m_tokensEdit = new QTextEdit();
    m_tokensEdit->setPlaceholderText("Enter text to preview tokenization...");
    m_tokensEdit->setMaximumHeight(80);
    mainLayout->addWidget(m_tokensEdit);
    
    // Preview button
    QPushButton* previewBtn = new QPushButton("Preview Tokenization");
    connect(previewBtn, &QPushButton::clicked, [this]() {
        QString text = m_tokensEdit->toPlainText();
        std::vector<QString> tokens = previewTokenization(text);
        
        QString preview;
        for (size_t i = 0; i < tokens.size(); ++i) {
            preview += QString::number(i) + ": " + tokens[i] + "\n";
        }
        
        m_previewEdit->setText(preview);
    });
    mainLayout->addWidget(previewBtn);
    
    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &TokenizerSelector::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    setLayout(mainLayout);
}

void TokenizerSelector::setupConnections()
{
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TokenizerSelector::onLanguageChanged);
    
    connect(m_tokenizerTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TokenizerSelector::onTokenizerTypeChanged);
    
    connect(m_vocabSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) { m_config.vocabSize = val; });
    
    connect(m_minFrequencySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) { m_config.minFrequency = val; });
    
    connect(m_lowercaseCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) { m_config.lowercaseTokens = checked; });
    
    connect(m_addSpecialTokensCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) { m_config.addSpecialTokens = checked; });
    
    connect(m_maxTokenLengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) { m_config.maxTokenLength = val; });
    
    connect(m_subwordRegularizationCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) { m_config.enableSubwordRegularization = checked; });
}

void TokenizerSelector::updateAvailableTokenizers()
{
    qDebug() << "[TokenizerSelector] Updating available tokenizers";
}

void TokenizerSelector::updateMetricsDisplay()
{
    TokenizerMetrics metrics = getTokenizerMetrics();
    QString metricsStr = QString("Vocab: %1 | Tokens: %2 | OOV: %3% | Encoding: %4")
                            .arg(metrics.vocabularySize)
                            .arg(metrics.uniqueTokens)
                            .arg(static_cast<int>(metrics.oovRate * 100))
                            .arg(metrics.encoding);
    m_metricsLabel->setText(metricsStr);
}

void TokenizerSelector::onLanguageChanged(int index)
{
    m_config.language = static_cast<Language>(index);
    qDebug() << "[TokenizerSelector] Language changed to" << index;
    updateAvailableTokenizers();
}

void TokenizerSelector::onTokenizerTypeChanged(int index)
{
    m_config.tokenizerType = static_cast<TokenizerType>(index);
    qDebug() << "[TokenizerSelector] Tokenizer type changed to" << index;
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

#include "moc_tokenizer_selector.cpp"
