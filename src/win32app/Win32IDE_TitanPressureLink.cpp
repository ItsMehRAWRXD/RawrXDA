// C-linkage Titan pressure for Swarm/AgentKernel (Win32IDE does not load RawrXD_Titan.dll at link time).

extern "C" double Titan_GetMemoryPressure()
{
    return 0.0;
}
