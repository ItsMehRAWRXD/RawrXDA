#include "MLWorkflowOptimizer.hpp"

class MLWorkflowOptimizer::Private {};

MLWorkflowOptimizer::MLWorkflowOptimizer(QObject *parent)
    : QObject(parent)
    , d_ptr(new Private()) {}

MLWorkflowOptimizer::~MLWorkflowOptimizer() = default;
