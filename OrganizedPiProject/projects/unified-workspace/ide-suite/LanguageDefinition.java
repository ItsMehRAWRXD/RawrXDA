import java.util.List;

public class LanguageDefinition {
    public final List<String> keywords;
    public final List<String> types;
    public final List<String> literals;
    public final String lineComment;
    public final String blockCommentStart;
    public final String blockCommentEnd;
    public final String stringDelimiter;
    public final String charDelimiter;
    
    public LanguageDefinition(List<String> keywords, List<String> types, List<String> literals,
                            String lineComment, String blockCommentStart, String blockCommentEnd,
                            String stringDelimiter, String charDelimiter) {
        this.keywords = keywords;
        this.types = types;
        this.literals = literals;
        this.lineComment = lineComment;
        this.blockCommentStart = blockCommentStart;
        this.blockCommentEnd = blockCommentEnd;
        this.stringDelimiter = stringDelimiter;
        this.charDelimiter = charDelimiter;
    }
    
    public boolean ping() {
        return keywords != null && !keywords.isEmpty();
    }
}