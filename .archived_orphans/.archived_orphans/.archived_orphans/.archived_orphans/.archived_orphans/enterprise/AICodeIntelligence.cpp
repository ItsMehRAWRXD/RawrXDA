#include "AICodeIntelligence.hpp"

class AICodeIntelligence::Private {};

AICodeIntelligence::AICodeIntelligence(QObject *parent)
    : QObject(parent)
    , d_ptr(new Private()) {}

AICodeIntelligence::~AICodeIntelligence() = default;
