#include "DebugExpressionEvaluator.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>
#include <QStack>
#include <cmath>
#include <algorithm>
#include <cctype>

// ============================================================================
// Expression Base Implementation
// ============================================================================

QJsonObject Expression::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["type"] = static_cast<int>(type);
    obj["source"] = sourceExpression;
    obj["value"] = value.toString();
    obj["dataType"] = dataType;
    obj["isConstant"] = isConstant;
    obj["error"] = errorMessage;
    return obj;
}

QVariant Expression::evaluate() const {
    return value;
}

// ============================================================================
// LiteralExpression Implementation
// ============================================================================

QJsonObject LiteralExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    obj["literalValue"] = literalValue.toString();
    return obj;
}

QVariant LiteralExpression::evaluate() const {
    return literalValue;
}

// ============================================================================
// VariableExpression Implementation
// ============================================================================

QJsonObject VariableExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    obj["variableName"] = variableName;
    return obj;
}

QVariant VariableExpression::evaluate() const {
    return value;
}

// ============================================================================
// MemberExpression Implementation
// ============================================================================

QJsonObject MemberExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    obj["objectName"] = objectName;
    obj["memberName"] = memberName;
    obj["isPointer"] = isPointer;
    return obj;
}

QVariant MemberExpression::evaluate() const {
    return value;
}

// ============================================================================
// ArrayAccessExpression Implementation
// ============================================================================

QJsonObject ArrayAccessExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    obj["arrayName"] = arrayName;
    obj["index"] = index;
    return obj;
}

QVariant ArrayAccessExpression::evaluate() const {
    return value;
}

// ============================================================================
// BinaryOpExpression Implementation
// ============================================================================

QJsonObject BinaryOpExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    if (left) obj["left"] = left->toJson();
    if (right) obj["right"] = right->toJson();
    obj["operator"] = static_cast<int>(op);
    return obj;
}

QVariant BinaryOpExpression::evaluate() const {
    if (!left || !right) return QVariant();
    
    QVariant leftVal = left->evaluate();
    QVariant rightVal = right->evaluate();
    
    switch (op) {
        case OperatorType::Add:
            return leftVal.toDouble() + rightVal.toDouble();
        case OperatorType::Subtract:
            return leftVal.toDouble() - rightVal.toDouble();
        case OperatorType::Multiply:
            return leftVal.toDouble() * rightVal.toDouble();
        case OperatorType::Divide:
            if (rightVal.toDouble() != 0) {
                return leftVal.toDouble() / rightVal.toDouble();
            }
            return QVariant();
        case OperatorType::Modulo:
            return leftVal.toLongLong() % rightVal.toLongLong();
        case OperatorType::Equal:
            return leftVal == rightVal;
        case OperatorType::NotEqual:
            return leftVal != rightVal;
        case OperatorType::GreaterThan:
            return leftVal.toDouble() > rightVal.toDouble();
        case OperatorType::LessThan:
            return leftVal.toDouble() < rightVal.toDouble();
        case OperatorType::GreaterOrEqual:
            return leftVal.toDouble() >= rightVal.toDouble();
        case OperatorType::LessOrEqual:
            return leftVal.toDouble() <= rightVal.toDouble();
        case OperatorType::And:
            return leftVal.toBool() && rightVal.toBool();
        case OperatorType::Or:
            return leftVal.toBool() || rightVal.toBool();
        case OperatorType::BitwiseAnd:
            return leftVal.toLongLong() & rightVal.toLongLong();
        case OperatorType::BitwiseOr:
            return leftVal.toLongLong() | rightVal.toLongLong();
        case OperatorType::BitwiseXor:
            return leftVal.toLongLong() ^ rightVal.toLongLong();
        case OperatorType::LeftShift:
            return leftVal.toLongLong() << rightVal.toLongLong();
        case OperatorType::RightShift:
            return leftVal.toLongLong() >> rightVal.toLongLong();
        default:
            return QVariant();
    }
}

