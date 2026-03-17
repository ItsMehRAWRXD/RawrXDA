# ==============================================================================
# Specific Request Types
# Concrete implementations of different AI request types
# ==============================================================================

# Text Processing Request
class TextProcessingRequest : BaseRequest {
    TextProcessingRequest([string] $text, [hashtable] $options) {
        $this.Type = "TextProcessing"
        $this.Parameters = @{
            Text = $text
            Options = $options
        }
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Priority = 5
    }
    
    [string] GetText() {
        return $this.Parameters.Text
    }
    
    [hashtable] GetOptions() {
        return $this.Parameters.Options
    }
}

# Code Analysis Request
class CodeAnalysisRequest : BaseRequest {
    CodeAnalysisRequest([string] $code, [string] $language, [string[]] $analysisTypes) {
        $this.Type = "CodeAnalysis"
        $this.Parameters = @{
            Code = $code
            Language = $language
            AnalysisTypes = $analysisTypes
        }
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Priority = 7  # Higher priority for code analysis
    }
    
    [string] GetCode() {
        return $this.Parameters.Code
    }
    
    [string] GetLanguage() {
        return $this.Parameters.Language
    }
    
    [string[]] GetAnalysisTypes() {
        return $this.Parameters.AnalysisTypes
    }
}

# Data Processing Request  
class DataProcessingRequest : BaseRequest {
    DataProcessingRequest([object[]] $data, [string] $operation, [hashtable] $config) {
        $this.Type = "DataProcessing"
        $this.Parameters = @{
            Data = $data
            Operation = $operation
            Configuration = $config
        }
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Priority = 6
    }
    
    [object[]] GetData() {
        return $this.Parameters.Data
    }
    
    [string] GetOperation() {
        return $this.Parameters.Operation
    }
    
    [hashtable] GetConfiguration() {
        return $this.Parameters.Configuration
    }
}

# Conversation Request
class ConversationRequest : BaseRequest {
    ConversationRequest([string] $message, [hashtable] $conversationContext) {
        $this.Type = "Conversation"
        $this.Parameters = @{
            Message = $message
            ConversationContext = $conversationContext
        }
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Priority = 5
    }
    
    [string] GetMessage() {
        return $this.Parameters.Message
    }
    
    [hashtable] GetConversationContext() {
        return $this.Parameters.ConversationContext
    }
}