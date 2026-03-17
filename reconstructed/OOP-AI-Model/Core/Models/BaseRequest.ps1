# ==============================================================================
# BaseRequest Class
# Base class for all AI model requests
# ==============================================================================

class BaseRequest {
    [string] $Id
    [string] $Type
    [datetime] $Timestamp
    [hashtable] $Context
    [hashtable] $Parameters
    [string] $SessionId
    [int] $Priority
    
    BaseRequest() {
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Parameters = @{}
        $this.Priority = 5  # Default priority (1-10 scale)
    }
    
    BaseRequest([string] $type, [hashtable] $parameters) {
        $this.Id = [System.Guid]::NewGuid().ToString()
        $this.Timestamp = Get-Date
        $this.Context = @{}
        $this.Parameters = $parameters
        $this.Type = $type
        $this.Priority = 5
    }
    
    [void] SetContext([string] $key, [object] $value) {
        $this.Context[$key] = $value
    }
    
    [object] GetContext([string] $key) {
        return $this.Context[$key]
    }
    
    [hashtable] ToHashtable() {
        return @{
            Id = $this.Id
            Type = $this.Type
            Timestamp = $this.Timestamp
            Context = $this.Context
            Parameters = $this.Parameters
            SessionId = $this.SessionId
            Priority = $this.Priority
        }
    }
    
    [string] ToJson() {
        return ($this.ToHashtable() | ConvertTo-Json -Depth 10)
    }
    
    [void] Validate() {
        if ([string]::IsNullOrEmpty($this.Type)) {
            throw [System.ArgumentException]::new("Request Type is required")
        }
        
        if ($this.Priority -lt 1 -or $this.Priority -gt 10) {
            throw [System.ArgumentException]::new("Priority must be between 1 and 10")
        }
    }
}