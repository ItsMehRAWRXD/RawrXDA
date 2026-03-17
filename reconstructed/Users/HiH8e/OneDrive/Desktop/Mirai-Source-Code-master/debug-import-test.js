#!/usr/bin/env node

/*
  Debug import test - find what's causing hanging
*/

console.log('Testing imports...');

async function testImports() {
  try {
    console.log('1. Testing core encryption...');
    await import('./core/encryption/advanced-encryption-engine.js');
    console.log('✓ Core encryption loaded');

    console.log('2. Testing polymorphic engine...');
    await import('./engines/polymorphic/polymorphic-engine.js');
    console.log('✓ Polymorphic engine loaded');

    console.log('3. Testing file type manager...');
    await import('./engines/file-types/file-type-manager.js');
    console.log('✓ File type manager loaded');

    console.log('4. Testing advanced payload builder...');
    await import('./engines/advanced-payload-builder.js');
    console.log('✓ Advanced payload builder loaded');

    console.log('5. Testing weaponized payload system...');
    await import('./engines/weaponized/weaponized-payload-system.js');
    console.log('✓ Weaponized payload system loaded');

    console.log('6. Testing integrated FUD toolkit...');
    await import('./engines/integrated/integrated-fud-toolkit.js');
    console.log('✓ Integrated FUD toolkit loaded');

    console.log('\n🎉 All imports successful!');
  } catch (error) {
    console.error('❌ Import failed:', error.message);
    console.error(error.stack);
  }
}

testImports();