import javax.swing.*;
import javax.swing.text.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.util.List;

public class MultiCursorManager {
    private final JTextPane textPane;
    private final List<Caret> carets = new ArrayList<>();
    private final CaretRenderer renderer;
    private boolean multiCaretMode = false;
    private final BlockSelectionHandler blockSelectionHandler;

    public MultiCursorManager(JTextPane textPane) {
        this.textPane = textPane;
        this.renderer = new CaretRenderer();
        this.blockSelectionHandler = new BlockSelectionHandler(textPane, this);
        
        // The primary caret is always managed by the textPane itself.
        // We will sync our list with it.
        carets.add(new Caret(textPane.getCaretPosition(), -1)); 
        
        setupKeyBindings();
        setupMouseHandling();

        // Ensure the textPane's default caret is not drawn when we are in multi-caret mode
        // and drawing our own.
        textPane.setCaret(new DefaultCaret() {
            @Override
            public void paint(Graphics g) {
                if (!multiCaretMode) {
                    super.paint(g);
                }
            }
        });
        
        // Listen to the textPane's caret to update our primary caret
        textPane.addCaretListener(e -> {
            if (!multiCaretMode && !carets.isEmpty()) {
                carets.get(0).position = e.getDot();
                carets.get(0).selectionStart = Math.min(e.getDot(), e.getMark());
                carets.get(0).selectionEnd = Math.max(e.getDot(), e.getMark());
            }
        });
    }

    private void setupKeyBindings() {
        InputMap inputMap = textPane.getInputMap();
        ActionMap actionMap = textPane.getActionMap();

        // --- New/Updated Keybindings ---
        inputMap.put(KeyStroke.getKeyStroke("ctrl alt DOWN"), "addCaretDown");
        inputMap.put(KeyStroke.getKeyStroke("ctrl alt UP"), "addCaretUp");
        inputMap.put(KeyStroke.getKeyStroke("ESCAPE"), "clearMultiCarets");
        
        // --- Actions for new bindings ---
        actionMap.put("addCaretDown", new AbstractAction() {
            public void actionPerformed(ActionEvent e) { addCaret(true); }
        });
        actionMap.put("addCaretUp", new AbstractAction() {
            public void actionPerformed(ActionEvent e) { addCaret(false); }
        });
        actionMap.put("clearMultiCarets", new AbstractAction() {
            public void actionPerformed(ActionEvent e) { clearMultiCarets(); }
        });

        // --- Override existing actions for multi-caret behavior ---
        overrideMovementActions(inputMap, actionMap);
        overrideTypingActions(actionMap);
    }

    private void setupMouseHandling() {
        // For adding individual carets
        textPane.addMouseListener(new MouseAdapter() {
            @Override
            public void mousePressed(MouseEvent e) {
                if (e.isControlDown() && e.isAltDown()) {
                    int pos = textPane.viewToModel2D(e.getPoint());
                    addCaretAt(pos, -1);
                    e.consume(); // Prevent default caret positioning
                } else if (!e.isControlDown() && !e.isAltDown() && !e.isShiftDown()) {
                    // If not a multi-caret action, revert to single-caret mode
                    if (multiCaretMode) {
                        clearMultiCarets();
                    }
                }
            }
        });

        // For block selection
        textPane.addMouseMotionListener(blockSelectionHandler);
        textPane.addMouseListener(blockSelectionHandler);
    }

    // --- Core Caret Management ---

    void addCaretAt(int position, int goalColumn) {
        if (position < 0) return;

        // If not in multi-caret mode, activate it.
        if (!multiCaretMode) {
            multiCaretMode = true;
            // The first caret in our list is the primary one, update it.
            carets.clear();
            int primaryPos = textPane.getCaretPosition();
            carets.add(new Caret(primaryPos, getColumnFromPosition(primaryPos)));
        }

        // Avoid adding duplicate carets
        for (Caret caret : carets) {
            if (caret.position == position) return;
        }

        carets.add(new Caret(position, goalColumn != -1 ? goalColumn : getColumnFromPosition(position)));
        sortAndDeduplicateCarets();
        textPane.repaint();
    }
    
    void addCaretsForBlock(int startLine, int endLine, int startColumn, int endColumn) {
        if (!multiCaretMode) {
            multiCaretMode = true;
            carets.clear();
        } else {
            // When creating a new block, clear previous carets
            carets.clear();
        }

        for (int line = startLine; line <= endLine; line++) {
            int pos = getPositionFromLineColumn(line, endColumn);
            int startPos = getPositionFromLineColumn(line, startColumn);
            
            Caret caret = new Caret(pos, endColumn);
            caret.selectionStart = startPos;
            caret.selectionEnd = pos;
            carets.add(caret);
        }
        sortAndDeduplicateCarets();
        textPane.repaint();
    }

