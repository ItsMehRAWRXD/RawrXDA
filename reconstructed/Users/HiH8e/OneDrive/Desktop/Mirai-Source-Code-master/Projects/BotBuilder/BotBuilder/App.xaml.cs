using System.Windows;

namespace BotBuilder
{
  public partial class App : Application
  {
    [System.STAThreadAttribute()]
    public static void Main()
    {
      BotBuilder.App app = new BotBuilder.App();
      app.InitializeComponent();
      app.Run();
    }

    protected override void OnStartup(StartupEventArgs e)
    {
      base.OnStartup(e);
    }
  }
}
