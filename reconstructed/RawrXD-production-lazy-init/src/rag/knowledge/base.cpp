/**
 * @file rag_knowledge_base.cpp
 * @brief Retrieval-Augmented Generation (RAG) knowledge base implementation
 */

#include "rag_knowledge_base.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <chrono>

// ============================================================================
// DocumentVector Implementation
// ============================================================================

QJsonObject DocumentVector::toJson() const
{
    QJsonObject json;
    json["documentId"] = documentId;
    json["documentTitle"] = documentTitle;
    json["documentSource"] = documentSource;
    json["chunkId"] = chunkId;
    json["chunkIndex"] = chunkIndex;
    json["startOffset"] = startOffset;
    json["endOffset"] = endOffset;
    json["content"] = content;
    json["createdTime"] = createdTime.toString(Qt::ISODate);
    json["lastModifiedTime"] = lastModifiedTime.toString(Qt::ISODate);
    json["confidence"] = confidence;
    json["metadata"] = metadata;
    
    QJsonArray embeddingArray;
    for (float value : embedding) {
        embeddingArray.append(value);
    }
    json["embedding"] = embeddingArray;
    
    return json;
}

DocumentVector DocumentVector::fromJson(const QJsonObject& json)
{
    DocumentVector dv;
    dv.documentId = json["documentId"].toString();
    dv.documentTitle = json["documentTitle"].toString();
    dv.documentSource = json["documentSource"].toString();
    dv.chunkId = json["chunkId"].toString();
    dv.chunkIndex = json["chunkIndex"].toInt();
    dv.startOffset = json["startOffset"].toInt();
    dv.endOffset = json["endOffset"].toInt();
    dv.content = json["content"].toString();
    dv.createdTime = QDateTime::fromString(json["createdTime"].toString(), Qt::ISODate);
    dv.lastModifiedTime = QDateTime::fromString(json["lastModifiedTime"].toString(), Qt::ISODate);
    dv.confidence = json["confidence"].toDouble(1.0);
    dv.metadata = json["metadata"].toObject();
    
    QJsonArray embeddingArray = json["embedding"].toArray();
    for (const auto& value : embeddingArray) {
        dv.embedding.push_back(value.toDouble());
    }
    
    return dv;
}

// ============================================================================
// VectorDatabase Implementation
// ============================================================================

VectorDatabase::VectorDatabase()
{
}

void VectorDatabase::addVector(const DocumentVector& vector)
{
    QMutexLocker locker(&m_mutex);
    m_vectors.push_back(vector);
    qInfo() << "[VectorDatabase] Added vector:" << vector.chunkId << "(" << vector.embedding.size() << "dimensions)";
}

bool VectorDatabase::removeVector(const QString& vectorId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = std::find_if(m_vectors.begin(), m_vectors.end(),
        [&vectorId](const DocumentVector& dv) { return dv.chunkId == vectorId; });
    
    if (it != m_vectors.end()) {
        m_vectors.erase(it);
        return true;
    }
    
    return false;
}

void VectorDatabase::clear()
{
    QMutexLocker locker(&m_mutex);
    m_vectors.clear();
}

double VectorDatabase::cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2)
{
    if (v1.empty() || v2.empty()) return 0.0;
    if (v1.size() != v2.size()) return 0.0;
    
    float dotProduct = 0.0f;
    float mag1 = 0.0f;
    float mag2 = 0.0f;
    
    for (size_t i = 0; i < v1.size(); ++i) {
        dotProduct += v1[i] * v2[i];
        mag1 += v1[i] * v1[i];
        mag2 += v2[i] * v2[i];
    }
    
    mag1 = std::sqrt(mag1);
    mag2 = std::sqrt(mag2);
    
    if (mag1 < std::numeric_limits<float>::epsilon() || mag2 < std::numeric_limits<float>::epsilon()) {
        return 0.0;
    }
    
    return static_cast<double>(dotProduct / (mag1 * mag2));
}

