import javax.swing.*;
import javax.swing.text.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

/**
 * Manages code folding by manipulating text attributes rather than replacing text.
 * This is a more robust and efficient approach.
 */
public class CodeFoldingManager {
    final JTextPane textPane;
    private final FoldingGutter gutter;
    final Map<Integer, FoldRegion> foldRegions = new HashMap<>();

    public CodeFoldingManager(JTextPane textPane) {
        this.textPane = textPane;
        this.gutter = new FoldingGutter(this);
        setupKeyBindings();
        // Initial analysis is now triggered by onTextChanged in AdvancedCodeEditor
    }

    public void updateFolding() {
        analyzeFoldableRegions();
        gutter.repaint();
    }

    private void analyzeFoldableRegions() {
        Map<Integer, FoldRegion> newFoldRegions = new HashMap<>();
        Document doc = textPane.getDocument();
        Element root = doc.getDefaultRootElement();
        String text;
        try {
            text = doc.getText(0, doc.getLength());
        } catch (BadLocationException e) {
            e.printStackTrace();
            return;
        }

        String[] lines = text.split("\n");
        Stack<Integer> braceStack = new Stack<>();

        for (int i = 0; i < lines.length; i++) {
            String line = lines[i];
            if (line.contains("{")) {
                braceStack.push(i);
            }
            if (line.contains("}") && !braceStack.isEmpty()) {
                int startLine = braceStack.pop();
                if (i > startLine + 1) { // Ensure it's a multi-line block of at least 3 lines
                    newFoldRegions.put(startLine, new FoldRegion(startLine, i, FoldType.BRACE));
                }
            }
        }
        
        // Preserve collapsed state
        for(Map.Entry<Integer, FoldRegion> entry : newFoldRegions.entrySet()) {
            if(foldRegions.containsKey(entry.getKey()) && foldRegions.get(entry.getKey()).collapsed) {
                entry.getValue().collapsed = true;
                // Re-apply folding attribute for the new region
                setFolding(entry.getValue(), true, false); 
            }
        }
        
        foldRegions.clear();
        foldRegions.putAll(newFoldRegions);

        // We need to force a re-render of the document view.
        if (doc instanceof StyledDocument) {
             ((StyledDocument)doc).fireChangedUpdate(doc.getDefaultRootElement());
        }
        gutter.repaint();
    }

    public void toggleFold(int line) {
        FoldRegion region = foldRegions.get(line);
        if (region != null) {
            region.collapsed = !region.collapsed;
            setFolding(region, region.collapsed, true);
        }
    }

    public void foldAll() {
        for (FoldRegion region : foldRegions.values()) {
            if (!region.collapsed) {
                region.collapsed = true;
                setFolding(region, true, false);
            }
        }
        if (textPane.getDocument() instanceof StyledDocument) {
            ((StyledDocument)textPane.getDocument()).fireChangedUpdate(textPane.getDocument().getDefaultRootElement());
        }
    }

    public void unfoldAll() {
        for (FoldRegion region : foldRegions.values()) {
            if (region.collapsed) {
                region.collapsed = false;
                setFolding(region, false, false);
            }
        }
         if (textPane.getDocument() instanceof StyledDocument) {
            ((StyledDocument)textPane.getDocument()).fireChangedUpdate(textPane.getDocument().getDefaultRootElement());
        }
    }

    private void setFolding(FoldRegion region, boolean fold, boolean repaint) {
        if (!(textPane.getDocument() instanceof StyledDocument)) return;
        
        StyledDocument doc = (StyledDocument) textPane.getDocument();
        Element root = doc.getDefaultRootElement();

        // The attribute change must be done on the paragraph elements
        for (int line = region.startLine + 1; line < region.endLine; line++) {
            if (line < root.getElementCount()) {
                Element paraElement = root.getElement(line);
                MutableAttributeSet attrs = (MutableAttributeSet) paraElement.getAttributes();
                attrs.addAttribute(FoldableStyledEditorKit.FOLDED_ATTRIBUTE, fold);
            }
        }

        if (repaint) {
            // We need to force a re-render of the document view.
            doc.fireChangedUpdate(doc.getDefaultRootElement());
            gutter.repaint();
        }
    }

