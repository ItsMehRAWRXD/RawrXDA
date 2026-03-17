#ifndef DEBUGEXPRESSIONEVALUATOR_H
#define DEBUGEXPRESSIONEVALUATOR_H

#include <QString>
#include <QMap>
#include <QObject>
#include <QVariant>
#include <QJsonObject>
#include <memory>
#include <functional>
#include <cstdint>

enum class ExpressionType {
    Literal,
    Variable,
    MemberAccess,
    ArrayAccess,
    FunctionCall,
    BinaryOp,
    UnaryOp,
    Conditional,
    Cast
};

enum class OperatorType {
    Add, Subtract, Multiply, Divide, Modulo,
    Equal, NotEqual, GreaterThan, LessThan, GreaterOrEqual, LessOrEqual,
    And, Or, Not,
    BitwiseAnd, BitwiseOr, BitwiseXor, BitwiseNot,
    LeftShift, RightShift,
    Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign,
    Increment, Decrement,
    Dereference, AddressOf
};

// Represents an abstract expression in the evaluation tree
struct Expression {
    int id;
    ExpressionType type;
    QString sourceExpression;
    QVariant value;
    QString dataType;
    bool isConstant;
    QString errorMessage;
    
    virtual ~Expression() = default;
    virtual QJsonObject toJson() const;
    virtual QVariant evaluate() const;
};

// Literal expression (e.g., 42, "hello", 3.14)
struct LiteralExpression : public Expression {
    QVariant literalValue;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Variable reference expression (e.g., x, myVar)
struct VariableExpression : public Expression {
    QString variableName;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Member access expression (e.g., obj.member, ptr->member)
struct MemberExpression : public Expression {
    QString objectName;
    QString memberName;
    bool isPointer;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Array/pointer access expression (e.g., arr[5], ptr[10])
struct ArrayAccessExpression : public Expression {
    QString arrayName;
    int index;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Binary operation expression (e.g., a + b, x > 5)
struct BinaryOpExpression : public Expression {
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    OperatorType op;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Unary operation expression (e.g., !flag, -value, *ptr)
struct UnaryOpExpression : public Expression {
    std::shared_ptr<Expression> operand;
    OperatorType op;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Conditional (ternary) expression (e.g., cond ? trueVal : falseVal)
struct ConditionalExpression : public Expression {
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> trueExpr;
    std::shared_ptr<Expression> falseExpr;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Function call expression (e.g., strlen(str), getValue())
struct FunctionCallExpression : public Expression {
    QString functionName;
    QList<std::shared_ptr<Expression>> arguments;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Type cast expression (e.g., (int)ptr, (float)x)
struct CastExpression : public Expression {
    std::shared_ptr<Expression> operand;
    QString targetType;
    
    QJsonObject toJson() const override;
    QVariant evaluate() const override;
};

// Main expression evaluator and parser
class DebugExpressionEvaluator : public QObject {
    Q_OBJECT

public:
    explicit DebugExpressionEvaluator(QObject* parent = nullptr);
    ~DebugExpressionEvaluator() override = default;

    // Expression parsing
    std::shared_ptr<Expression> parseExpression(const QString& expression);
    std::shared_ptr<Expression> parseVariable(const QString& varName);
    std::shared_ptr<Expression> parseLiteral(const QString& literal);
    std::shared_ptr<Expression> parseBinaryOperation(const QString& expression);
    std::shared_ptr<Expression> parseUnaryOperation(const QString& expression);
    std::shared_ptr<Expression> parseConditional(const QString& expression);
    std::shared_ptr<Expression> parseArrayAccess(const QString& expression);
    std::shared_ptr<Expression> parseMemberAccess(const QString& expression);
    std::shared_ptr<Expression> parseFunctionCall(const QString& expression);
    std::shared_ptr<Expression> parseCast(const QString& expression);

    // Expression evaluation
    QVariant evaluateExpression(const QString& expression);
    QVariant evaluateExpression(std::shared_ptr<Expression> expr);
    bool evaluateCondition(const QString& condition);
    QString evaluateToString(const QString& expression);
    int evaluateToInt(const QString& expression);
    double evaluateToDouble(const QString& expression);
    
    // Variable context management
    void setVariable(const QString& name, const QVariant& value);
    QVariant getVariable(const QString& name) const;
    bool hasVariable(const QString& name) const;
    void removeVariable(const QString& name);
    void clearVariables();
    QMap<QString, QVariant> getAllVariables() const;
    