float VectorDatabase::magnitude(const std::vector<float>& v)
{
    float sum = 0.0f;
    for (float value : v) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

std::vector<DocumentVector> VectorDatabase::searchSimilar(
    const std::vector<float>& queryVector,
    int topK,
    double minSimilarity)
{
    QMutexLocker locker(&m_mutex);
    
    std::vector<std::pair<DocumentVector, double>> results;
    
    for (const auto& dv : m_vectors) {
        double similarity = cosineSimilarity(queryVector, dv.embedding);
        if (similarity >= minSimilarity) {
            results.emplace_back(dv, similarity);
        }
    }
    
    // Sort by similarity (descending)
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return top K results
    std::vector<DocumentVector> topResults;
    for (int i = 0; i < std::min(topK, (int)results.size()); ++i) {
        results[i].first.confidence = results[i].second;
        topResults.push_back(results[i].first);
    }
    
    return topResults;
}

DocumentVector VectorDatabase::getVector(const QString& vectorId) const
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& dv : m_vectors) {
        if (dv.chunkId == vectorId) {
            return dv;
        }
    }
    
    return DocumentVector();
}

std::vector<DocumentVector> VectorDatabase::getAllVectors() const
{
    QMutexLocker locker(&m_mutex);
    return m_vectors;
}

long long VectorDatabase::getIndexSize() const
{
    QMutexLocker locker(&m_mutex);
    
    long long size = 0;
    for (const auto& dv : m_vectors) {
        size += dv.embedding.size() * sizeof(float);
        size += dv.content.length();
        size += dv.documentId.length();
        size += dv.chunkId.length();
    }
    
    return size;
}

// ============================================================================
// DocumentChunker Implementation
// ============================================================================

DocumentChunker::DocumentChunker(const ChunkConfig& config)
    : m_config(config)
{
}

std::vector<DocumentVector> DocumentChunker::chunkDocument(
    const QString& documentId,
    const QString& title,
    const QString& content,
    const QString& source)
{
    std::vector<DocumentVector> chunks;
    std::vector<QString> textChunks;
    
    // Apply appropriate chunking strategy
    switch (m_config.strategy) {
        case SENTENCE_BASED:
            textChunks = splitSentences(content);
            break;
        case PARAGRAPH_BASED:
            textChunks = splitParagraphs(content);
            break;
        case SLIDING_WINDOW:
            textChunks = applySlidingWindow(content);
            break;
        case SEMANTIC_SPLIT:
            // For now, fallback to sliding window
            textChunks = applySlidingWindow(content);
            break;
    }
    
    // Create DocumentVector for each chunk
    int startOffset = 0;
    for (int i = 0; i < (int)textChunks.size(); ++i) {
        const QString& chunkText = textChunks[i];
        
        if (chunkText.length() < m_config.minChunkSize) {
            // Skip too small chunks
            continue;
        }
        
        DocumentVector dv;
        dv.documentId = documentId;
        dv.documentTitle = title;
        dv.documentSource = source;
        dv.chunkId = QString("%1_chunk_%2").arg(documentId).arg(i);
        dv.chunkIndex = chunks.size();
        dv.content = chunkText;
        dv.startOffset = startOffset;
        dv.endOffset = startOffset + chunkText.length();
        dv.createdTime = QDateTime::currentDateTime();
        dv.lastModifiedTime = QDateTime::currentDateTime();
        
        // Embedding will be set by RAGKnowledgeBase
        
        chunks.push_back(dv);
        startOffset = dv.endOffset - m_config.overlapSize;
    }
    
    qInfo() << "[DocumentChunker] Chunked document" << documentId << "into" << chunks.size() << "chunks";
    
    return chunks;
}

std::vector<QString> DocumentChunker::splitSentences(const QString& text)
{
    std::vector<QString> sentences;
    QString current;
    
    for (int i = 0; i < text.length(); ++i) {
        current += text[i];
        
        if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && 
            i + 1 < text.length() && text[i + 1] == ' ') {
            
            if (current.length() >= m_config.minChunkSize) {
                sentences.push_back(current.trimmed());
                current.clear();
            }
        }
    }
    
    if (!current.isEmpty()) {
        sentences.push_back(current.trimmed());
    }
    
    return sentences;
}

