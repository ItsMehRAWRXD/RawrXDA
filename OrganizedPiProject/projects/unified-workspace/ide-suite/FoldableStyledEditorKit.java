
import javax.swing.text.*;
import java.awt.*;

/**
 * A custom EditorKit that supports hiding ranges of text for code folding.
 * It uses a custom ViewFactory to create views that can be collapsed.
 */
public class FoldableStyledEditorKit extends StyledEditorKit {

    public static final String FOLDED_ATTRIBUTE = "folded";

    private ViewFactory viewFactory;

    public FoldableStyledEditorKit() {
        viewFactory = new FoldableViewFactory();
    }

    @Override
    public ViewFactory getViewFactory() {
        return viewFactory;
    }

    /**
     * The custom ViewFactory that creates views capable of folding.
     */
    private static class FoldableViewFactory implements ViewFactory {
        @Override
        public View create(Element elem) {
            String kind = elem.getName();
            if (kind != null) {
                if (kind.equals(AbstractDocument.ContentElementName)) {
                    return new LabelView(elem);
                } else if (kind.equals(AbstractDocument.ParagraphElementName)) {
                    return new FoldableParagraphView(elem);
                } else if (kind.equals(AbstractDocument.SectionElementName)) {
                    return new BoxView(elem, View.Y_AXIS);
                } else if (kind.equals(StyleConstants.ComponentElementName)) {
                    return new ComponentView(elem);
                } else if (kind.equals(StyleConstants.IconElementName)) {
                    return new IconView(elem);
                }
            }
            // Default to a label view
            return new LabelView(elem);
        }
    }

    /**
     * A custom ParagraphView that can be "folded" (i.e., have its height reduced to zero).
     */
    private static class FoldableParagraphView extends ParagraphView {
        public FoldableParagraphView(Element elem) {
            super(elem);
        }

        @Override
        public float getPreferredSpan(int axis) {
            if (isFolded()) {
                // If folded, return a height of 0
                return 0;
            }
            return super.getPreferredSpan(axis);
        }

        @Override
        public void paint(Graphics g, Shape a) {
            if (!isFolded()) {
                super.paint(g, a);
            }
            // If folded, do nothing (don't paint)
        }

        @Override
        public Shape modelToView(int pos, Shape a, Position.Bias b) throws BadLocationException {
             if (isFolded()) {
                // For folded views, return a zero-width rectangle at the start of the view
                Rectangle r = a.getBounds();
                return new Rectangle(r.x, r.y, 0, 0);
            }
            return super.modelToView(pos, a, b);
        }
        
        @Override
        public int viewToModel(float x, float y, Shape a, Position.Bias[] bias) {
            if (isFolded()) {
                // For folded views, always return the start offset of the element
                return getStartOffset();
            }
            return super.viewToModel(x, y, a, bias);
        }

        private boolean isFolded() {
            AttributeSet attrs = getElement().getAttributes();
            Object foldedAttr = attrs.getAttribute(FOLDED_ATTRIBUTE);
            return (foldedAttr instanceof Boolean) && ((Boolean) foldedAttr);
        }
    }
}
