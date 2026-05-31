#include "ModelLoader.hpp"
#include "streaming_gguf_loader.h"
#include "utils/ErrorReporter.hpp"
#include <iostream>

ModelLoader::ModelLoader()
    : m_usingStreaming(false), m_isLoaded(false)
{
}

ModelLoader::~ModelLoader()
{
    closeModel();
}

bool ModelLoader::loadModel(const std::string& filepath, bool useStreaming)
{
    closeModel();  // Close any previously loaded model

    m_usingStreaming = true;  // Only support streaming for now
    m_loadedModelPath = filepath;

    try
    {
        m_streamingLoader = std::make_unique<StreamingGGUFLoader>();
        
        if (!m_streamingLoader->Open(filepath))
        {
            ErrorReporter::report("Failed to open GGUF file: " + filepath, nullptr);
            return false;
        }

        if (!m_streamingLoader->ParseHeader())
        {
            ErrorReporter::report("Failed to parse GGUF header: " + filepath, nullptr);
            m_streamingLoader->Close();
            return false;
        }

        if (!m_streamingLoader->ParseMetadata())
        {
            ErrorReporter::report("Failed to parse GGUF metadata: " + filepath, nullptr);
            m_streamingLoader->Close();
            return false;
        }

        if (!m_streamingLoader->BuildTensorIndex())
        {
            ErrorReporter::report("Failed to build tensor index: " + filepath, nullptr);
            m_streamingLoader->Close();
            return false;
        }

        // Optionally pre-load embedding zone
        m_streamingLoader->LoadZone("embedding");

        m_isLoaded = true;
        return true;
    }
    catch (const std::exception& e)
    {
        ErrorReporter::report("Exception loading GGUF: " + std::string(e.what()), nullptr);
        closeModel();
        return false;
    }
    catch (...)
    {
        ErrorReporter::report("Unknown exception loading GGUF: " + filepath, nullptr);
        closeModel();
        return false;
    }
}

void ModelLoader::closeModel()
{
    if (m_streamingLoader)
    {
        m_streamingLoader->Close();
        m_streamingLoader.reset();
    }

    m_isLoaded = false;
    m_loadedModelPath.clear();
}

GGUFMetadata ModelLoader::getMetadata() const
{
    if (m_usingStreaming && m_streamingLoader)
    {
        return m_streamingLoader->GetMetadata();
    }

    return GGUFMetadata{};
}

size_t ModelLoader::getCurrentMemoryUsage() const
{
    if (m_usingStreaming && m_streamingLoader)
    {
        return m_streamingLoader->GetCurrentMemoryUsage();
    }

    return 0;
}

bool ModelLoader::isModelLoaded() const
{
    return m_isLoaded;
}

std::string ModelLoader::getLoadedModelPath() const
{
    return m_loadedModelPath;
}

std::vector<std::string> ModelLoader::getAvailableTensors() const
{
    if (m_usingStreaming && m_streamingLoader)
    {
        return m_streamingLoader->GetTensorNames();
    }

    return {};
}

std::vector<size_t> ModelLoader::getTensorShape(const std::string& tensorName) const
{
    if (m_usingStreaming && m_streamingLoader)
    {
        return m_streamingLoader->GetTensorShape(tensorName);
    }

    return {};
}

bool ModelLoader::preloadZone(const std::string& zoneName)
{
    if (m_usingStreaming && m_streamingLoader)
    {
        try
        {
            m_streamingLoader->LoadZone(zoneName);
            return true;
        }
        catch (const std::exception& e)
        {
            ErrorReporter::report("Failed to preload zone " + zoneName + ": " + e.what(), nullptr);
            return false;
        }
    }

    return false;
}

void ModelLoader::unloadZone(const std::string& zoneName)
{
    if (m_usingStreaming && m_streamingLoader)
    {
        try
        {
            m_streamingLoader->UnloadZone(zoneName);
        }
        catch (const std::exception& e)
        {
            ErrorReporter::report("Failed to unload zone " + zoneName + ": " + e.what(), nullptr);
        }
    }
}