std::vector<QString> DocumentChunker::splitParagraphs(const QString& text)
{
    std::vector<QString> paragraphs;
    QStringList parts = text.split("\n\n", Qt::SkipEmptyParts);
    
    for (const auto& part : parts) {
        if (part.trimmed().length() >= m_config.minChunkSize) {
            paragraphs.push_back(part.trimmed());
        }
    }
    
    return paragraphs;
}

std::vector<QString> DocumentChunker::applySlidingWindow(const QString& text)
{
    std::vector<QString> chunks;
    
    for (int i = 0; i < text.length(); i += (m_config.maxChunkSize - m_config.overlapSize)) {
        int endIdx = std::min(i + m_config.maxChunkSize, (int)text.length());
        QString chunk = text.mid(i, endIdx - i).trimmed();
        
        if (chunk.length() >= m_config.minChunkSize) {
            chunks.push_back(chunk);
        }
    }
    
    return chunks;
}

// ============================================================================
// SimpleEmbeddingModel Implementation
// ============================================================================

SimpleEmbeddingModel::SimpleEmbeddingModel(int dimension)
    : m_dimension(dimension)
{
}

std::vector<float> SimpleEmbeddingModel::embed(const QString& text)
{
    return hashToVector(text, m_dimension);
}

std::vector<std::vector<float>> SimpleEmbeddingModel::embedBatch(const QStringList& texts)
{
    std::vector<std::vector<float>> result;
    for (const auto& text : texts) {
        result.push_back(embed(text));
    }
    return result;
}

std::vector<float> SimpleEmbeddingModel::hashToVector(const QString& text, int dimension)
{
    std::vector<float> vector(dimension, 0.0f);
    
    // Simple hash-based embedding: use character codes to seed random values
    uint32_t seed = 0;
    for (const auto& c : text) {
        seed = seed * 31 + c.unicode();
    }
    
    // Generate deterministic pseudo-random values
    for (int i = 0; i < dimension; ++i) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        vector[i] = (float)seed / 0x7fffffff - 0.5f;
    }
    
    // Normalize vector
    float mag = 0.0f;
    for (float v : vector) {
        mag += v * v;
    }
    mag = std::sqrt(mag);
    
    if (mag > 0.001f) {
        for (auto& v : vector) {
            v /= mag;
        }
    }
    
    return vector;
}

// ============================================================================
// RetrievalRanker Implementation
// ============================================================================

RetrievalRanker::RetrievalRanker(const RankingStrategy& strategy)
    : m_strategy(strategy)
{
}

void RetrievalRanker::rankDocuments(std::vector<DocumentVector>& documents) const
{
    switch (m_strategy.type) {
        case Type::SIMILARITY_ONLY:
            // Already ranked by similarity
            break;
            
        case Type::RECENCY_BOOSTED: {
            for (auto& doc : documents) {
                double recencyScore = getRecencyScore(doc.lastModifiedTime);
                doc.confidence = doc.confidence * 0.7 + recencyScore * 0.3;
            }
            break;
        }
            
        case Type::HYBRID: {
            for (auto& doc : documents) {
                double recencyScore = getRecencyScore(doc.lastModifiedTime);
                doc.confidence = doc.confidence * m_strategy.similarityWeight + 
                                recencyScore * m_strategy.recencyWeight;
            }
            break;
        }
    }
    
    // Re-sort by confidence
    std::sort(documents.begin(), documents.end(),
        [](const DocumentVector& a, const DocumentVector& b) {
            return a.confidence > b.confidence;
        });
}

double RetrievalRanker::getRecencyScore(const QDateTime& timestamp) const
{
    int daysSinceModification = timestamp.daysTo(QDateTime::currentDateTime());
    
    if (daysSinceModification < 0) {
        daysSinceModification = 0;
    }
    
    // Score decreases with age
    double score = 1.0 - (static_cast<double>(daysSinceModification) / m_strategy.recencyDays);
    return std::max(0.0, score);
}

// ============================================================================
// QueryExpander Implementation
// ============================================================================