// ============================================================================
// UnaryOpExpression Implementation
// ============================================================================

QJsonObject UnaryOpExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    if (operand) obj["operand"] = operand->toJson();
    obj["operator"] = static_cast<int>(op);
    return obj;
}

QVariant UnaryOpExpression::evaluate() const {
    if (!operand) return QVariant();
    
    QVariant val = operand->evaluate();
    
    switch (op) {
        case OperatorType::Not:
            return !val.toBool();
        case OperatorType::BitwiseNot:
            return ~val.toLongLong();
        case OperatorType::Subtract:
            return -val.toDouble();
        default:
            return QVariant();
    }
}

// ============================================================================
// ConditionalExpression Implementation
// ============================================================================

QJsonObject ConditionalExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    if (condition) obj["condition"] = condition->toJson();
    if (trueExpr) obj["trueExpr"] = trueExpr->toJson();
    if (falseExpr) obj["falseExpr"] = falseExpr->toJson();
    return obj;
}

QVariant ConditionalExpression::evaluate() const {
    if (!condition) return QVariant();
    
    if (condition->evaluate().toBool()) {
        return trueExpr ? trueExpr->evaluate() : QVariant();
    } else {
        return falseExpr ? falseExpr->evaluate() : QVariant();
    }
}

// ============================================================================
// FunctionCallExpression Implementation
// ============================================================================

QJsonObject FunctionCallExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    obj["functionName"] = functionName;
    QJsonArray argsArray;
    for (const auto& arg : arguments) {
        if (arg) argsArray.append(arg->toJson());
    }
    obj["arguments"] = argsArray;
    return obj;
}

QVariant FunctionCallExpression::evaluate() const {
    return value;
}

// ============================================================================
// CastExpression Implementation
// ============================================================================

QJsonObject CastExpression::toJson() const {
    QJsonObject obj = Expression::toJson();
    if (operand) obj["operand"] = operand->toJson();
    obj["targetType"] = targetType;
    return obj;
}

QVariant CastExpression::evaluate() const {
    if (!operand) return QVariant();
    return operand->evaluate();
}

// ============================================================================
// DebugExpressionEvaluator Implementation
// ============================================================================