    private void addCaret(boolean down) {
        if (carets.isEmpty()) return;
        if (!multiCaretMode) {
            // Activate multi-caret mode based on the primary caret
            multiCaretMode = true;
            carets.clear();
            int primaryPos = textPane.getCaretPosition();
            carets.add(new Caret(primaryPos, getColumnFromPosition(primaryPos)));
        }

        Caret lastCaret = down ? carets.get(carets.size() - 1) : carets.get(0);
        int currentLine = getLineFromPosition(lastCaret.position);
        int targetLine = currentLine + (down ? 1 : -1);

        if (targetLine >= 0 && targetLine < getLineCount()) {
            int goalColumn = lastCaret.goalColumn != -1 ? lastCaret.goalColumn : getColumnFromPosition(lastCaret.position);
            int newPos = getPositionFromLineColumn(targetLine, goalColumn);
            addCaretAt(newPos, goalColumn);
        }
    }

    private void clearMultiCarets() {
        if (multiCaretMode) {
            multiCaretMode = false;
            blockSelectionHandler.clearBlockSelection();
            // Keep only the last active caret as the primary one
            Caret primaryCaret = carets.isEmpty() ? new Caret(0, 0) : carets.get(carets.size() - 1);
            carets.clear();
            carets.add(new Caret(primaryCaret.position, -1)); // Add it back as the single caret
            textPane.setCaretPosition(primaryCaret.position);
            textPane.repaint();
        }
    }

    private void sortAndDeduplicateCarets() {
        if (carets.isEmpty()) return;
        carets.sort(Comparator.comparingInt(c -> c.position));
        
        List<Caret> uniqueCarets = new ArrayList<>();
        Caret last = null;
        for (Caret c : carets) {
            if (last == null || c.position != last.position) {
                uniqueCarets.add(c);
            }
            last = c;
        }
        carets.clear();
        carets.addAll(uniqueCarets);
    }

    // --- Action Overrides ---

