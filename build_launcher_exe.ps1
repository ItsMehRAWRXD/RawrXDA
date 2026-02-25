param(
  [string]$OutDir = (Join-Path $PSScriptRoot 'build_ninja\\bin'),
  [string]$OutName = 'RawrXD-Launcher.exe'
)

$ErrorActionPreference = 'Stop'

function Resolve-FullPath([string]$Path) {
  return (Resolve-Path -LiteralPath $Path).Path
}

$outDirFull = if (Test-Path -LiteralPath $OutDir) { Resolve-FullPath $OutDir } else { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null; Resolve-FullPath $OutDir }
$outExe = Join-Path $outDirFull $OutName

$bridgeExe = Resolve-FullPath (Join-Path $PSScriptRoot 'build_ninja\bin\RawrXD-Standalone-WebBridge.exe')
$browserExe = Resolve-FullPath (Join-Path $PSScriptRoot 'build_ninja\bin\RawrXD-InHouseBrowser.exe')
$webRoot = Resolve-FullPath $PSScriptRoot

$cs = @"
using System;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;

static class Program
{
    static readonly HttpClient Http = new HttpClient { Timeout = TimeSpan.FromSeconds(10) };
    const string BridgeExe = @"@BRIDGE_EXE@";
    const string BrowserExe = @"@BROWSER_EXE@";
    const string WebRoot = @"@WEB_ROOT@";