DebugExpressionEvaluator::DebugExpressionEvaluator(QObject* parent)
    : QObject(parent), m_nextWatchId(1), m_cachingEnabled(true) {
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseExpression(const QString& expression) {
    if (m_cachingEnabled && m_expressionCache.contains(expression)) {
        return m_expressionCache[expression];
    }
    
    QString trimmed = expression.trimmed();
    std::shared_ptr<Expression> result;
    
    if (trimmed.startsWith('(') && trimmed.contains(')')) {
        result = parseCast(trimmed);
    } else if (trimmed.contains('[')) {
        result = parseArrayAccess(trimmed);
    } else if (trimmed.contains('.') || trimmed.contains("->")) {
        result = parseMemberAccess(trimmed);
    } else if (trimmed.contains('(') && trimmed.endsWith(')')) {
        result = parseFunctionCall(trimmed);
    } else if (trimmed.contains('?') && trimmed.contains(':')) {
        result = parseConditional(trimmed);
    } else if (trimmed.contains('+') || trimmed.contains('-') || trimmed.contains('*') || 
               trimmed.contains('/') || trimmed.contains('%') || trimmed.contains("==") ||
               trimmed.contains("!=") || trimmed.contains('>') || trimmed.contains('<') ||
               trimmed.contains("&&") || trimmed.contains("||")) {
        result = parseBinaryOperation(trimmed);
    } else if (trimmed.startsWith('!') || trimmed.startsWith('~') || trimmed.startsWith('*') ||
               trimmed.startsWith('&')) {
        result = parseUnaryOperation(trimmed);
    } else if (isValidNumber(trimmed) || (trimmed.startsWith('"') && trimmed.endsWith('"'))) {
        result = parseLiteral(trimmed);
    } else {
        result = parseVariable(trimmed);
    }
    
    if (result && m_cachingEnabled) {
        m_expressionCache[expression] = result;
    }
    
    return result;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseVariable(const QString& varName) {
    auto var = std::make_shared<VariableExpression>();
    var->type = ExpressionType::Variable;
    var->variableName = varName;
    var->sourceExpression = varName;
    var->isConstant = false;
    
    if (m_variables.contains(varName)) {
        var->value = m_variables[varName];
        var->dataType = inferType(var->value);
    } else {
        var->errorMessage = "Variable not found: " + varName;
        m_lastError = var->errorMessage;
    }
    
    return var;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseLiteral(const QString& literal) {
    auto lit = std::make_shared<LiteralExpression>();
    lit->type = ExpressionType::Literal;
    lit->sourceExpression = literal;
    lit->isConstant = true;
    
    if (literal.startsWith('"') && literal.endsWith('"')) {
        lit->literalValue = literal.mid(1, literal.length() - 2);
        lit->dataType = "string";
    } else if (isValidNumber(literal)) {
        if (literal.contains('.')) {
            lit->literalValue = literal.toDouble();
            lit->dataType = "double";
        } else {
            lit->literalValue = literal.toLongLong();
            lit->dataType = "int";
        }
    } else {
        lit->errorMessage = "Invalid literal: " + literal;
        m_lastError = lit->errorMessage;
    }
    
    lit->value = lit->literalValue;
    return lit;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseBinaryOperation(const QString& expression) {
    QStringList tokens = tokenize(expression);
    return shuntingYardAlgorithm(tokens);
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseUnaryOperation(const QString& expression) {
    auto unary = std::make_shared<UnaryOpExpression>();
    unary->type = ExpressionType::UnaryOp;
    unary->sourceExpression = expression;
    
    QChar op = expression[0];
    QString operandStr = expression.mid(1).trimmed();
    
    if (op == '!') {
        unary->op = OperatorType::Not;
    } else if (op == '~') {
        unary->op = OperatorType::BitwiseNot;
    } else if (op == '*') {
        unary->op = OperatorType::Dereference;
    } else if (op == '&') {
        unary->op = OperatorType::AddressOf;
    } else if (op == '-') {
        unary->op = OperatorType::Subtract;
    }
    
    unary->operand = parseExpression(operandStr);
    if (unary->operand) {
        unary->value = unary->evaluate();
        unary->dataType = unary->operand->dataType;
    }
    
    return unary;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseConditional(const QString& expression) {
    int questionPos = expression.indexOf('?');
    int colonPos = expression.indexOf(':', questionPos);
    
    if (questionPos < 0 || colonPos < 0) {
        return nullptr;
    }
    
    auto cond = std::make_shared<ConditionalExpression>();
    cond->type = ExpressionType::Conditional;
    cond->sourceExpression = expression;
    
    QString condStr = expression.left(questionPos).trimmed();
    QString trueStr = expression.mid(questionPos + 1, colonPos - questionPos - 1).trimmed();
    QString falseStr = expression.mid(colonPos + 1).trimmed();
    
    cond->condition = parseExpression(condStr);
    cond->trueExpr = parseExpression(trueStr);
    cond->falseExpr = parseExpression(falseStr);
    
    if (cond->condition && cond->trueExpr && cond->falseExpr) {
        cond->value = cond->evaluate();
        cond->dataType = cond->trueExpr->dataType;
    }
    
    return cond;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseArrayAccess(const QString& expression) {
    int bracketPos = expression.indexOf('[');
    int closeBracketPos = expression.lastIndexOf(']');
    
    if (bracketPos < 0 || closeBracketPos < 0) {
        return nullptr;
    }
    
    auto arr = std::make_shared<ArrayAccessExpression>();
    arr->type = ExpressionType::ArrayAccess;
    arr->arrayName = expression.left(bracketPos).trimmed();
    arr->sourceExpression = expression;
    
    QString indexStr = expression.mid(bracketPos + 1, closeBracketPos - bracketPos - 1).trimmed();
    arr->index = indexStr.toInt();
    arr->value = QVariant();
    
    return arr;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseMemberAccess(const QString& expression) {
    auto member = std::make_shared<MemberExpression>();
    member->type = ExpressionType::MemberAccess;
    member->sourceExpression = expression;
    
    int pointerPos = expression.indexOf("->");
    int dotPos = expression.indexOf('.');
    
    if (pointerPos >= 0) {
        member->objectName = expression.left(pointerPos).trimmed();
        member->memberName = expression.mid(pointerPos + 2).trimmed();
        member->isPointer = true;
    } else if (dotPos >= 0) {
        member->objectName = expression.left(dotPos).trimmed();
        member->memberName = expression.mid(dotPos + 1).trimmed();
        member->isPointer = false;
    }
    
    return member;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseFunctionCall(const QString& expression) {
    int parenPos = expression.indexOf('(');
    
    if (parenPos < 0) {
        return nullptr;
    }
    
    auto funcCall = std::make_shared<FunctionCallExpression>();
    funcCall->type = ExpressionType::FunctionCall;
    funcCall->functionName = expression.left(parenPos).trimmed();
    funcCall->sourceExpression = expression;
    
    int closeParenPos = expression.lastIndexOf(')');
    QString argsStr = expression.mid(parenPos + 1, closeParenPos - parenPos - 1);
    
    if (!argsStr.isEmpty()) {
        QStringList argsList = argsStr.split(',');
        for (const auto& arg : argsList) {
            auto argExpr = parseExpression(arg.trimmed());
            if (argExpr) {
                funcCall->arguments.append(argExpr);
            }
        }
    }
    
    return funcCall;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::parseCast(const QString& expression) {
    QRegularExpression castRegex("^\\(([^)]+)\\)(.+)$");
    auto match = castRegex.match(expression);
    
    if (!match.hasMatch()) {
        return nullptr;
    }
    
    auto cast = std::make_shared<CastExpression>();
    cast->type = ExpressionType::Cast;
    cast->targetType = match.captured(1).trimmed();
    cast->sourceExpression = expression;
    
    QString operandStr = match.captured(2).trimmed();
    cast->operand = parseExpression(operandStr);
    
    if (cast->operand) {
        cast->value = cast->evaluate();
        cast->dataType = cast->targetType;
    }
    
    return cast;
}

QVariant DebugExpressionEvaluator::evaluateExpression(const QString& expression) {
    auto expr = parseExpression(expression);
    return expr ? expr->evaluate() : QVariant();
}

QVariant DebugExpressionEvaluator::evaluateExpression(std::shared_ptr<Expression> expr) {
    return expr ? expr->evaluate() : QVariant();
}

bool DebugExpressionEvaluator::evaluateCondition(const QString& condition) {
    return evaluateExpression(condition).toBool();
}

QString DebugExpressionEvaluator::evaluateToString(const QString& expression) {
    return evaluateExpression(expression).toString();
}

int DebugExpressionEvaluator::evaluateToInt(const QString& expression) {
    return evaluateExpression(expression).toInt();
}

double DebugExpressionEvaluator::evaluateToDouble(const QString& expression) {
    return evaluateExpression(expression).toDouble();
}

void DebugExpressionEvaluator::setVariable(const QString& name, const QVariant& value) {
    QVariant oldValue = m_variables.value(name);
    m_variables[name] = value;
    emit variableChanged(name, oldValue, value);
}

QVariant DebugExpressionEvaluator::getVariable(const QString& name) const {
    return m_variables.value(name);
}

bool DebugExpressionEvaluator::hasVariable(const QString& name) const {
    return m_variables.contains(name);
}

void DebugExpressionEvaluator::removeVariable(const QString& name) {
    m_variables.remove(name);
}

void DebugExpressionEvaluator::clearVariables() {
    m_variables.clear();
}

QMap<QString, QVariant> DebugExpressionEvaluator::getAllVariables() const {
    return m_variables;
}

int DebugExpressionEvaluator::addWatchExpression(const QString& expression) {
    int id = m_nextWatchId++;
    m_watchExpressions[id] = expression;
    m_watchValues[id] = evaluateExpression(expression);
    return id;
}

bool DebugExpressionEvaluator::removeWatchExpression(int id) {
    bool found = m_watchExpressions.contains(id);
    m_watchExpressions.remove(id);
    m_watchValues.remove(id);
    return found;
}

QString DebugExpressionEvaluator::getWatchExpression(int id) const {
    return m_watchExpressions.value(id, "");
}

QVariant DebugExpressionEvaluator::evaluateWatch(int id) {
    if (!m_watchExpressions.contains(id)) {
        return QVariant();
    }
    
    QVariant newValue = evaluateExpression(m_watchExpressions[id]);
    QVariant oldValue = m_watchValues[id];
    
    if (newValue != oldValue) {
        m_watchValues[id] = newValue;
        emit watchTriggered(id, oldValue, newValue);
    }
    
    return newValue;
}

QMap<int, QString> DebugExpressionEvaluator::getAllWatchExpressions() const {
    return m_watchExpressions;
}

QList<QVariant> DebugExpressionEvaluator::evaluateAllWatches() {
    QList<QVariant> results;
    for (int id : m_watchExpressions.keys()) {
        results.append(evaluateWatch(id));
    }
    return results;
}

void DebugExpressionEvaluator::registerFunction(const QString& name, std::function<QVariant(const QList<QVariant>&)> func) {
    m_customFunctions[name] = func;
}

bool DebugExpressionEvaluator::hasCustomFunction(const QString& name) const {
    return m_customFunctions.contains(name);
}

QVariant DebugExpressionEvaluator::callCustomFunction(const QString& name, const QList<QVariant>& args) {
    if (!m_customFunctions.contains(name)) {
        m_lastError = "Function not found: " + name;
        return QVariant();
    }
    return m_customFunctions[name](args);
}

QStringList DebugExpressionEvaluator::getAllCustomFunctions() const {
    return m_customFunctions.keys();
}

void DebugExpressionEvaluator::setMemoryValue(uintptr_t address, const QByteArray& value) {
    // Placeholder: actual implementation would write to process memory
}

QByteArray DebugExpressionEvaluator::readMemory(uintptr_t address, size_t size) {
    // Placeholder: actual implementation would read from process memory
    return QByteArray(static_cast<int>(size), 0);
}

void DebugExpressionEvaluator::setPointerVariable(const QString& name, uintptr_t address) {
    m_variables[name] = QVariant::fromValue(address);
}

QString DebugExpressionEvaluator::inferType(const QVariant& value) const {
    if (!value.isValid()) return "void";
    if (value.type() == QVariant::Bool) return "bool";
    if (value.type() == QVariant::Int) return "int";
    if (value.type() == QVariant::Double) return "double";
    if (value.type() == QVariant::String) return "string";
    if (value.type() == QVariant::ByteArray) return "bytes";
    return "unknown";
}

QString DebugExpressionEvaluator::inferType(const std::shared_ptr<Expression>& expr) const {
    return expr ? expr->dataType : "unknown";
}

bool DebugExpressionEvaluator::isTypeCompatible(const QString& type1, const QString& type2) const {
    if (type1 == type2) return true;
    if ((type1 == "int" && type2 == "double") || (type1 == "double" && type2 == "int")) return true;
    return false;
}

QVariant DebugExpressionEvaluator::castValue(const QVariant& value, const QString& targetType) {
    if (targetType == "int") return value.toInt();
    if (targetType == "double") return value.toDouble();
    if (targetType == "string") return value.toString();
    if (targetType == "bool") return value.toBool();
    return value;
}

bool DebugExpressionEvaluator::validateExpression(const QString& expression) const {
    return !expression.isEmpty() && validateVariableNames(expression);
}

bool DebugExpressionEvaluator::validateVariableNames(const QString& expression) const {
    QStringList tokens = tokenize(expression);
    for (const auto& token : tokens) {
        if (isValidIdentifier(token) && !m_variables.contains(token) && !m_customFunctions.contains(token)) {
            // Identifier not found but might be a literal or operator
        }
    }
    return true;
}

QString DebugExpressionEvaluator::getLastError() const {
    return m_lastError;
}

std::shared_ptr<Expression> DebugExpressionEvaluator::optimizeExpression(std::shared_ptr<Expression> expr) {
    ExpressionOptimizer optimizer(this);
    return optimizer.optimize(expr);
}

bool DebugExpressionEvaluator::isConstantExpression(std::shared_ptr<Expression> expr) const {
    return expr && expr->isConstant;
}

void DebugExpressionEvaluator::enableExpressionCaching(bool enabled) {
    m_cachingEnabled = enabled;
}

void DebugExpressionEvaluator::clearExpressionCache() {
    m_expressionCache.clear();
}

int DebugExpressionEvaluator::getCacheSize() const {
    return m_expressionCache.size();
}

QString DebugExpressionEvaluator::expressionToString(std::shared_ptr<Expression> expr) const {
    return expr ? expr->sourceExpression : "";
}

void DebugExpressionEvaluator::dumpExpressionTree(std::shared_ptr<Expression> expr) const {
    if (!expr) return;
    qDebug() << "Expression: " << expr->sourceExpression;
    qDebug() << "Type: " << static_cast<int>(expr->type);
    qDebug() << "Value: " << expr->value.toString();
}

void DebugExpressionEvaluator::saveVariables(const QString& filePath) const {
    QJsonObject obj;
    for (auto it = m_variables.begin(); it != m_variables.end(); ++it) {
        obj[it.key()] = it.value().toString();
    }
    QJsonDocument doc(obj);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void DebugExpressionEvaluator::loadVariables(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            m_variables[it.key()] = it.value().toVariant();
        }
    }
}

QJsonObject DebugExpressionEvaluator::toJson() const {
    QJsonObject root;
    QJsonObject varsObj;
    for (auto it = m_variables.begin(); it != m_variables.end(); ++it) {
        varsObj[it.key()] = it.value().toString();
    }
    root["variables"] = varsObj;
    return root;
}

void DebugExpressionEvaluator::fromJson(const QJsonObject& obj) {
    QJsonObject varsObj = obj["variables"].toObject();
    for (auto it = varsObj.begin(); it != varsObj.end(); ++it) {
        m_variables[it.key()] = it.value().toVariant();
    }
}

bool DebugExpressionEvaluator::isOperator(const QString& token) const {
    static const QStringList operators = {"+", "-", "*", "/", "%", "==", "!=", ">", "<", ">=", "<=",
                                          "&&", "||", "!", "~", "&", "|", "^", "<<", ">>"};
    return operators.contains(token);
}

OperatorType DebugExpressionEvaluator::stringToOperator(const QString& op) const {
    if (op == "+") return OperatorType::Add;
    if (op == "-") return OperatorType::Subtract;
    if (op == "*") return OperatorType::Multiply;
    if (op == "/") return OperatorType::Divide;
    if (op == "%") return OperatorType::Modulo;
    if (op == "==") return OperatorType::Equal;
    if (op == "!=") return OperatorType::NotEqual;
    if (op == ">") return OperatorType::GreaterThan;
    if (op == "<") return OperatorType::LessThan;
    if (op == ">=") return OperatorType::GreaterOrEqual;
    if (op == "<=") return OperatorType::LessOrEqual;
    if (op == "&&") return OperatorType::And;
    if (op == "||") return OperatorType::Or;
    if (op == "!") return OperatorType::Not;
    if (op == "~") return OperatorType::BitwiseNot;
    if (op == "&") return OperatorType::BitwiseAnd;
    if (op == "|") return OperatorType::BitwiseOr;
    if (op == "^") return OperatorType::BitwiseXor;
    if (op == "<<") return OperatorType::LeftShift;
    if (op == ">>") return OperatorType::RightShift;
    return OperatorType::Add;
}

QString DebugExpressionEvaluator::operatorToString(OperatorType op) const {
    switch (op) {
        case OperatorType::Add: return "+";
        case OperatorType::Subtract: return "-";
        case OperatorType::Multiply: return "*";
        case OperatorType::Divide: return "/";
        case OperatorType::Modulo: return "%";
        case OperatorType::Equal: return "==";
        case OperatorType::NotEqual: return "!=";
        case OperatorType::GreaterThan: return ">";
        case OperatorType::LessThan: return "<";
        case OperatorType::GreaterOrEqual: return ">=";
        case OperatorType::LessOrEqual: return "<=";
        case OperatorType::And: return "&&";
        case OperatorType::Or: return "||";
        case OperatorType::Not: return "!";
        case OperatorType::BitwiseNot: return "~";
        case OperatorType::BitwiseAnd: return "&";
        case OperatorType::BitwiseOr: return "|";
        case OperatorType::BitwiseXor: return "^";
        case OperatorType::LeftShift: return "<<";
        case OperatorType::RightShift: return ">>";
        default: return "?";
    }
}

int DebugExpressionEvaluator::getOperatorPrecedence(OperatorType op) const {
    switch (op) {
        case OperatorType::Or: return 1;
        case OperatorType::And: return 2;
        case OperatorType::BitwiseOr: return 3;
        case OperatorType::BitwiseXor: return 4;
        case OperatorType::BitwiseAnd: return 5;
        case OperatorType::Equal:
        case OperatorType::NotEqual: return 6;
        case OperatorType::GreaterThan:
        case OperatorType::LessThan:
        case OperatorType::GreaterOrEqual:
        case OperatorType::LessOrEqual: return 7;
        case OperatorType::LeftShift:
        case OperatorType::RightShift: return 8;
        case OperatorType::Add:
        case OperatorType::Subtract: return 9;
        case OperatorType::Multiply:
        case OperatorType::Divide:
        case OperatorType::Modulo: return 10;
        case OperatorType::Not:
        case OperatorType::BitwiseNot: return 11;
        default: return 0;
    }
}

bool DebugExpressionEvaluator::isLeftAssociative(OperatorType op) const {
    return true; // Most operators are left associative
}

QStringList DebugExpressionEvaluator::tokenize(const QString& expression) const {
    QStringList tokens;
    QString current;
    
    for (int i = 0; i < expression.length(); ++i) {
        QChar c = expression[i];
        
        if (c.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else if (c == '(' || c == ')' || c == '[' || c == ']' || c == ',' || c == ';') {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            tokens.append(c);
        } else if (isOperator(QString(c))) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            if (i + 1 < expression.length() && isOperator(QString(c) + expression[i + 1])) {
                tokens.append(QString(c) + expression[i + 1]);
                ++i;
            } else {
                tokens.append(c);
            }
        } else {
            current.append(c);
        }
    }
    
    if (!current.isEmpty()) {
        tokens.append(current);
    }
    
    return tokens;
}

bool DebugExpressionEvaluator::isValidIdentifier(const QString& name) const {
    if (name.isEmpty()) return false;
    if (!name[0].isLetter() && name[0] != '_') return false;
    for (const auto& c : name) {
        if (!c.isLetterOrNumber() && c != '_') return false;
    }
    return true;
}

bool DebugExpressionEvaluator::isValidNumber(const QString& str) const {
    bool ok;
    if (str.contains('.')) {
        str.toDouble(&ok);
    } else {
        str.toLongLong(&ok);
    }
    return ok;
}

bool DebugExpressionEvaluator::isValidString(const QString& str) const {
    return str.startsWith('"') && str.endsWith('"');
}

std::shared_ptr<Expression> DebugExpressionEvaluator::buildExpressionFromTokens(const QStringList& tokens) {
    return shuntingYardAlgorithm(tokens);
}

std::shared_ptr<Expression> DebugExpressionEvaluator::shuntingYardAlgorithm(const QStringList& tokens) {
    QStack<std::shared_ptr<Expression>> outputStack;
    QStack<QString> operatorStack;
    
    for (const auto& token : tokens) {
        if (isValidNumber(token) || isValidString(token)) {
            outputStack.push(parseLiteral(token));
        } else if (isValidIdentifier(token)) {
            outputStack.push(parseVariable(token));
        } else if (isOperator(token)) {
            while (!operatorStack.isEmpty()) {
                QString topOp = operatorStack.top();
                if (isOperator(topOp) && 
                    getOperatorPrecedence(stringToOperator(topOp)) >= getOperatorPrecedence(stringToOperator(token))) {
                    operatorStack.pop();
                    // Apply operator
                } else {
                    break;
                }
            }
            operatorStack.push(token);
        }
    }
    
    while (!operatorStack.isEmpty()) {
        // Apply remaining operators
        operatorStack.pop();
    }
    
    return outputStack.isEmpty() ? nullptr : outputStack.top();
}

QVariant DebugExpressionEvaluator::stringToVariant(const QString& str) const {
    if (isValidNumber(str)) {
        if (str.contains('.')) {
            return str.toDouble();
        } else {
            return str.toLongLong();
        }
    }
    return str;
}

QString DebugExpressionEvaluator::variantToString(const QVariant& var) const {
    return var.toString();
}

// ============================================================================
// ExpressionOptimizer Implementation
// ============================================================================

ExpressionOptimizer::ExpressionOptimizer(DebugExpressionEvaluator* evaluator)
    : m_evaluator(evaluator) {
}

std::shared_ptr<Expression> ExpressionOptimizer::optimize(std::shared_ptr<Expression> expr) {
    if (!expr) return nullptr;
    
    switch (expr->type) {
        case ExpressionType::BinaryOp: {
            auto binOp = std::dynamic_pointer_cast<BinaryOpExpression>(expr);
            return optimizeBinary(binOp);
        }
        case ExpressionType::UnaryOp: {
            auto unaryOp = std::dynamic_pointer_cast<UnaryOpExpression>(expr);
            return optimizeUnary(unaryOp);
        }
        case ExpressionType::Conditional: {
            auto cond = std::dynamic_pointer_cast<ConditionalExpression>(expr);
            return optimizeConditional(cond);
        }
        default:
            return expr;
    }
}

std::shared_ptr<Expression> ExpressionOptimizer::optimizeBinary(std::shared_ptr<BinaryOpExpression> expr) {
    if (!expr) return nullptr;
    
    // If both sides are constant, evaluate at compile time
    if (expr->left && expr->right && 
        m_evaluator->isConstantExpression(expr->left) && 
        m_evaluator->isConstantExpression(expr->right)) {
        
        auto lit = std::make_shared<LiteralExpression>();
        lit->literalValue = expr->evaluate();
        lit->isConstant = true;
        lit->type = ExpressionType::Literal;
        return lit;
    }
    
    return expr;
}

std::shared_ptr<Expression> ExpressionOptimizer::optimizeUnary(std::shared_ptr<UnaryOpExpression> expr) {
    if (!expr || !expr->operand) return expr;
    
    // If operand is constant, evaluate at compile time
    if (m_evaluator->isConstantExpression(expr->operand)) {
        auto lit = std::make_shared<LiteralExpression>();
        lit->literalValue = expr->evaluate();
        lit->isConstant = true;
        lit->type = ExpressionType::Literal;
        return lit;
    }
    
    return expr;
}

std::shared_ptr<Expression> ExpressionOptimizer::optimizeConditional(std::shared_ptr<ConditionalExpression> expr) {
    if (!expr || !expr->condition) return expr;
    
    // If condition is constant, return only the branch that will be taken
    if (m_evaluator->isConstantExpression(expr->condition)) {
        if (expr->condition->evaluate().toBool()) {
            return expr->trueExpr;
        } else {
            return expr->falseExpr;
        }
    }
    
    return expr;
}