QString QueryExpander::expandQuery(const QString& query)
{
    QString expanded = query;
    
    // Add synonyms for common terms
    for (auto it = m_synonyms.begin(); it != m_synonyms.end(); ++it) {
        if (query.contains(it.key(), Qt::CaseInsensitive)) {
            expanded += " OR ";
            for (const auto& synonym : it.value()) {
                expanded += synonym + " OR ";
            }
        }
    }
    
    // Remove trailing " OR "
    if (expanded.endsWith(" OR ")) {
        expanded = expanded.left(expanded.length() - 4);
    }
    
    return expanded;
}

QStringList QueryExpander::generateAlternativePhrases(const QString& query)
{
    QStringList alternatives;
    alternatives.append(query);
    
    // Generate variations (simplified)
    if (query.length() > 20) {
        alternatives.append(query.mid(0, 20));
    }
    
    return alternatives;
}

QStringList QueryExpander::extractKeywords(const QString& query)
{
    QStringList keywords = query.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    
    // Filter out common stop words
    QSet<QString> stopWords = {"the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for"};
    
    QStringList filtered;
    for (const auto& kw : keywords) {
        if (!stopWords.contains(kw.toLower())) {
            filtered.append(kw);
        }
    }
    
    return filtered;
}

// ============================================================================
// RAGKnowledgeBase Implementation
// ============================================================================

RAGKnowledgeBase::RAGKnowledgeBase(QObject* parent)
    : QObject(parent), m_documentCount(0)
{
    // Initialize with simple embedding model by default
    initialize(std::make_unique<SimpleEmbeddingModel>());
    qInfo() << "[RAGKnowledgeBase] Initialized with SimpleEmbeddingModel";
}

RAGKnowledgeBase::~RAGKnowledgeBase()
{
}

void RAGKnowledgeBase::initialize(std::unique_ptr<EmbeddingModel> model)
{
    QMutexLocker locker(&m_mutex);
    m_embeddingModel = std::move(model);
    m_vectorDatabase.clear();
    m_documentChunks.clear();
    m_documentCount = 0;
    
    qInfo() << "[RAGKnowledgeBase] Initialized with embedding dimension:" << m_embeddingModel->getDimension();
}

void RAGKnowledgeBase::ingestDocument(
    const QString& documentId,
    const QString& title,
    const QString& content,
    const QString& source)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_embeddingModel) {
        emit retrievalError("Embedding model not initialized");
        return;
    }
    
    // Chunk the document
    auto chunks = m_chunker.chunkDocument(documentId, title, content, source);
    
    if (chunks.empty()) {
        emit retrievalError("Failed to chunk document");
        return;
    }
    
    // Generate embeddings for each chunk
    QStringList chunkTexts;
    for (const auto& chunk : chunks) {
        chunkTexts.append(chunk.content);
    }
    
    auto embeddings = m_embeddingModel->embedBatch(chunkTexts);
    
    // Add vectors to database
    std::vector<QString> chunkIds;
    for (int i = 0; i < (int)chunks.size(); ++i) {
        chunks[i].embedding = embeddings[i];
        m_vectorDatabase.addVector(chunks[i]);
        chunkIds.push_back(chunks[i].chunkId);
    }
    
    m_documentChunks[documentId] = chunkIds;
    m_documentCount++;
    
    emit documentIngested(documentId, chunks.size());
    emit statisticsUpdated(m_documentCount, (int)m_vectorDatabase.getVectorCount());
    
    qInfo() << "[RAGKnowledgeBase] Ingested document" << documentId << "with" << chunks.size() << "chunks";
}

bool RAGKnowledgeBase::removeDocument(const QString& documentId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_documentChunks.find(documentId);
    if (it == m_documentChunks.end()) {
        return false;
    }
    
    // Remove all chunks for this document
    for (const auto& chunkId : it.value()) {
        m_vectorDatabase.removeVector(chunkId);
    }
    
    m_documentChunks.erase(it);
    m_documentCount--;
    
    emit documentRemoved(documentId);
    emit statisticsUpdated(m_documentCount, (int)m_vectorDatabase.getVectorCount());
    
    qInfo() << "[RAGKnowledgeBase] Removed document:" << documentId;
    
    return true;
}

