using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using AIDesktopHelper.Api.Core.Interfaces;
using AIDesktopHelper.Api.Models;
using System.Threading.Tasks;
using System.Linq;

namespace AIDesktopHelper.Api.Controllers
{
    /// <summary>
    /// Thin controller for AI operations
    /// Controller's job is to route requests, not contain business logic
    /// </summary>
    [Authorize]
    [ApiController]
    [Route("api/[controller]")]
    public sealed class AiController : ControllerBase
    {
        private readonly IChatService _chatService;

        public AiController(IChatService chatService) => _chatService = chatService;

        /// <summary>
        /// Handles chat requests with AI
        /// </summary>
        /// <param name="request">Chat request data</param>
        /// <returns>AI response</returns>
        [HttpPost("chat")]
        public async Task<IActionResult> ChatAsync([FromBody] ChatRequestDto request)
        {
            var userId = User.Identity?.Name;
            if (userId is null) return Unauthorized();

            var response = await _chatService.GetCompletionAsync(request.Prompt, userId);
            return Ok(new { message = response });
        }

        /// <summary>
        /// Handles streaming chat requests
        /// </summary>
        /// <param name="request">Chat request data</param>
        /// <returns>Streaming AI response</returns>
        [HttpPost("chat/stream")]
        public async Task<IActionResult> ChatStreamAsync([FromBody] ChatRequestDto request)
        {
            var userId = User.Identity?.Name;
            if (userId is null) return Unauthorized();

            var responseStream = _chatService.GetStreamingCompletionAsync(request.Prompt, userId);
            
            return new StreamingActionResult(async (stream, cancellationToken) =>
            {
                await foreach (var chunk in responseStream.WithCancellation(cancellationToken))
                {
                    var data = $"data: {System.Text.Json.JsonSerializer.Serialize(new { chunk })}\n\n";
                    await stream.WriteAsync(System.Text.Encoding.UTF8.GetBytes(data), cancellationToken);
                }
            });
        }

        /// <summary>
        /// Gets conversation history for the user
        /// </summary>
        /// <param name="limit">Maximum number of messages to return</param>
        /// <returns>List of chat messages</returns>
        [HttpGet("history")]
        public async Task<IActionResult> GetHistoryAsync([FromQuery] int limit = 50)
        {
            var userId = User.Identity?.Name;
            if (userId is null) return Unauthorized();

            var history = await _chatService.GetConversationHistoryAsync(userId, limit);
            return Ok(history);
        }

        /// <summary>
        /// Clears conversation history for the user
        /// </summary>
        /// <returns>Success status</returns>
        [HttpDelete("history")]
        public async Task<IActionResult> ClearHistoryAsync()
        {
            var userId = User.Identity?.Name;
            if (userId is null) return Unauthorized();

            var success = await _chatService.ClearHistoryAsync(userId);
            return Ok(new { success });
        }

        /// <summary>
        /// Handles large context chat requests
        /// </summary>
        /// <param name="request">Large context chat request</param>
        /// <returns>AI response with context</returns>
        [HttpPost("chat/large-context")]
        public async Task<IActionResult> LargeContextChatAsync([FromBody] LargeContextChatRequestDto request)
        {
            var userId = User.Identity?.Name;
            if (userId is null) return Unauthorized();

            var response = await _chatService.GetLargeContextCompletionAsync(request.Prompt, userId, request.Filename);
            return Ok(new { message = response });
        }
    }

    /// <summary>
    /// Data transfer object for chat requests
    /// </summary>
    public class ChatRequestDto
    {
        public string Prompt { get; set; }
        public string Context { get; set; }
        public string ServiceType { get; set; } = "Standard";
    }

    /// <summary>
    /// Data transfer object for large context chat requests
    /// </summary>
    public class LargeContextChatRequestDto
    {
        public string Prompt { get; set; }
        public string Filename { get; set; }
        public string Context { get; set; }
    }

    /// <summary>
    /// Custom action result for streaming responses
    /// </summary>
    public class StreamingActionResult : IActionResult
    {
        private readonly Func<Stream, CancellationToken, Task> _streamWriter;

        public StreamingActionResult(Func<Stream, CancellationToken, Task> streamWriter)
        {
            _streamWriter = streamWriter;
        }

        public async Task ExecuteResultAsync(ActionContext context)
        {
            var response = context.HttpContext.Response;
            response.ContentType = "text/event-stream";
            response.Headers.Add("Cache-Control", "no-cache");
            response.Headers.Add("Connection", "keep-alive");

            await _streamWriter(response.Body, context.HttpContext.RequestAborted);
        }
    }
}