    [STAThread]
    static void Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new LauncherForm());
    }

    sealed class LauncherForm : Form
    {
        readonly TextBox _status = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill };
        readonly TextBox _httpPort = new TextBox { Text = "8080", Width = 80 };
        readonly TextBox _wsPort = new TextBox { Text = "8081", Width = 80 };
        readonly TextBox _modelName = new TextBox { Text = "model.gguf", Dock = DockStyle.Fill };
        readonly Button _start = new Button { Text = "Start Bridge" };
        readonly Button _openIde = new Button { Text = "Open IDE" };
        readonly Button _upload = new Button { Text = "Upload Model..." };
        readonly Button _stop = new Button { Text = "Stop Bridge" };

        Process _bridge;

        public LauncherForm()
        {
            Text = "RawrXD Launcher";
            Width = 900;
            Height = 560;

            var row1 = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 38, Padding = new Padding(8), WrapContents = false, AutoScroll = true };
            row1.Controls.Add(new Label { Text = "HTTP:", AutoSize = true, Padding = new Padding(0,8,0,0) });
            row1.Controls.Add(_httpPort);
            row1.Controls.Add(new Label { Text = "WS:", AutoSize = true, Padding = new Padding(12,8,0,0) });
            row1.Controls.Add(_wsPort);
            row1.Controls.Add(_start);
            row1.Controls.Add(_stop);
            row1.Controls.Add(_openIde);

            var row2 = new TableLayoutPanel { Dock = DockStyle.Top, Height = 38, Padding = new Padding(8), ColumnCount = 3 };
            row2.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            row2.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            row2.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            row2.Controls.Add(new Label { Text = "Model name:", AutoSize = true, Padding = new Padding(0,8,0,0) }, 0, 0);
            row2.Controls.Add(_modelName, 1, 0);
            row2.Controls.Add(_upload, 2, 0);

            Controls.Add(_status);
            Controls.Add(row2);
            Controls.Add(row1);

            _start.Click += async (_, __) => await StartBridgeAsync();
            _stop.Click += (_, __) => StopBridge();
            _openIde.Click += (_, __) => OpenIde();
            _upload.Click += async (_, __) => await UploadModelAsync();

            FormClosing += (_, __) => StopBridge();

            Log("Bridge: {0}", BridgeExe);
            Log("Browser: {0}", BrowserExe);
            Log("WebRoot: {0}", WebRoot);
        }

        int HttpPort { get { int p; return int.TryParse(_httpPort.Text, out p) ? p : 8080; } }
        int WsPort { get { int p; return int.TryParse(_wsPort.Text, out p) ? p : 8081; } }

        void Log(string fmt, params object[] args)
        {
            var line = string.Format(fmt, args);
            _status.AppendText("[" + DateTime.Now.ToString("HH:mm:ss") + "] " + line + Environment.NewLine);
        }

        async Task<bool> WaitHealthyAsync()
        {
            var url = "http://127.0.0.1:" + HttpPort + "/api/status";
            var deadline = DateTime.UtcNow.AddSeconds(8);
            while (DateTime.UtcNow < deadline)
            {
                try
                {
                    var res = await Http.GetAsync(url);
                    try { if (res.IsSuccessStatusCode) return true; }
                    finally { res.Dispose(); }
                }
                catch { }
                await Task.Delay(150);
            }
            return false;
        }

        async Task StartBridgeAsync()
        {
            if (_bridge != null && !_bridge.HasExited)
            {
                Log("Bridge already running (pid={0}).", _bridge.Id);
                return;
            }

            var args = HttpPort + " " + WsPort + " 0 127.0.0.1 \"" + WebRoot + "\"";
            var psi = new ProcessStartInfo
            {
                FileName = BridgeExe,
                Arguments = args,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            _bridge = Process.Start(psi);
            if (_bridge == null)
            {
                Log("Failed to start bridge.");
                return;
            }

            Log("Bridge started (pid={0}). Waiting for /api/status...", _bridge.Id);
            if (await WaitHealthyAsync()) Log("Bridge ONLINE at http://127.0.0.1:{0}/", HttpPort);
            else Log("Bridge did not become healthy. Check DLL/model paths.");
        }

        void StopBridge()
        {
            try
            {
                if (_bridge != null && !_bridge.HasExited)
                {
                    _bridge.Kill();
                    _bridge.WaitForExit(1500);
                    Log("Bridge stopped.");
                }
            }
            catch (Exception e)
            {
                Log("StopBridge error: {0}", e.Message);
            }
        }

        void OpenIde()
        {
            var url = "http://127.0.0.1:" + HttpPort + "/gui/ide_chatbot.html";
            var psi = new ProcessStartInfo
            {
                FileName = BrowserExe,
                Arguments = url,
                UseShellExecute = false
            };
            Process.Start(psi);
            Log("Opened IDE: {0}", url);
        }

        async Task UploadModelAsync()
        {
            if (!(_bridge != null && !_bridge.HasExited) && !await WaitHealthyAsync())
            {
                Log("Bridge is not running/healthy. Start it first.");
                return;
            }

            using (var dlg = new OpenFileDialog
            {
                Title = "Select GGUF model",
                Filter = "GGUF (*.gguf)|*.gguf|All files (*.*)|*.*",
                CheckFileExists = true
            })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;

                var name = (_modelName.Text ?? "").Trim();
                if (string.IsNullOrWhiteSpace(name)) name = Path.GetFileName(dlg.FileName);

                var url = "http://127.0.0.1:" + HttpPort + "/api/models/upload?name=" + Uri.EscapeDataString(name) + "&overwrite=1";
                Log("Uploading {0} as {1}...", dlg.FileName, name);

                using (var fs = File.OpenRead(dlg.FileName))
                using (var content = new StreamContent(fs))
                {
                    content.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream");
                    var res = await Http.PostAsync(url, content);
                    try
                    {
                        var body = await res.Content.ReadAsStringAsync();
                        Log("Upload status: {0} {1}", (int)res.StatusCode, res.ReasonPhrase);
                        Log(body);
                    }
                    finally { res.Dispose(); }
                }
            }
        }
    }
}
"@

$cs = $cs.Replace('@BRIDGE_EXE@', $bridgeExe)
$cs = $cs.Replace('@BROWSER_EXE@', $browserExe)
$cs = $cs.Replace('@WEB_ROOT@', $webRoot)

Add-Type -TypeDefinition $cs -Language CSharp -ReferencedAssemblies @(
  'System.dll',
  'System.Core.dll',
  'System.Net.Http.dll',
  'System.Windows.Forms.dll',
  'System.Drawing.dll'
) -OutputAssembly $outExe -OutputType WindowsApplication

Write-Host "Built: $outExe"
