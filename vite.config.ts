import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'node:path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@shared': path.resolve(__dirname, 'src/shared'),
    },
  },
  server: {
    port: 5273,
    strictPort: true,
    proxy: {
      '/ws': {
        target: 'ws://localhost:3037',
        ws: true,
      },
      '/api': {
        target: 'http://localhost:3037',
        changeOrigin: true,
      },
    },
  },
});
