using RawrZSecurityPlatform.Models;

namespace RawrZSecurityPlatform.Services
{
    public interface IStubGeneratorService
    {
        Task<StubGenerationResponse> GenerateStubAsync(StubGenerationRequest request);
        Task<string> GenerateCSharpCodeAsync(StubGenerationRequest request);
        Task<byte[]> CompileCSharpToExecutableAsync(string csharpCode, StubGenerationRequest request);
    }
}