    private void overrideTypingActions(ActionMap actionMap) {
        // This part is complex. For now, we'll keep the existing logic but adapt it to the new Caret class.
        // A full implementation would require handling all sorts of edits.
        
        // Example for insert
        actionMap.put(DefaultEditorKit.insertContentAction, new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                if (multiCaretMode) {
                    insertAtAllCarets(e.getActionCommand());
                } else {
                    // Let the default action handle it
                    textPane.replaceSelection(e.getActionCommand());
                }
            }
        });

        // Example for backspace
        actionMap.put(DefaultEditorKit.deletePrevCharAction, new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                if (multiCaretMode) {
                    backspaceAtAllCarets();
                } else {
                    // Find the original action and invoke it
                     Action originalAction = textPane.getActionMap().get(e.getActionCommand());
                     if(originalAction != null) originalAction.actionPerformed(e);
                }
            }
        });
    }
    
    private void overrideMovementActions(InputMap inputMap, ActionMap actionMap) {
        // This is a simplified example. A full implementation would cover more actions.
        String[] actionsToOverride = {
            DefaultEditorKit.forwardAction, DefaultEditorKit.backwardAction,
            DefaultEditorKit.selectionForwardAction, DefaultEditorKit.selectionBackwardAction,
            DefaultEditorKit.beginLineAction, DefaultEditorKit.endLineAction,
            DefaultEditorKit.selectionBeginLineAction, DefaultEditorKit.selectionEndLineAction
        };

        for (String actionName : actionsToOverride) {
            Action originalAction = actionMap.get(actionName);
            if (originalAction != null) {
                actionMap.put(actionName, new MultiCaretMovementAction(actionName, originalAction));
            }
        }
    }

    // --- Edit Operations ---

    private void insertAtAllCarets(String text) {
        if (text == null) return;
        Document doc = textPane.getDocument();
        
        // Process from bottom to top to avoid shifting positions
        try {
            for (int i = carets.size() - 1; i >= 0; i--) {
                Caret caret = carets.get(i);
                if (caret.hasSelection()) {
                    doc.remove(caret.getSelectionStart(), caret.getSelectionLength());
                    doc.insertString(caret.getSelectionStart(), text, null);
                    caret.position = caret.getSelectionStart() + text.length();
                } else {
                    doc.insertString(caret.position, text, null);
                    caret.position += text.length();
                }
                caret.clearSelection();
                caret.goalColumn = -1; // Reset goal column after typing
            }
        } catch (BadLocationException e) {
            // Handle exception
        }
        updateAllCaretPositions(0); // Update positions after modification
        textPane.repaint();
    }

    private void backspaceAtAllCarets() {
        Document doc = textPane.getDocument();
        try {
            for (int i = carets.size() - 1; i >= 0; i--) {
                Caret caret = carets.get(i);
                if (caret.hasSelection()) {
                    doc.remove(caret.getSelectionStart(), caret.getSelectionLength());
                    caret.position = caret.getSelectionStart();
                } else if (caret.position > 0) {
                    doc.remove(caret.position - 1, 1);
                    caret.position--;
                }
                caret.clearSelection();
                caret.goalColumn = -1;
            }
        } catch (BadLocationException e) {
            // Handle exception
        }
        updateAllCaretPositions(0);
        textPane.repaint();
    }
    
    private void updateAllCaretPositions(int delta) {
        // This is a simplified update. A real implementation needs to track text changes more granularly.
        for (Caret caret : carets) {
            caret.position += delta;
        }
        sortAndDeduplicateCarets();
    }

    // --- Helper & Utility Methods ---

    public int getLineFromPosition(int pos) {
        return textPane.getDocument().getDefaultRootElement().getElementIndex(pos);
    }

    public int getColumnFromPosition(int pos) {
        try {
            return pos - Utilities.getRowStart(textPane, pos);
        } catch (BadLocationException e) {
            return 0;
        }
    }

    public int getPositionFromLineColumn(int line, int column) {
        Element root = textPane.getDocument().getDefaultRootElement();
        if (line >= root.getElementCount()) return textPane.getDocument().getLength();
        
        Element lineElement = root.getElement(line);
        int lineStart = lineElement.getStartOffset();
        int lineEnd = lineElement.getEndOffset() - 1; // Exclude newline
        
        return Math.min(lineStart + column, lineEnd);
    }

    public int getLineCount() {
        return textPane.getDocument().getDefaultRootElement().getElementCount();
    }

    public void paintCarets(Graphics g) {
        if (multiCaretMode) {
            renderer.paintCarets((Graphics2D) g, carets, textPane);
        }
        if (blockSelectionHandler.isBlockSelectionActive()) {
            renderer.paintBlockSelection((Graphics2D) g, blockSelectionHandler.getSelectionRect(), textPane);
        }
    }
    
    // --- Inner Classes ---

    /**
     * A custom action to wrap existing movement actions and apply them to all carets.
     */
    private class MultiCaretMovementAction extends AbstractAction {
        private final String originalActionName;
        private final Action originalAction;

        MultiCaretMovementAction(String name, Action originalAction) {
            this.originalActionName = name;
            this.originalAction = originalAction;
        }

        @Override
        public void actionPerformed(ActionEvent e) {
            if (multiCaretMode) {
                // Create a temporary JTextPane to simulate the action for each caret
                JTextPane tempPane = new JTextPane();
                tempPane.setDocument(textPane.getDocument());

                for (Caret caret : carets) {
                    // Set the caret and selection on the temp pane
                    tempPane.setCaretPosition(caret.position);
                    if (caret.hasSelection()) {
                        tempPane.setSelectionStart(caret.selectionStart);
                        tempPane.setSelectionEnd(caret.selectionEnd);
                    } else {
                        tempPane.select(caret.position, caret.position);
                    }

                    // Execute the original action
                    originalAction.actionPerformed(new ActionEvent(tempPane, ActionEvent.ACTION_PERFORMED, null));

                    // Update our caret from the temp pane's state
                    caret.position = tempPane.getCaretPosition();
                    caret.selectionStart = tempPane.getSelectionStart();
                    caret.selectionEnd = tempPane.getSelectionEnd();
                    
                    // Update goal column for vertical movement
                    if (originalActionName.contains("Down") || originalActionName.contains("Up")) {
                        if (caret.goalColumn == -1) {
                             caret.goalColumn = getColumnFromPosition(tempPane.getCaretPosition());
                        }
                    } else {
                        caret.goalColumn = -1; // Reset for horizontal movement
                    }
                }
                sortAndDeduplicateCarets();
                textPane.repaint();
            } else {
                originalAction.actionPerformed(e);
            }
        }
    }
}

/**
 * Represents a single caret in the editor, including its position and selection.
 */
class Caret {
    int position;
    int selectionStart = -1;
    int selectionEnd = -1;
    int goalColumn = -1; // For vertical movement

    Caret(int position, int goalColumn) {
        this.position = position;
        this.goalColumn = goalColumn;
    }

