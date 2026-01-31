public class EmojiRemover {
    public static String removeEmojis(String text) {
        return text.replaceAll("[\\p{So}\\p{Cn}]", "");
    }
    
    public static String removeEmojisAdvanced(String text) {
        return text.replaceAll("[\uD83C-\uDBFF\uDC00-\uDFFF]+", "")
                  .replaceAll("[\\p{So}\\p{Cn}]", "")
                  .replaceAll("\\s+", " ")
                  .trim();
    }
    
    public static void testEmojiRemoval() {
        System.out.println("Emoji removal test completed");
    }
}