std::vector<DocumentVector> RAGKnowledgeBase::retrieve(
    const QString& query,
    const RetrievalConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_embeddingModel) {
        emit retrievalError("Embedding model not initialized");
        return {};
    }
    
    // Expand query if configured
    QString expandedQuery = query;
    if (config.useQueryExpansion) {
        expandedQuery = m_queryExpander.expandQuery(query);
    }
    
    // Generate query embedding
    auto queryEmbedding = m_embeddingModel->embed(expandedQuery);
    
    // Search for similar vectors
    auto results = m_vectorDatabase.searchSimilar(
        queryEmbedding,
        config.topK,
        config.minSimilarity
    );
    
    // Rerank if configured
    if (config.useReranking) {
        m_ranker.rankDocuments(results);
        if ((int)results.size() > config.topK) {
            results.resize(config.topK);
        }
    }
    
    emit retrievalComplete(results.size());
    
    qInfo() << "[RAGKnowledgeBase] Retrieved" << results.size() << "documents for query:" << query;
    
    return results;
}

QString RAGKnowledgeBase::buildContext(const std::vector<DocumentVector>& documents)
{
    QString context;
    
    for (const auto& doc : documents) {
        context += QString("Document: %1 (Confidence: %2%%)\n")
            .arg(doc.documentTitle)
            .arg(static_cast<int>(doc.confidence * 100));
        context += doc.content + "\n\n";
    }
    
    return context;
}

QJsonArray RAGKnowledgeBase::buildContextJson(const std::vector<DocumentVector>& documents)
{
    QJsonArray context;
    
    for (const auto& doc : documents) {
        QJsonObject obj = doc.toJson();
        obj["confidence"] = doc.confidence;
        context.append(obj);
    }
    
    return context;
}

RAGKnowledgeBase::Statistics RAGKnowledgeBase::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    
    Statistics stats;
    stats.documentCount = m_documentCount;
    stats.chunkCount = m_vectorDatabase.getVectorCount();
    stats.indexSize = m_vectorDatabase.getIndexSize();
    stats.vectorDimension = m_embeddingModel ? m_embeddingModel->getDimension() : 0;
    
    return stats;
}

void RAGKnowledgeBase::clearAll()
{
    QMutexLocker locker(&m_mutex);
    
    m_vectorDatabase.clear();
    m_documentChunks.clear();
    m_documentCount = 0;
    
    emit statisticsUpdated(0, 0);
    qInfo() << "[RAGKnowledgeBase] Cleared all documents";
}

void RAGKnowledgeBase::setChunkConfig(const DocumentChunker::ChunkConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_chunker.setConfig(config);
}

QJsonObject RAGKnowledgeBase::exportToJson() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject json;
    json["documentCount"] = m_documentCount;
    json["vectorDimension"] = m_embeddingModel ? m_embeddingModel->getDimension() : 0;
    
    QJsonArray vectorsArray;
    for (const auto& vector : m_vectorDatabase.getAllVectors()) {
        vectorsArray.append(vector.toJson());
    }
    json["vectors"] = vectorsArray;
    
    return json;
}

bool RAGKnowledgeBase::importFromJson(const QJsonObject& json)
{
    QMutexLocker locker(&m_mutex);
    
    m_vectorDatabase.clear();
    m_documentChunks.clear();
    m_documentCount = 0;
    
    QJsonArray vectorsArray = json["vectors"].toArray();
    for (const auto& vectorValue : vectorsArray) {
        DocumentVector dv = DocumentVector::fromJson(vectorValue.toObject());
        m_vectorDatabase.addVector(dv);
        
        // Rebuild document chunks map
        if (!dv.documentId.isEmpty()) {
            m_documentChunks[dv.documentId].push_back(dv.chunkId);
        }
    }
    
    // Count unique documents
    m_documentCount = m_documentChunks.size();
    
    emit statisticsUpdated(m_documentCount, (int)m_vectorDatabase.getVectorCount());
    qInfo() << "[RAGKnowledgeBase] Imported" << m_documentCount << "documents";
    
    return true;
}

QStringList RAGKnowledgeBase::getQueryExpansions(const QString& query)
{
    QMutexLocker locker(&m_mutex);
    return m_queryExpander.generateAlternativePhrases(query);
}
