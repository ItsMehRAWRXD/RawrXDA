namespace AIDesktopHelper.Api.Services
{
    public interface IFileService
    {
        Task<FileUploadResult> UploadEncryptedFileAsync(byte[] fileData, string fileName, string contentType, EncryptionAlgorithm algorithm);
        Task<byte[]> DownloadEncryptedFileAsync(string fileId);
        Task<FileMetadata> GetFileMetadataAsync(string fileId);
        Task<bool> DeleteFileAsync(string fileId);
        Task<List<FileMetadata>> ListUserFilesAsync(string userId);
        Task<string> GenerateSignedDownloadUrlAsync(string fileId, TimeSpan expiration);
        Task<bool> ShareFileAsync(string fileId, string targetUserId, FilePermission permission);
    }

    public class FileUploadResult
    {
        public string FileId { get; set; } = string.Empty;
        public string FileName { get; set; } = string.Empty;
        public long FileSize { get; set; }
        public string ContentType { get; set; } = string.Empty;
        public EncryptionAlgorithm Algorithm { get; set; }
        public DateTime UploadedAt { get; set; }
        public string OwnerId { get; set; } = string.Empty;
    }

    public class FileMetadata
    {
        public string FileId { get; set; } = string.Empty;
        public string FileName { get; set; } = string.Empty;
        public long FileSize { get; set; }
        public string ContentType { get; set; } = string.Empty;
        public EncryptionAlgorithm Algorithm { get; set; }
        public DateTime UploadedAt { get; set; }
        public DateTime LastAccessed { get; set; }
        public string OwnerId { get; set; } = string.Empty;
        public List<FileShare> Shares { get; set; } = new();
    }

    public class FileShare
    {
        public string UserId { get; set; } = string.Empty;
        public FilePermission Permission { get; set; }
        public DateTime SharedAt { get; set; }
        public DateTime? ExpiresAt { get; set; }
    }

    public enum FilePermission
    {
        Read,
        Write,
        Admin
    }
}