    // Watch expression management (for breakpoint conditions)
    int addWatchExpression(const QString& expression);
    bool removeWatchExpression(int id);
    QString getWatchExpression(int id) const;
    QVariant evaluateWatch(int id);
    QMap<int, QString> getAllWatchExpressions() const;
    QList<QVariant> evaluateAllWatches();
    
    // Custom function support
    void registerFunction(const QString& name, std::function<QVariant(const QList<QVariant>&)> func);
    bool hasCustomFunction(const QString& name) const;
    QVariant callCustomFunction(const QString& name, const QList<QVariant>& args);
    QStringList getAllCustomFunctions() const;
    
    // Memory operations
    void setMemoryValue(uintptr_t address, const QByteArray& value);
    QByteArray readMemory(uintptr_t address, size_t size);
    void setPointerVariable(const QString& name, uintptr_t address);
    
    // Type inference and checking
    QString inferType(const QVariant& value) const;
    QString inferType(const std::shared_ptr<Expression>& expr) const;
    bool isTypeCompatible(const QString& type1, const QString& type2) const;
    QVariant castValue(const QVariant& value, const QString& targetType);
    
    // Expression validation
    bool validateExpression(const QString& expression) const;
    bool validateVariableNames(const QString& expression) const;
    QString getLastError() const;
    
    // Optimization
    std::shared_ptr<Expression> optimizeExpression(std::shared_ptr<Expression> expr);
    bool isConstantExpression(std::shared_ptr<Expression> expr) const;
    
    // Caching
    void enableExpressionCaching(bool enabled);
    void clearExpressionCache();
    int getCacheSize() const;
    
    // Debug support
    QString expressionToString(std::shared_ptr<Expression> expr) const;
    void dumpExpressionTree(std::shared_ptr<Expression> expr) const;
    
    // Persistence
    void saveVariables(const QString& filePath) const;
    void loadVariables(const QString& filePath);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

signals:
    void expressionEvaluated(int id, const QVariant& result);
    void watchTriggered(int id, const QVariant& oldValue, const QVariant& newValue);
    void evaluationError(const QString& expression, const QString& error);
    void variableChanged(const QString& name, const QVariant& oldValue, const QVariant& newValue);

private:
    QMap<QString, QVariant> m_variables;
    QMap<int, QString> m_watchExpressions;
    QMap<QString, std::function<QVariant(const QList<QVariant>&)>> m_customFunctions;
    int m_nextWatchId;
    QString m_lastError;
    bool m_cachingEnabled;
    QMap<QString, std::shared_ptr<Expression>> m_expressionCache;
    QMap<int, QVariant> m_watchValues;
    
    // Parser helpers
    bool isOperator(const QString& token) const;
    OperatorType stringToOperator(const QString& op) const;
    QString operatorToString(OperatorType op) const;
    int getOperatorPrecedence(OperatorType op) const;
    bool isLeftAssociative(OperatorType op) const;
    
    // Tokenizer
    QStringList tokenize(const QString& expression) const;
    bool isValidIdentifier(const QString& name) const;
    bool isValidNumber(const QString& str) const;
    bool isValidString(const QString& str) const;
    
    // Expression builders
    std::shared_ptr<Expression> buildExpressionFromTokens(const QStringList& tokens);
    std::shared_ptr<Expression> shuntingYardAlgorithm(const QStringList& tokens);
    
    // Value conversion helpers
    QVariant stringToVariant(const QString& str) const;
    QString variantToString(const QVariant& var) const;
};

// Expression optimizer for compile-time evaluation
class ExpressionOptimizer {
public:
    explicit ExpressionOptimizer(DebugExpressionEvaluator* evaluator);
    
    std::shared_ptr<Expression> optimize(std::shared_ptr<Expression> expr);
    
private:
    DebugExpressionEvaluator* m_evaluator;
    
    std::shared_ptr<Expression> optimizeBinary(std::shared_ptr<BinaryOpExpression> expr);
    std::shared_ptr<Expression> optimizeUnary(std::shared_ptr<UnaryOpExpression> expr);
    std::shared_ptr<Expression> optimizeConditional(std::shared_ptr<ConditionalExpression> expr);
};

#endif // DEBUGEXPRESSIONEVALUATOR_H
