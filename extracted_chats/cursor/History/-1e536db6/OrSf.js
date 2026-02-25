import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    open: true
  },
  build: {
    outDir: 'dist',
    sourcemap: true,
    rollupOptions: {
      input: {
        main: 'src/BigDaddyGEngine/index.html'
      }
    }
  },
  resolve: {
    alias: {
      '@': '/src/BigDaddyGEngine'
    }
  }
})
