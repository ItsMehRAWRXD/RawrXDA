#include "causal_graph.h"
#include <windows.h>
#include <QString>
#include <QVector>
#include <QDebug>

// MASM64 CKG engine function pointers
extern "C" {
    typedef BOOL (__stdcall *RawrXD_CKG_Init_t)(void *pGraph);
    typedef void *(__stdcall *RawrXD_CKG_CreateEntity_t)(void *pGraph, int entityType, const wchar_t *name, void *pData);
    typedef void *(__stdcall *RawrXD_CKG_FindEntity_t)(void *pGraph, const wchar_t *name);
    typedef void *(__stdcall *RawrXD_CKG_CreateLink_t)(void *pGraph, void *pFrom, void *pTo, int linkType);
    typedef BOOL (__stdcall *RawrXD_CKG_CreateBelief_t)(void *pEntity, const wchar_t *hypothesis);
    typedef BOOL (__stdcall *RawrXD_CKG_AddEvidence_t)(void *pEntity, const wchar_t *evidence, BOOL isSupporting);
    typedef BOOL (__stdcall *RawrXD_CKG_PropagateConfidence_t)(void *pGraph, void *pEntity, int hops);
}

static HMODULE s_ckgLib = nullptr;
static RawrXD_CKG_Init_t s_CKG_Init = nullptr;
static RawrXD_CKG_CreateEntity_t s_CKG_CreateEntity = nullptr;
static RawrXD_CKG_FindEntity_t s_CKG_FindEntity = nullptr;
static RawrXD_CKG_CreateLink_t s_CKG_CreateLink = nullptr;
static RawrXD_CKG_CreateBelief_t s_CKG_CreateBelief = nullptr;
static RawrXD_CKG_AddEvidence_t s_CKG_AddEvidence = nullptr;
static RawrXD_CKG_PropagateConfidence_t s_CKG_PropagateConfidence = nullptr;

CausalGraph::CausalGraph() : m_revision(0) {
    // Load MASM64 CKG library
    // TODO: Load from appropriate path
}

CausalGraph::~CausalGraph() {
    if (s_ckgLib) {
        FreeLibrary(s_ckgLib);
        s_ckgLib = nullptr;
    }
}

int CausalGraph::entityCount() const {
    // TODO: Implement
    return 0;
}

int CausalGraph::linkCount() const {
    // TODO: Implement
    return 0;
}

int CausalGraph::revision() const {
    return m_revision;
}