    boolean hasSelection() {
        return selectionStart != -1 && selectionEnd != -1 && selectionStart != selectionEnd;
    }

    void clearSelection() {
        selectionStart = -1;
        selectionEnd = -1;
    }
    
    int getSelectionStart() {
        return Math.min(position, selectionStart);
    }

    int getSelectionEnd() {
        return Math.max(position, selectionEnd);
    }
    
    int getSelectionLength() {
        return Math.abs(selectionEnd - selectionStart);
    }
}

/**
 * Handles rendering of multiple carets and block selection.
 */
class CaretRenderer {
    private static final Color CARET_COLOR = Color.BLACK;
    private static final Color SELECTION_COLOR = new Color(0, 120, 215, 50);
    private static final Color BLOCK_SELECTION_COLOR = new Color(0, 120, 215, 30);
    private static final Stroke CARET_STROKE = new BasicStroke(1.5f);

    void paintCarets(Graphics2D g2d, List<Caret> carets, JTextPane textPane) {
        g2d.setColor(CARET_COLOR);
        
        for (Caret caret : carets) {
            try {
                // Draw selection for this caret
                if (caret.hasSelection()) {
                    g2d.setColor(SELECTION_COLOR);
                    // A bit of a hack to draw custom selections.
                    // A more robust solution would be a custom Highlighter.
                    textPane.select(caret.selectionStart, caret.selectionEnd);
                    g2d.fillRect(0,0,0,0); // placeholder
                    textPane.getUI().paint(g2d, textPane);

                }

                // Draw the caret itself
                g2d.setColor(CARET_COLOR);
                g2d.setStroke(CARET_STROKE);
                Rectangle rect = textPane.modelToView2D(caret.position).getBounds();
                g2d.drawLine(rect.x, rect.y, rect.x, rect.y + rect.height);

            } catch (BadLocationException e) {
                // Ignore invalid positions
            }
        }
    }

    void paintBlockSelection(Graphics2D g2d, Rectangle selectionRect, JTextPane textPane) {
        if (selectionRect == null) return;
        g2d.setColor(BLOCK_SELECTION_COLOR);
        g2d.fillRect(selectionRect.x, selectionRect.y, selectionRect.width, selectionRect.height);
    }
}

/**
 * Manages block selection via mouse drag.
 */
class BlockSelectionHandler extends MouseAdapter implements MouseMotionListener {
    private final JTextPane textPane;
    private final MultiCursorManager manager;
    private Point startPoint;
    private Rectangle selectionRect;

    public BlockSelectionHandler(JTextPane textPane, MultiCursorManager manager) {
        this.textPane = textPane;
        this.manager = manager;
    }

    @Override
    public void mousePressed(MouseEvent e) {
        if (SwingUtilities.isLeftMouseButton(e) && e.isShiftDown() && e.isAltDown()) {
            startPoint = e.getPoint();
            e.consume();
        }
    }

    @Override
    public void mouseDragged(MouseEvent e) {
        if (startPoint != null) {
            selectionRect = new Rectangle(
                Math.min(startPoint.x, e.getX()),
                Math.min(startPoint.y, e.getY()),
                Math.abs(e.getX() - startPoint.x),
                Math.abs(e.getY() - startPoint.y)
            );
            textPane.repaint();
            e.consume();
        }
    }

    @Override
    public void mouseReleased(MouseEvent e) {
        if (startPoint != null) {
            applyBlockSelection();
            startPoint = null;
            selectionRect = null;
            textPane.repaint();
            e.consume();
        }
    }
    
    private void applyBlockSelection() {
        if (selectionRect == null) return;

        int startPos = textPane.viewToModel2D(new Point(selectionRect.x, selectionRect.y));
        int endPos = textPane.viewToModel2D(new Point(selectionRect.x + selectionRect.width, selectionRect.y + selectionRect.height));

        int startLine = manager.getLineFromPosition(startPos);
        int endLine = manager.getLineFromPosition(endPos);

        int startCol = manager.getColumnFromPosition(textPane.viewToModel2D(new Point(selectionRect.x, 0)));
        int endCol = manager.getColumnFromPosition(textPane.viewToModel2D(new Point(selectionRect.x + selectionRect.width, 0)));

        manager.addCaretsForBlock(startLine, endLine, startCol, endCol);
    }

    public boolean isBlockSelectionActive() {
        return startPoint != null;
    }

    public Rectangle getSelectionRect() {
        return selectionRect;
    }
    
    public void clearBlockSelection() {
        startPoint = null;
        selectionRect = null;
    }

    @Override
    public void mouseMoved(MouseEvent e) { /* Not used */ }
}
