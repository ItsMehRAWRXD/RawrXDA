#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <memory>

// C++/Qt wrapper for MASM64 Causal Knowledge Graph
class CausalEntity;
class CausalLink;
class EvidenceNode;
class BeliefState;
class KnowledgeGraph;

class CausalGraph {
public:
    CausalGraph();
    ~CausalGraph();
    // Entity operations
    std::shared_ptr<CausalEntity> createEntity(int entityType, const QString &name, void *pData = nullptr);
    std::shared_ptr<CausalEntity> findEntity(const QString &name);
    // Link operations
    std::shared_ptr<CausalLink> createLink(std::shared_ptr<CausalEntity> from, std::shared_ptr<CausalEntity> to, int linkType);
    // Belief state
    bool createBelief(std::shared_ptr<CausalEntity> entity, const QString &hypothesis);
    bool addEvidence(std::shared_ptr<CausalEntity> entity, const QString &evidence, bool isSupporting);
    // Confidence propagation
    bool propagateConfidence(std::shared_ptr<CausalEntity> entity, int hops);
    // Query
    QVector<std::shared_ptr<CausalEntity>> queryEntities(const QString &pattern);
    QVector<std::shared_ptr<CausalLink>> queryLinks(std::shared_ptr<CausalEntity> entity);
    // Graph stats
    int entityCount() const;
    int linkCount() const;
    int revision() const;
    // MASM64 bridge
    void *nativeGraphPtr() const;
private:
    void *m_graph; // Pointer to MASM64 KnowledgeGraph
    int m_revision; // Graph revision counter
};
