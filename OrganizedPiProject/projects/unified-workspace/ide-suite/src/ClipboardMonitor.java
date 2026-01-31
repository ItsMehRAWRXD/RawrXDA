import java.awt.*;
import java.awt.datatransfer.*;

public class ClipboardMonitor implements Runnable {
    private final Clipboard clipboard = Toolkit.getDefaultToolkit().getSystemClipboard();
    private String lastContent = "";
    private final AutoResponder responder;
    
    public ClipboardMonitor(AutoResponder responder) {
        this.responder = responder;
    }
    
    @Override
    public void run() {
        while (true) {
            try {
                Transferable contents = clipboard.getContents(null);
                if (contents != null && contents.isDataFlavorSupported(DataFlavor.stringFlavor)) {
                    String content = (String) contents.getTransferData(DataFlavor.stringFlavor);
                    if (!content.equals(lastContent)) {
                        lastContent = content;
                        if (content.contains("@") || content.contains("AI:")) {
                            // Detected potential chat message
                            responder.processClipboard(content);
                        }
                    }
                }
                Thread.sleep(500);
            } catch (Exception e) {
                // Continue monitoring
            }
        }
    }
}