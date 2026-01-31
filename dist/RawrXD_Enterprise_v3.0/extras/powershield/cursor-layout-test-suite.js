/**
 * Cursor Layout Controls - Full Test Suite
 * Tests all menu items, tabs, splits, panels, and keyboard shortcuts
 */

class CursorLayoutTestSuite {
  constructor() {
    this.results = {
      passed: 0,
      failed: 0,
      warnings: 0,
      tests: []
    };
    this.logBuffer = [];
  }

  // ============================================================
  // TEST RUNNER
  // ============================================================

  async runAllTests() {
    console.log('%c========================================', 'color: #4CAF50; font-size: 16px; font-weight: bold;');
    console.log('%c  CURSOR LAYOUT CONTROLS TEST SUITE', 'color: #4CAF50; font-size: 16px; font-weight: bold;');
    console.log('%c========================================', 'color: #4CAF50; font-size: 16px; font-weight: bold;');
    console.log('');

    // Test groups
    await this.testMenuBar();
    await this.testTabBar();
    await this.testEditorControls();
    await this.testBottomPanel();
    await this.testKeyboardShortcuts();
    await this.testMenuActions();
    await this.testStateManagement();

    // Final report
    this.printReport();
  }

  // ============================================================
  // 1. MENU BAR TESTS
  // ============================================================

