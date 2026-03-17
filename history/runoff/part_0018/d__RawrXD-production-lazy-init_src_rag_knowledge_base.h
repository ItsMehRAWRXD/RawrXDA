/**
 * @file rag_knowledge_base.h
 * @brief Retrieval-Augmented Generation (RAG) knowledge base header
 */

#ifndef RAG_KNOWLEDGE_BASE_H
#define RAG_KNOWLEDGE_BASE_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QDateTime>
#include <vector>
#include <map>
#include <memory>
#include <limits>

/**
 * @class DocumentVector
 * @brief Vector embedding with metadata
 */
class DocumentVector
{
public:
    // Document metadata
    QString documentId;
    QString documentTitle;
    QString documentSource;
    QString chunkId;
    int chunkIndex = 0;
    int startOffset = 0;
    int endOffset = 0;
    
    // Content
    QString content;
    QDateTime createdTime;
    QDateTime lastModifiedTime;
    
    // Vector embedding (simplified: dimension = 384 for common models)
    std::vector<float> embedding;
    
    // Metadata
    QJsonObject metadata;
    double confidence = 1.0;
    
    // Methods
    QJsonObject toJson() const;
    static DocumentVector fromJson(const QJsonObject& json);
};

/**
 * @class VectorDatabase
 * @brief In-memory vector database with similarity search
 */
class VectorDatabase
{
public:
    VectorDatabase();
    
    /**
     * Add a vector to the database
     */
    void addVector(const DocumentVector& vector);
    
    /**
     * Remove a vector by ID
     */
    bool removeVector(const QString& vectorId);
    
    /**
     * Clear all vectors
     */
    void clear();
    
    /**
     * Search for similar vectors using cosine similarity
     */
    std::vector<DocumentVector> searchSimilar(
        const std::vector<float>& queryVector,
        int topK = 5,
        double minSimilarity = 0.5
    );
    
    /**
     * Get vector by ID
     */
    DocumentVector getVector(const QString& vectorId) const;
    
    /**
     * Get all vectors
     */
    std::vector<DocumentVector> getAllVectors() const;
    
    /**
     * Get vector count
     */
    size_t getVectorCount() const { return m_vectors.size(); }
    
    /**
     * Get index size in bytes
     */
    long long getIndexSize() const;
    
private:
    /**
     * Calculate cosine similarity between two vectors
     */
    static double cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2);
    
    /**
     * Calculate vector magnitude
     */
    static float magnitude(const std::vector<float>& v);
    
    mutable QMutex m_mutex;
    std::vector<DocumentVector> m_vectors;
};

/**
 * @class DocumentChunker
 * @brief Splits documents into semantic chunks
 */
class DocumentChunker
{
public:
    enum ChunkerStrategy {
        SENTENCE_BASED,      // Split on sentence boundaries
        PARAGRAPH_BASED,     // Split on paragraph boundaries
        SLIDING_WINDOW,      // Overlapping chunks with sliding window
        SEMANTIC_SPLIT       // Split on semantic boundaries (placeholder for advanced NLP)
    };
    
    struct ChunkConfig {
        int maxChunkSize = 512;           // Max characters per chunk
        int minChunkSize = 50;            // Min characters per chunk
        int overlapSize = 50;             // Overlap between chunks (for sliding window)
        ChunkerStrategy strategy = SLIDING_WINDOW;
    };
    
    DocumentChunker(const ChunkConfig& config = ChunkConfig());
    
    /**
     * Split document into chunks
     */
    std::vector<DocumentVector> chunkDocument(
        const QString& documentId,
        const QString& title,
        const QString& content,
        const QString& source = ""
    );
    
    /**
     * Set chunking configuration
     */
    void setConfig(const ChunkConfig& config) { m_config = config; }
    
    /**
     * Get current configuration
     */
    ChunkConfig getConfig() const { return m_config; }
    
private:
    /**
     * Split text into sentences
     */
    std::vector<QString> splitSentences(const QString& text);
    
    /**
     * Split text into paragraphs
     */
    std::vector<QString> splitParagraphs(const QString& text);
    
    /**
     * Apply sliding window chunking
     */
    std::vector<QString> applySlidingWindow(const QString& text);
    
    ChunkConfig m_config;
};

/**
 * @class EmbeddingModel
 * @brief Interface for embedding generation (placeholder for real embedding models)
 */
class EmbeddingModel
{
public:
    virtual ~EmbeddingModel() = default;
    
    /**
     * Generate embedding for text
     */
    virtual std::vector<float> embed(const QString& text) = 0;
    
    /**
     * Generate embeddings for multiple texts
     */
    virtual std::vector<std::vector<float>> embedBatch(const QStringList& texts) = 0;
    
    /**
     * Get embedding dimension
     */
    virtual int getDimension() const = 0;
};

/**
 * @class SimpleEmbeddingModel
 * @brief Simple hash-based embedding model for testing/development
 */
class SimpleEmbeddingModel : public EmbeddingModel
{
public:
    SimpleEmbeddingModel(int dimension = 384);
    
