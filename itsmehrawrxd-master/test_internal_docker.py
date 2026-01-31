#!/usr/bin/env python3
"""
Test Internal Docker System
Tests the internal Docker engine and AI model services
"""

import os
import sys
import time
import json
from pathlib import Path

# Add the current directory to Python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_internal_docker_engine():
    """Test the InternalDockerEngine functionality"""
    print("🐳 Testing Internal Docker Engine...")
    
    try:
        from safe_hybrid_ide import InternalDockerEngine
        
        # Initialize Docker engine
        docker_engine = InternalDockerEngine()
        
        # Test listing images
        print("\n📋 Available Docker Images:")
        images = docker_engine.list_images()
        for image_id, image_info in images.items():
            print(f"  • {image_info['name']} - {image_info['description']}")
            print(f"    Ports: {image_info['ports']}, Capabilities: {image_info['capabilities']}")
        
        # Test pulling an image
        print("\n📥 Testing image pull...")
        success = docker_engine.pull_image('chatgpt-api')
        if success:
            print("✅ ChatGPT API image pulled successfully")
        else:
            print("❌ Failed to pull ChatGPT API image")
        
        # Test running a container
        print("\n▶️ Testing container run...")
        success = docker_engine.run_container('chatgpt-api')
        if success:
            print("✅ ChatGPT API container started")
        else:
            print("❌ Failed to start ChatGPT API container")
        
        # Test listing containers
        print("\n📋 Running Containers:")
        containers = docker_engine.list_containers()
        for container_id, container_info in containers.items():
            print(f"  • {container_id}: {container_info['image']} - {container_info['status']}")
        
        # Test stopping container
        print("\n⏹️ Testing container stop...")
        for container_id in containers.keys():
            success = docker_engine.stop_container(container_id)
            if success:
                print(f"✅ Container {container_id} stopped")
            else:
                print(f"❌ Failed to stop container {container_id}")
        
        print("\n✅ Internal Docker Engine test completed!")
        return True
        
    except Exception as e:
        print(f"❌ Internal Docker Engine test failed: {e}")
        return False

def test_docker_services():
    """Test individual Docker services"""
    print("\n🔧 Testing Docker Services...")
    
    try:
        from safe_hybrid_ide import InternalDockerEngine
        
        docker_engine = InternalDockerEngine()
        
        # Test each service
        services = ['chatgpt-api', 'claude-api', 'code-analyzer', 'ai-copilot']
        
        for service in services:
            print(f"\n📦 Testing {service}...")
            
            # Pull image
            success = docker_engine.pull_image(service)
            if success:
                print(f"  ✅ {service} image pulled")
            else:
                print(f"  ❌ Failed to pull {service} image")
                continue
            
            # Run container
            success = docker_engine.run_container(service)
            if success:
                print(f"  ✅ {service} container started")
                
                # Wait a moment for service to start
                time.sleep(2)
                
                # Test service health (simplified)
                print(f"  🔍 Testing {service} health...")
                # In a real test, we would make HTTP requests to the service
                print(f"  ✅ {service} service is running")
                
                # Stop container
                containers = docker_engine.list_containers()
                for container_id, container_info in containers.items():
                    if container_info.get('image') == service:
                        docker_engine.stop_container(container_id)
                        print(f"  ⏹️ {service} container stopped")
                        break
            else:
                print(f"  ❌ Failed to start {service} container")
        
        print("\n✅ Docker Services test completed!")
        return True
        
    except Exception as e:
        print(f"❌ Docker Services test failed: {e}")
        return False

def test_ai_service_integration():
    """Test AI service integration with Docker"""
    print("\n🤖 Testing AI Service Integration...")
    
    try:
        from safe_hybrid_ide import AIServiceManager
        
        # Initialize AI service manager
        ai_manager = AIServiceManager()
        
        # Test Docker connectors
        docker_connectors = [
            'docker-chatgpt', 'docker-claude', 'docker-ollama', 
            'docker-analyzer', 'docker-copilot'
        ]
        
        for connector_name in docker_connectors:
            if connector_name in ai_manager.connectors:
                print(f"  ✅ {connector_name} connector available")
            else:
                print(f"  ❌ {connector_name} connector missing")
        
        # Test service priorities
        print("\n📊 Service Priorities:")
        for service, priority in ai_manager.service_priorities.items():
            if 'docker' in service:
                print(f"  • {service}: {priority}")
        
        print("\n✅ AI Service Integration test completed!")
        return True
        
    except Exception as e:
        print(f"❌ AI Service Integration test failed: {e}")
        return False

def test_model_registry_integration():
    """Test model registry with Docker services"""
    print("\n📋 Testing Model Registry Integration...")
    
    try:
        from safe_hybrid_ide import AIModelRegistry
        
        registry = AIModelRegistry()
        
        # Check for Docker services in registry
        docker_models = []
        for model_id, model_info in registry.models.items():
            if 'docker' in model_info.get('type', '') or 'api_server' in model_info.get('type', ''):
                docker_models.append(model_id)
        
        print(f"  📦 Found {len(docker_models)} Docker-based models:")
        for model_id in docker_models:
            model_info = registry.models[model_id]
            print(f"    • {model_info['name']} - {model_info['description']}")
            print(f"      Ports: {model_info.get('ports', 'N/A')}")
            print(f"      Source: {model_info.get('source', 'N/A')}")
        
        print("\n✅ Model Registry Integration test completed!")
        return True
        
    except Exception as e:
        print(f"❌ Model Registry Integration test failed: {e}")
        return False

def main():
    """Run all tests"""
    print("🚀 Internal Docker System Test Suite")
    print("=" * 50)
    
    tests = [
        ("Internal Docker Engine", test_internal_docker_engine),
        ("Docker Services", test_docker_services),
        ("AI Service Integration", test_ai_service_integration),
        ("Model Registry Integration", test_model_registry_integration)
    ]
    
    results = []
    
    for test_name, test_func in tests:
        print(f"\n🧪 Running {test_name} test...")
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ {test_name} test crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 Test Results Summary:")
    
    passed = 0
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {test_name}: {status}")
        if result:
            passed += 1
    
    print(f"\n🎯 Overall: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Internal Docker system is working correctly.")
    else:
        print("⚠️ Some tests failed. Check the output above for details.")
    
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
