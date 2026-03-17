class SimpleEditor:
    def __init__(self):
        self.content = ""
        
    def load(self, text):
        """Just loads text, that's it"""
        self.content = text
        
    def get_text(self):
        """Just returns the text, that's it"""
        return self.content

# Usage:
editor = SimpleEditor()
editor.load("some text")  # Load text
text = editor.get_text()  # Get text