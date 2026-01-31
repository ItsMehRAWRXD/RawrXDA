(() => {
  const storageKey = 'web_todo_items_v1';
  /** @typedef {{ id:string, text:string, completed:boolean, createdAt:number, updatedAt:number }} Todo */

  const $ = (sel, ctx = document) => ctx.querySelector(sel);
  const $$ = (sel, ctx = document) => Array.from(ctx.querySelectorAll(sel));

  const form = $('#todo-form');
  const input = $('#todo-input');
  const list = $('#todo-list');
  const count = $('#count');
  const hideCompleted = $('#toggle-completed');
  const clearCompletedBtn = $('#clear-completed');

  function loadTodos() {
    try {
      const raw = localStorage.getItem(storageKey);
      if (!raw) return [];
      const data = JSON.parse(raw);
      if (!Array.isArray(data)) return [];
      return data.filter(Boolean);
    } catch {
      return [];
    }
  }

  /** @param {Todo[]} todos */
  function saveTodos(todos) {
    localStorage.setItem(storageKey, JSON.stringify(todos));
  }

  /** @returns {Todo[]} */
  function getTodos() {
    return loadTodos();
  }

  /** @param {Partial<Todo>} patch */
  function createTodo(patch) {
    const now = Date.now();
    return {
      id: crypto.randomUUID(),
      text: '',
      completed: false,
      createdAt: now,
      updatedAt: now,
      ...patch,
    };
  }

  function render() {
    const todos = getTodos();
    const filtered = hideCompleted.checked ? todos.filter(t => !t.completed) : todos;
    list.innerHTML = '';
    for (const todo of filtered) {
      list.appendChild(renderItem(todo));
    }
    const remaining = todos.filter(t => !t.completed).length;
    count.textContent = `${remaining} item${remaining === 1 ? '' : 's'} left`;
  }

  /** @param {Todo} todo */
  function renderItem(todo) {
    const li = document.createElement('li');
    li.className = 'item' + (todo.completed ? ' completed' : '');
    li.dataset.id = todo.id;

    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.checked = todo.completed;
    checkbox.ariaLabel = 'Toggle completed';

    const text = document.createElement('div');
    text.className = 'text';
    text.textContent = todo.text;
    text.contentEditable = 'true';
    text.role = 'textbox';
    text.ariaLabel = 'Edit task';

    const actions = document.createElement('div');
    actions.className = 'actions';
    const delBtn = document.createElement('button');
    delBtn.className = 'icon';
    delBtn.textContent = 'Delete';

    actions.appendChild(delBtn);
    li.appendChild(checkbox);
    li.appendChild(text);
    li.appendChild(actions);

    checkbox.addEventListener('change', () => updateTodo(todo.id, { completed: checkbox.checked }));
    delBtn.addEventListener('click', () => removeTodo(todo.id));

    let saveTimeout;
    text.addEventListener('input', () => {
      clearTimeout(saveTimeout);
      saveTimeout = setTimeout(() => updateTodo(todo.id, { text: text.textContent || '' }), 300);
    });

    text.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        text.blur();
      }
    });

    return li;
  }

  /** @param {string} id @param {Partial<Todo>} patch */
  function updateTodo(id, patch) {
    const todos = getTodos();
    const idx = todos.findIndex(t => t.id === id);
    if (idx === -1) return;
    todos[idx] = { ...todos[idx], ...patch, updatedAt: Date.now() };
    saveTodos(todos);
    render();
  }

  function addTodoFromInput() {
    const text = (input.value || '').trim();
    if (!text) return;
    const todos = getTodos();
    todos.unshift(createTodo({ text }));
    saveTodos(todos);
    input.value = '';
    render();
  }

  /** @param {string} id */
  function removeTodo(id) {
    const todos = getTodos().filter(t => t.id !== id);
    saveTodos(todos);
    render();
  }

  function clearCompleted() {
    const todos = getTodos().filter(t => !t.completed);
    saveTodos(todos);
    render();
  }

  // Events
  form.addEventListener('submit', (e) => {
    e.preventDefault();
    addTodoFromInput();
  });
  hideCompleted.addEventListener('change', render);
  clearCompletedBtn.addEventListener('click', clearCompleted);

  // Init
  render();
})();