    std::vector<float> embed(const QString& text) override;
    std::vector<std::vector<float>> embedBatch(const QStringList& texts) override;
    int getDimension() const override { return m_dimension; }
    
private:
    int m_dimension;
    
    /**
     * Hash text to floating point values
     */
    std::vector<float> hashToVector(const QString& text, int dimension);
};

/**
 * @class RetrievalRanker
 * @brief Ranks retrieved documents by relevance
 */
class RetrievalRanker
{
public:
    struct RankingStrategy {
        enum Type {
            SIMILARITY_ONLY,
            RECENCY_BOOSTED,
            HYBRID
        } type = SIMILARITY_ONLY;
        
        double similarityWeight = 0.7;
        double recencyWeight = 0.3;
        int recencyDays = 30;
    };
    
    RetrievalRanker(const RankingStrategy& strategy = RankingStrategy());
    
    /**
     * Rank retrieved documents
     */
    void rankDocuments(std::vector<DocumentVector>& documents) const;
    
    /**
     * Set ranking strategy
     */
    void setStrategy(const RankingStrategy& strategy) { m_strategy = strategy; }
    
private:
    /**
     * Calculate recency score (0.0 - 1.0, higher = more recent)
     */
    double getRecencyScore(const QDateTime& timestamp) const;
    
    RankingStrategy m_strategy;
};

/**
 * @class QueryExpander
 * @brief Expands queries for better retrieval
 */
class QueryExpander
{
public:
    /**
     * Expand query with related terms
     */
    QString expandQuery(const QString& query);
    
    /**
     * Generate alternative phrasings
     */
    QStringList generateAlternativePhrases(const QString& query);
    
    /**
     * Extract keywords from query
     */
    QStringList extractKeywords(const QString& query);
    
private:
    /**
     * Common synonym mappings
     */
    std::map<QString, QStringList> m_synonyms;
};

/**
 * @class RAGKnowledgeBase
 * @brief Main RAG system orchestrator
 */
class RAGKnowledgeBase : public QObject
{
    Q_OBJECT
    
public:
    struct RetrievalConfig {
        int topK = 5;
        double minSimilarity = 0.5;
        bool useQueryExpansion = true;
        bool useReranking = true;
        int maxContextTokens = 3000;
    };
    
    explicit RAGKnowledgeBase(QObject* parent = nullptr);
    ~RAGKnowledgeBase();
    
    /**
     * Initialize the RAG system with an embedding model
     */
    void initialize(std::unique_ptr<EmbeddingModel> model);
    
    /**
     * Ingest a document
     */
    void ingestDocument(
        const QString& documentId,
        const QString& title,
        const QString& content,
        const QString& source = ""
    );
    
    /**
     * Remove document and its chunks
     */
    bool removeDocument(const QString& documentId);
    
    /**
     * Retrieve relevant documents for a query
     */
    std::vector<DocumentVector> retrieve(
        const QString& query,
        const RetrievalConfig& config = RetrievalConfig()
    );
    
    /**
     * Build context string from retrieved documents
     */
    QString buildContext(const std::vector<DocumentVector>& documents);
    
    /**
     * Build context as JSON array
     */
    QJsonArray buildContextJson(const std::vector<DocumentVector>& documents);
    
    /**
     * Get knowledge base statistics
     */
    struct Statistics {
        int documentCount = 0;
        int chunkCount = 0;
        long long indexSize = 0;
        int vectorDimension = 0;
    };
    Statistics getStatistics() const;
    
    /**
     * Clear all documents
     */
    void clearAll();
    
    /**
     * Set chunking configuration
     */
    void setChunkConfig(const DocumentChunker::ChunkConfig& config);
    
    /**
     * Set retrieval configuration
     */
    void setRetrievalConfig(const RetrievalConfig& config) { m_retrievalConfig = config; }
    
    /**
     * Export knowledge base to JSON
     */
    QJsonObject exportToJson() const;
    
    /**
     * Import knowledge base from JSON
     */
    bool importFromJson(const QJsonObject& json);
    
    /**
     * Get query expansion suggestions
     */
    QStringList getQueryExpansions(const QString& query);
    
signals:
    /**
     * Emitted when a document is ingested
     */
    void documentIngested(const QString& documentId, int chunkCount);
    
    /**
     * Emitted when a document is removed
     */
    void documentRemoved(const QString& documentId);
    
    /**
     * Emitted when retrieval is complete
     */
    void retrievalComplete(int resultCount);
    
    /**
     * Emitted on retrieval error
     */
    void retrievalError(const QString& error);
    
    /**
     * Emitted when statistics change
     */
    void statisticsUpdated(int documentCount, int chunkCount);
    
private:
    mutable QMutex m_mutex;
    
    std::unique_ptr<EmbeddingModel> m_embeddingModel;
    VectorDatabase m_vectorDatabase;
    DocumentChunker m_chunker;
    RetrievalRanker m_ranker;
    QueryExpander m_queryExpander;
    
    RetrievalConfig m_retrievalConfig;
    std::map<QString, std::vector<QString>> m_documentChunks; // documentId -> chunkIds
    
    int m_documentCount = 0;
};

#endif // RAG_KNOWLEDGE_BASE_H
