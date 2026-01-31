// Copilot Integration Script for Enhanced TODO System
// This allows GitHub Copilot, Cursor, and other AI assistants to interact with your TODO system

class TodoAPIClient {
    constructor(baseUrl = 'http://localhost:8080') {
        this.baseUrl = baseUrl;
    }

    // Fetch all todos
    async getTodos() {
        try {
            const response = await fetch(`${this.baseUrl}/todos`);
            return await response.json();
        } catch (error) {
            console.error('Failed to fetch todos:', error);
            return { todos: [] };
        }
    }

    // Add a new todo
    async addTodo(text, priority = 'MEDIUM') {
        try {
            const response = await fetch(`${this.baseUrl}/todos/add`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `text=${encodeURIComponent(text)}&priority=${priority}`
            });
            return response.ok;
        } catch (error) {
            console.error('Failed to add todo:', error);
            return false;
        }
    }

    // Get activity report
    async getActivity() {
        try {
            const response = await fetch(`${this.baseUrl}/activity`);
            return await response.text();
        } catch (error) {
            console.error('Failed to fetch activity:', error);
            return 'Activity data unavailable';
        }
    }

    // Smart todo suggestions based on code analysis
    async suggestTodos(codeContext) {
        const suggestions = [];
        
        // Analyze code patterns and suggest todos
        if (codeContext.includes('TODO:') || codeContext.includes('FIXME:')) {
            suggestions.push('Review and address existing TODO/FIXME comments');
        }
        
        if (codeContext.includes('console.log') || codeContext.includes('System.out.println')) {
            suggestions.push('Remove debug logging statements');
        }
        
        if (codeContext.includes('catch (Exception e)') && !codeContext.includes('logger')) {
            suggestions.push('Add proper error logging to exception handlers');
        }
        
        if (codeContext.includes('public class') && !codeContext.includes('@Test')) {
            suggestions.push('Add unit tests for new classes');
        }
        
        if (codeContext.includes('SELECT *') || codeContext.includes('select *')) {
            suggestions.push('Optimize SQL queries to select specific columns');
        }
        
        return suggestions;
    }

    // Batch operations for productivity
    async addMultipleTodos(todos) {
        const results = [];
        for (const todo of todos) {
            const result = await this.addTodo(todo.text, todo.priority || 'MEDIUM');
            results.push({ todo: todo.text, success: result });
        }
        return results;
    }
}

// VS Code Extension Integration
class VSCodeTodoIntegration {
    constructor() {
        this.client = new TodoAPIClient();
    }

    // Analyze current file and suggest todos
    async analyzeCurrentFile() {
        // This would integrate with VS Code API
        const activeEditor = vscode.window.activeTextEditor;
        if (activeEditor) {
            const document = activeEditor.document;
            const text = document.getText();
            
            const suggestions = await this.client.suggestTodos(text);
            
            // Show suggestions to user
            const selected = await vscode.window.showQuickPick(suggestions, {
                placeHolder: 'Select todos to add',
                canPickMany: true
            });
            
            if (selected) {
                for (const suggestion of selected) {
                    await this.client.addTodo(suggestion, 'MEDIUM');
                }
                vscode.window.showInformationMessage(`Added ${selected.length} todos`);
            }
        }
    }

    // Show todos in VS Code sidebar
    async showTodosInSidebar() {
        const todos = await this.client.getTodos();
        // Display in VS Code tree view or webview
    }
}

// GitHub Copilot Chat Integration
class CopilotChatIntegration {
    constructor() {
        this.client = new TodoAPIClient();
    }

    // Process natural language todo requests
    async processTodoRequest(message) {
        const patterns = {
            add: /add todo[:\s]+(.*)/i,
            list: /show|list|get todos/i,
            priority: /(high|medium|low|critical) priority/i
        };

        if (patterns.add.test(message)) {
            const match = message.match(patterns.add);
            const todoText = match[1];
            const priority = patterns.priority.test(message) ? 
                message.match(patterns.priority)[1].toUpperCase() : 'MEDIUM';
            
            const success = await this.client.addTodo(todoText, priority);
            return success ? `Added todo: ${todoText}` : 'Failed to add todo';
        }

        if (patterns.list.test(message)) {
            const todos = await this.client.getTodos();
            return this.formatTodoList(todos.todos);
        }

        return 'I can help you manage todos. Try "add todo: your task" or "show todos"';
    }

    formatTodoList(todos) {
        if (todos.length === 0) return 'No todos found';
        
        return todos.map((todo, index) => 
            `${index + 1}. ${todo.completed ? '✅' : '⭕'} ${todo.text} [${todo.priority}]`
        ).join('\n');
    }
}

// Cursor IDE Integration
class CursorIntegration {
    constructor() {
        this.client = new TodoAPIClient();
    }

    // Smart code analysis for todo generation
    async analyzeCodeForTodos(filePath, content) {
        const todos = [];
        
        // Complexity analysis
        const cyclomaticComplexity = this.calculateComplexity(content);
        if (cyclomaticComplexity > 10) {
            todos.push({
                text: `Refactor ${filePath} - high complexity (${cyclomaticComplexity})`,
                priority: 'HIGH'
            });
        }

        // Security analysis
        if (content.includes('eval(') || content.includes('innerHTML =')) {
            todos.push({
                text: `Security review needed for ${filePath}`,
                priority: 'CRITICAL'
            });
        }

        // Performance analysis
        if (content.includes('for (') && content.includes('for (')) {
            todos.push({
                text: `Optimize nested loops in ${filePath}`,
                priority: 'MEDIUM'
            });
        }

        return todos;
    }

    calculateComplexity(code) {
        // Simplified cyclomatic complexity calculation
        const patterns = ['if', 'else', 'while', 'for', 'switch', 'case', 'catch'];
        return patterns.reduce((count, pattern) => 
            count + (code.match(new RegExp(pattern, 'g')) || []).length, 1);
    }
}

// Export for use in different environments
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        TodoAPIClient,
        VSCodeTodoIntegration,
        CopilotChatIntegration,
        CursorIntegration
    };
}

// Browser global
if (typeof window !== 'undefined') {
    window.TodoIntegration = {
        TodoAPIClient,
        VSCodeTodoIntegration,
        CopilotChatIntegration,
        CursorIntegration
    };
}

// Example usage:
/*
const client = new TodoAPIClient();

// Add todos from Copilot
await client.addTodo('Implement user authentication', 'HIGH');
await client.addTodo('Add input validation', 'MEDIUM');

// Get current todos
const todos = await client.getTodos();
console.log('Current todos:', todos);

// Batch add from code analysis
const suggestions = await client.suggestTodos(codeContent);
await client.addMultipleTodos(suggestions.map(s => ({ text: s, priority: 'MEDIUM' })));
*/