    private void setupKeyBindings() {
        // InputMap is retrieved in AdvancedCodeEditor, actions are mapped here.
        ActionMap actionMap = textPane.getActionMap();

        // Use existing actions from AdvancedCodeEditor if they exist, otherwise create new ones
        actionMap.put("foldCode", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                int line = getCaretLine();
                for (int i = line; i >= 0; i--) {
                    if (foldRegions.containsKey(i)) {
                        toggleFold(i);
                        break;
                    }
                }
            }
        });

        actionMap.put("unfoldCode", new AbstractAction() {
             public void actionPerformed(ActionEvent e) {
                int line = getCaretLine();
                FoldRegion containingRegion = findContainingRegion(line);
                if (containingRegion != null) {
                     toggleFold(containingRegion.startLine);
                }
            }
        });

        actionMap.put("foldAll", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                foldAll();
            }
        });

        actionMap.put("unfoldAll", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                unfoldAll();
            }
        });
    }
    
    private FoldRegion findContainingRegion(int line) {
        for (FoldRegion region : foldRegions.values()) {
            if (line > region.startLine && line < region.endLine) {
                return region;
            }
        }
        return null;
    }

    private int getCaretLine() {
        int caretPos = textPane.getCaretPosition();
        Element root = textPane.getDocument().getDefaultRootElement();
        return root.getElementIndex(caretPos);
    }

    public FoldingGutter getGutter() {
        return gutter;
    }

    public boolean isLineFolded(int line) {
        FoldRegion region = foldRegions.get(line);
        return region != null && region.collapsed;
    }
    
    public boolean isLineVisible(int line) {
        for (FoldRegion region : foldRegions.values()) {
            if (region.collapsed && line > region.startLine && line < region.endLine) {
                return false;
            }
        }
        return true;
    }

    public FoldRegion getFoldRegion(int line) {
        return foldRegions.get(line);
    }
    
    public boolean ping() {
        return textPane != null && gutter != null;
    }

    static class FoldRegion {
        final int startLine;
        final int endLine;
        final FoldType type;
        boolean collapsed = false;

        FoldRegion(int startLine, int endLine, FoldType type) {
            this.startLine = startLine;
            this.endLine = endLine;
            this.type = type;
        }
    }

    enum FoldType {
        BRACE, METHOD, COMMENT, IMPORT
    }
}

class FoldingGutter extends JPanel {
    private final CodeFoldingManager foldingManager;
    private final JTextPane textPane;
    private final Font font = new Font("Monospaced", Font.PLAIN, 12);

    public FoldingGutter(CodeFoldingManager foldingManager) {
        this.foldingManager = foldingManager;
        this.textPane = foldingManager.textPane;
        setPreferredSize(new Dimension(20, 0));
        setBackground(new Color(0x252526)); 
        setOpaque(true);

        addMouseListener(new MouseAdapter() {
            public void mouseClicked(MouseEvent evt) {
                int line = getLineAtY(evt.getY());
                if (foldingManager.getFoldRegion(line) != null) {
                    foldingManager.toggleFold(line);
                }
            }
        });
        
        textPane.getComponentListeners();
    }

    private int getLineAtY(int y) {
        try {
            int pos = textPane.viewToModel2D(new Point(0, y));
            if (pos >= 0) {
                return textPane.getDocument().getDefaultRootElement().getElementIndex(pos);
            }
        } catch (Exception e) {
            // Can happen if clicking empty space
        }
        return -1;
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        Graphics2D g2d = (Graphics2D) g;
        g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g2d.setFont(font);

        Rectangle clip = g.getClipBounds();
        
        for (Map.Entry<Integer, CodeFoldingManager.FoldRegion> entry : foldingManager.foldRegions.entrySet()) {
            int startLine = entry.getKey();
            
            try {
                Rectangle rect = textPane.modelToView2D(textPane.getDocument().getDefaultRootElement().getElement(startLine).getStartOffset()).getBounds();
                
                if (clip.intersects(rect)) {
                    boolean isCollapsed = foldingManager.isLineFolded(startLine);
                    drawMarker(g2d, rect.y, isCollapsed);
                }
            } catch (BadLocationException e) {
                // ignore, can happen during updates
            }
        }
    }

    private void drawMarker(Graphics2D g2d, int y, boolean collapsed) {
        int midY = y + textPane.getFontMetrics(textPane.getFont()).getAscent() / 2;
        g2d.setColor(new Color(0x858585));
        g2d.drawRect(4, midY - 4, 8, 8);
        g2d.setColor(new Color(0xd4d4d4));
        g2d.drawLine(6, midY, 10, midY);
        if (collapsed) {
            g2d.drawLine(8, midY - 2, 8, midY + 2);
        }
    }
}