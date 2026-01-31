using System;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using Microsoft.Web.WebView2.Core;
using System.Threading.Tasks;
using System.Windows.Controls;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Media.Animation;

namespace KimiAppNative
{
    public partial class MainWindow : Window
    {
        private string _currentAssistant = "Kimi";
        private SystemTrayManager _trayManager;
        private TextBox _liveStreamPanel;
        private StringBuilder _streamBuffer = new StringBuilder();
        private readonly string[] _assistantUrls = {
            "https://kimi.moonshot.cn/",           // Kimi AI
            "https://www.cursor.com/",             // Cursor
            "https://chat.openai.com/"             // ChatGPT
        };

        public MainWindow()
        {
            InitializeComponent();
            InitializeLiveStreamPanel();
            InitializeWebView();
            SetupEventHandlers();
            InitializeSystemTray();
            ShowAssistant("Kimi");
        }

        private void InitializeLiveStreamPanel()
        {
            // Live stream panel is now defined in XAML
            // Just initialize the buffer
            _streamBuffer.AppendLine("[Live Stream] Waiting for AI responses...");
        }

        private async void InitializeWebView()
        {
            try
            {
                await WebView.EnsureCoreWebView2Async(null);
                WebView.CoreWebView2.Settings.IsStatusBarEnabled = false;
                WebView.CoreWebView2.Settings.AreDefaultContextMenusEnabled = true;
                WebView.CoreWebView2.Settings.AreDevToolsEnabled = false;
                
                // Monitor DOM changes for live streaming
                WebView.CoreWebView2.DOMContentLoaded += OnDOMContentLoaded;
                WebView.CoreWebView2.NavigationCompleted += OnNavigationCompleted;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to initialize WebView: {ex.Message}", "Error", 
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void SetupEventHandlers()
        {
            // Enable window dragging
            this.MouseDown += (sender, e) =>
            {
                if (e.ChangedButton == MouseButton.Left)
                    this.DragMove();
            };

            // Handle window state changes
            this.StateChanged += (sender, e) =>
            {
                if (this.WindowState == WindowState.Minimized)
                {
                    this.Hide();
                }
            };
        }

        private void InitializeSystemTray()
        {
            _trayManager = new SystemTrayManager(this);
        }

        public void ShowAssistant(string assistantName)
        {
            _currentAssistant = assistantName;
            
            // Update tab appearance
            ResetTabStyles();
            switch (assistantName)
            {
                case "Kimi":
                    KimiTab.Background = new SolidColorBrush(Color.FromRgb(0, 120, 212));
                    NavigateToUrl(_assistantUrls[0]);
                    break;
                case "Cursor":
                    CursorTab.Background = new SolidColorBrush(Color.FromRgb(0, 120, 212));
                    NavigateToUrl(_assistantUrls[1]);
                    break;
                case "ChatGPT":
                    ChatGPTTab.Background = new SolidColorBrush(Color.FromRgb(0, 120, 212));
                    NavigateToUrl(_assistantUrls[2]);
                    break;
            }

            // Show and activate window
            if (this.Visibility != Visibility.Visible)
            {
                this.Show();
                this.Activate();
                this.Topmost = true;
                this.Topmost = false;
            }
        }

        private void ResetTabStyles()
        {
            KimiTab.Background = new SolidColorBrush(Color.FromRgb(108, 117, 125));
            CursorTab.Background = new SolidColorBrush(Color.FromRgb(108, 117, 125));
            ChatGPTTab.Background = new SolidColorBrush(Color.FromRgb(108, 117, 125));
        }

        private void NavigateToUrl(string url)
        {
            try
            {
                if (WebView.CoreWebView2 != null)
                {
                    WebView.CoreWebView2.Navigate(url);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to navigate to {url}: {ex.Message}", "Navigation Error", 
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void KimiTab_Click(object sender, RoutedEventArgs e)
        {
            ShowAssistant("Kimi");
        }

        private void CursorTab_Click(object sender, RoutedEventArgs e)
        {
            ShowAssistant("Cursor");
        }

        private void ChatGPTTab_Click(object sender, RoutedEventArgs e)
        {
            ShowAssistant("ChatGPT");
        }

        private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        {
            this.WindowState = WindowState.Minimized;
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            this.Hide();
        }

        private async void OnDOMContentLoaded(object sender, CoreWebView2DOMContentLoadedEventArgs e)
        {
            // Inject JavaScript to monitor AI responses
            await InjectResponseMonitor();
        }

        private async void OnNavigationCompleted(object sender, CoreWebView2NavigationCompletedEventArgs e)
        {
            if (e.IsSuccess)
            {
                await InjectResponseMonitor();
            }
        }

        private async Task InjectResponseMonitor()
        {
            try
            {
                string script = @"
                    (function() {
                        let lastContent = '';
                        
                        function checkForNewContent() {
                            // Generic selectors for AI response containers
                            const selectors = [
                                '[data-testid="conversation-turn-content"]',  // ChatGPT
                                '.message-content',                            // Generic
                                '.response-text',                             // Generic
                                '.ai-response',                               // Generic
                                '.chat-message',                              // Generic
                                'div[class*="message"]',                     // Partial match
                                'div[class*="response"]',                    // Partial match
                                'div[class*="content"]'                      // Partial match
                            ];
                            
                            for (let selector of selectors) {
                                const elements = document.querySelectorAll(selector);
                                if (elements.length > 0) {
                                    const latestElement = elements[elements.length - 1];
                                    const currentContent = latestElement.innerText || latestElement.textContent;
                                    
                                    if (currentContent && currentContent !== lastContent && currentContent.length > 10) {
                                        lastContent = currentContent;
                                        window.chrome.webview.postMessage({
                                            type: 'aiResponse',
                                            content: currentContent,
                                            assistant: '" + _currentAssistant + @"'
                                        });
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Check every 500ms for new content
                        setInterval(checkForNewContent, 500);
                        
                        // Also monitor for DOM mutations
                        const observer = new MutationObserver(checkForNewContent);
                        observer.observe(document.body, {
                            childList: true,
                            subtree: true,
                            characterData: true
                        });
                    })();
                ";

                await WebView.CoreWebView2.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
                await WebView.CoreWebView2.ExecuteScriptAsync(script);
                
                // Listen for messages from injected script
                WebView.CoreWebView2.WebMessageReceived += OnWebMessageReceived;
            }
            catch (Exception ex)
            {
                UpdateLiveStream($"[Error] Failed to inject monitor: {ex.Message}");
            }
        }

        private void OnWebMessageReceived(object sender, CoreWebView2WebMessageReceivedEventArgs e)
        {
            try
            {
                var message = System.Text.Json.JsonSerializer.Deserialize<dynamic>(e.TryGetWebMessageAsString());
                if (message != null)
                {
                    var messageDict = (System.Text.Json.JsonElement)message;
                    if (messageDict.TryGetProperty("type", out var typeElement) && 
                        typeElement.GetString() == "aiResponse")
                    {
                        if (messageDict.TryGetProperty("content", out var contentElement) &&
                            messageDict.TryGetProperty("assistant", out var assistantElement))
                        {
                            string content = contentElement.GetString();
                            string assistant = assistantElement.GetString();
                            
                            // Stream the response
                            StreamResponse(assistant, content);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                UpdateLiveStream($"[Error] Message parsing failed: {ex.Message}");
            }
        }

        private void StreamResponse(string assistant, string content)
        {
            Dispatcher.Invoke(() =>
            {
                // Simulate streaming by showing content progressively
                var timestamp = DateTime.Now.ToString("HH:mm:ss");
                var header = $"\n[{timestamp}] {assistant} Response:\n";
                
                UpdateLiveStream(header);
                
                // Stream content word by word for effect
                Task.Run(async () =>
                {
                    var words = content.Split(' ');
                    var currentLine = "";
                    
                    foreach (var word in words)
                    {
                        currentLine += word + " ";
                        
                        Dispatcher.Invoke(() =>
                        {
                            UpdateLiveStream(word + " ", false);
                        });
                        
                        await Task.Delay(50); // Streaming delay
                    }
                    
                    Dispatcher.Invoke(() =>
                    {
                        UpdateLiveStream("\n---\n");
                    });
                });
            });
        }

        private void UpdateLiveStream(string text, bool newLine = true)
        {
            var liveStreamText = this.FindName("LiveStreamText") as TextBlock;
            if (liveStreamText != null)
            {
                _streamBuffer.Append(text);
                if (newLine && !text.EndsWith("\n"))
                {
                    _streamBuffer.AppendLine();
                }
                
                liveStreamText.Text = _streamBuffer.ToString();
                
                // Auto-scroll to bottom
                var scrollViewer = FindVisualParent<ScrollViewer>(liveStreamText);
                scrollViewer?.ScrollToBottom();
                
                // Keep buffer size manageable
                if (_streamBuffer.Length > 10000)
                {
                    var excess = _streamBuffer.Length - 8000;
                    _streamBuffer.Remove(0, excess);
                    liveStreamText.Text = _streamBuffer.ToString();
                }
            }
        }
        
        private T FindVisualParent<T>(DependencyObject child) where T : DependencyObject
        {
            var parentObject = VisualTreeHelper.GetParent(child);
            if (parentObject == null) return null;
            if (parentObject is T parent) return parent;
            return FindVisualParent<T>(parentObject);
        }

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);
            
            // Make window stay on top when activated
            this.Activated += (sender, args) => this.Topmost = true;
            this.Deactivated += (sender, args) => this.Topmost = false;
        }
    }
}
