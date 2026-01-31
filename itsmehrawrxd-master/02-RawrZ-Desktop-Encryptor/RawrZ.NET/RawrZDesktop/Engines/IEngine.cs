using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public interface IEngine
    {
        string Name { get; }
        string Description { get; }
        string Version { get; }
        Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters);
    }
}