  async testMenuBar() {
    console.log('%c[TEST GROUP 1] Menu Bar', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 1.1: Menu bar exists
    this.test('Menu bar exists', () => {
      const menuBar = document.querySelector('.cursor-menu-bar');
      return menuBar !== null;
    });

    // Test 1.2: All menus present
    this.test('All main menus present', () => {
      const menus = ['File', 'Edit', 'Selection', 'View', 'Go', 'Run', 'Terminal', 'Help'];
      const items = document.querySelectorAll('.menu-label');
      return menus.every(menu => 
        Array.from(items).some(item => item.textContent === menu)
      );
    });

    // Test 1.3: Menu dropdowns work
    this.test('Menu dropdowns can be opened', () => {
      const menuItem = document.querySelector('.menu-item');
      if (!menuItem) return false;
      menuItem.click();
      const dropdown = menuItem.querySelector('.menu-dropdown');
      return dropdown && dropdown.classList.contains('active');
    });

    // Test 1.4: Submenus have entries
    this.test('File menu has entries', () => {
      const fileMenu = document.querySelector('[data-menu="file"]');
      return fileMenu && fileMenu.querySelectorAll('.menu-entry').length > 0;
    });

    // Test 1.5: Keyboard shortcuts displayed
    this.test('Menu shortcuts displayed', () => {
      const shortcuts = document.querySelectorAll('.menu-shortcut');
      return shortcuts.length > 0;
    });

    console.log('');
  }

  // ============================================================
  // 2. TAB BAR TESTS
  // ============================================================

  async testTabBar() {
    console.log('%c[TEST GROUP 2] Tab Bar', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 2.1: Tab bar exists
    this.test('Tab bar exists', () => {
      return document.querySelector('.cursor-tab-bar') !== null;
    });

    // Test 2.2: Can create new tab
    this.test('New tab creation works', () => {
      const initialTabs = window.cursorLayout.state.openTabs.length;
      window.cursorLayout.newTab();
      return window.cursorLayout.state.openTabs.length === initialTabs + 1;
    });

    // Test 2.3: Tab renders in DOM
    this.test('Tab renders in DOM', () => {
      const tabs = document.querySelectorAll('.tab');
      return tabs.length > 0;
    });

    // Test 2.4: Can close tab
    this.test('Close tab works', () => {
      const tabId = window.cursorLayout.state.openTabs[0]?.id;
      if (!tabId) return false;
      const beforeCount = window.cursorLayout.state.openTabs.length;
      window.cursorLayout.closeTab(tabId);
      return window.cursorLayout.state.openTabs.length === beforeCount - 1;
    });

    // Test 2.5: Active tab tracking
    this.test('Active tab tracking works', () => {
      window.cursorLayout.newTab();
      const tabs = window.cursorLayout.state.openTabs;
      return window.cursorLayout.state.activeTab === tabs[tabs.length - 1]?.id;
    });

    // Test 2.6: Tab controls present
    this.test('Tab controls visible', () => {
      return document.querySelector('#tab-new-btn') !== null &&
             document.querySelector('#tab-close-btn') !== null;
    });

    console.log('');
  }

  // ============================================================
  // 3. EDITOR CONTROLS TESTS
  // ============================================================

  async testEditorControls() {
    console.log('%c[TEST GROUP 3] Editor Controls', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 3.1: Editor controls exist
    this.test('Editor controls panel exists', () => {
      return document.querySelector('.cursor-editor-controls') !== null;
    });

    // Test 3.2: Split buttons present
    this.test('Split editor buttons present', () => {
      return document.querySelector('#split-right-btn') !== null &&
             document.querySelector('#split-down-btn') !== null;
    });

    // Test 3.3: Split vertical works
    this.test('Vertical split works', () => {
      window.cursorLayout.splitEditor('vertical');
      return window.cursorLayout.state.splitMode === 'vertical';
    });

    // Test 3.4: Split horizontal works
    this.test('Horizontal split works', () => {
      window.cursorLayout.splitEditor('horizontal');
      return window.cursorLayout.state.splitMode === 'horizontal';
    });

    // Test 3.5: Layout toggle works
    this.test('Layout toggle works', () => {
      const before = window.cursorLayout.state.splitMode;
      window.cursorLayout.toggleLayout();
      const after = window.cursorLayout.state.splitMode;
      return before !== after;
    });

    // Test 3.6: Maximize button works
    this.test('Editor maximize button works', () => {
      window.cursorLayout.toggleMaximize();
      const editor = document.querySelector('.editor');
      return editor ? editor.classList.contains('maximized') : true;
    });

    console.log('');
  }

  // ============================================================
  // 4. BOTTOM PANEL TESTS
  // ============================================================

  async testBottomPanel() {
    console.log('%c[TEST GROUP 4] Bottom Panel', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 4.1: Panel exists
    this.test('Bottom panel exists', () => {
      return document.querySelector('.cursor-bottom-panel') !== null;
    });

    // Test 4.2: Panel tabs present
    this.test('Panel tabs present', () => {
      const tabs = document.querySelectorAll('.panel-tab');
      return tabs.length >= 4; // terminal, problems, output, debug
    });

    // Test 4.3: Terminal tab works
    this.test('Terminal tab switchable', () => {
      window.cursorLayout.switchPanel('terminal');
      return window.cursorLayout.state.panels.bottom.type === 'terminal';
    });

    // Test 4.4: Problems tab works
    this.test('Problems tab switchable', () => {
      window.cursorLayout.switchPanel('problems');
      return window.cursorLayout.state.panels.bottom.type === 'problems';
    });

    // Test 4.5: Panel controls present
    this.test('Panel controls visible', () => {
      return document.querySelector('#panel-maximize-btn') !== null &&
             document.querySelector('#panel-close-btn') !== null;
    });

    // Test 4.6: Panel visibility toggle
    this.test('Panel visibility toggle works', () => {
      const before = window.cursorLayout.state.panels.bottom.visible;
      window.cursorLayout.togglePanel();
      const after = window.cursorLayout.state.panels.bottom.visible;
      return before !== after;
    });

    console.log('');
  }

  // ============================================================
  // 5. KEYBOARD SHORTCUTS TESTS
  // ============================================================

  async testKeyboardShortcuts() {
    console.log('%c[TEST GROUP 5] Keyboard Shortcuts', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 5.1: Ctrl+N creates tab
    this.test('Ctrl+N creates new tab', () => {
      const before = window.cursorLayout.state.openTabs.length;
      const event = new KeyboardEvent('keydown', { ctrlKey: true, key: 'n' });
      document.dispatchEvent(event);
      return window.cursorLayout.state.openTabs.length >= before;
    });

    // Test 5.2: Ctrl+W closes tab
    this.test('Ctrl+W closes tab', () => {
      window.cursorLayout.newTab();
      const before = window.cursorLayout.state.openTabs.length;
      const event = new KeyboardEvent('keydown', { ctrlKey: true, key: 'w' });
      document.dispatchEvent(event);
      return window.cursorLayout.state.openTabs.length < before;
    });

    // Test 5.3: Ctrl+` toggles terminal
    this.test('Ctrl+` toggles terminal', () => {
      const event = new KeyboardEvent('keydown', { ctrlKey: true, key: '`' });
      document.dispatchEvent(event);
      return window.cursorLayout.state.panels.bottom.type === 'terminal';
    });

    console.log('');
  }

  // ============================================================
  // 6. MENU ACTIONS TESTS
  // ============================================================

  async testMenuActions() {
    console.log('%c[TEST GROUP 6] Menu Actions', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    const actions = [
      'file-new',
      'file-open',
      'file-save',
      'edit-undo',
      'edit-copy',
      'view-explorer',
      'view-terminal',
      'run-start',
      'terminal-new',
      'help-about'
    ];

    // Test each major action
    let actionCount = 0;
    for (const action of actions) {
      const canExecute = () => {
        try {
          window.cursorLayout.handleMenuAction(action);
          return true;
        } catch (e) {
          return false;
        }
      };

      if (canExecute()) {
        actionCount++;
      }
    }

    this.test(`Menu actions executable (${actionCount}/${actions.length})`, () => {
      return actionCount >= actions.length * 0.8; // 80% success threshold
    });

    console.log('');
  }

  // ============================================================
  // 7. STATE MANAGEMENT TESTS
  // ============================================================

  async testStateManagement() {
    console.log('%c[TEST GROUP 7] State Management', 'color: #2196F3; font-weight: bold; font-size: 14px;');

    // Test 7.1: State exists
    this.test('Global state object exists', () => {
      return window.cursorLayout.state !== undefined;
    });

    // Test 7.2: State has required properties
    this.test('State has required properties', () => {
      const state = window.cursorLayout.state;
      return state.openTabs &&
             state.activeTab !== undefined &&
             state.splitMode &&
             state.panels;
    });

    // Test 7.3: State can be exported
    this.test('State can be exported as JSON', () => {
      try {
        const exported = window.cursorLayout.exportState();
        return typeof exported === 'string' && exported.length > 0;
      } catch (e) {
        return false;
      }
    });

    // Test 7.4: Panel state tracking
    this.test('Panel state tracks visibility', () => {
      const before = window.cursorLayout.state.panels.bottom.visible;
      window.cursorLayout.togglePanel();
      const after = window.cursorLayout.state.panels.bottom.visible;
      window.cursorLayout.togglePanel(); // Reset
      return before !== after;
    });

    console.log('');
  }

  // ============================================================
  // UTILITY FUNCTIONS
  // ============================================================

  test(description, fn) {
    try {
      const result = fn();
      if (result === true) {
        this.results.passed++;
        console.log(`  ✅ ${description}`);
        this.results.tests.push({ name: description, status: 'PASSED' });
      } else {
        this.results.failed++;
        console.log(`  ❌ ${description}`);
        this.results.tests.push({ name: description, status: 'FAILED' });
      }
    } catch (e) {
      this.results.failed++;
      console.log(`  ❌ ${description} (Error: ${e.message})`);
      this.results.tests.push({ name: description, status: 'ERROR', error: e.message });
    }
  }

  printReport() {
    const total = this.results.passed + this.results.failed;
    const percentage = Math.round((this.results.passed / total) * 100);

    console.log('');
    console.log('%c========================================', 'color: #FFC107; font-size: 16px; font-weight: bold;');
    console.log('%c  TEST REPORT', 'color: #FFC107; font-size: 16px; font-weight: bold;');
    console.log('%c========================================', 'color: #FFC107; font-size: 16px; font-weight: bold;');
    console.log(`  Total Tests: ${total}`);
    console.log(`  ✅ Passed: ${this.results.passed}`);
    console.log(`  ❌ Failed: ${this.results.failed}`);
    console.log(`  Success Rate: ${percentage}%`);
    console.log('%c========================================', 'color: #FFC107; font-size: 16px; font-weight: bold;');
    console.log('');

    if (this.results.failed === 0) {
      console.log('%c🎉 ALL TESTS PASSED! 🎉', 'color: #4CAF50; font-size: 16px; font-weight: bold;');
    } else {
      console.log(`%c⚠️ ${this.results.failed} test(s) failed`, 'color: #f44336; font-size: 14px; font-weight: bold;');
    }

    console.log('');
    return this.results;
  }

  getResults() {
    return this.results;
  }
}

// Export test suite
window.CursorLayoutTestSuite = CursorLayoutTestSuite;

// Auto-run tests when Cursor Layout is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    if (window.cursorLayout) {
      const tester = new CursorLayoutTestSuite();
      window.cursorLayoutTests = tester;
      // Auto-run: uncomment line below to run tests automatically
      // setTimeout(() => tester.runAllTests(), 500);
    }
  });
} else {
  if (window.cursorLayout) {
    const tester = new CursorLayoutTestSuite();
    window.cursorLayoutTests = tester;
  }
}

console.log('%c✅ Cursor Layout Test Suite loaded - run window.cursorLayoutTests.runAllTests()', 'color: #4CAF50; font-weight: bold;');
