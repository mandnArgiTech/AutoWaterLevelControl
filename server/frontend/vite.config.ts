import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const apiPort = process.env.API_PORT || '8080';
const devPort = Number(process.env.FRONTEND_DEV_PORT || 5173);

export default defineConfig({
  plugins: [react()],
  server: {
    port: devPort,
    proxy: {
      '/api': `http://localhost:${apiPort}`,
    },
  },
  preview: {
    port: devPort,
    proxy: {
      '/api': `http://localhost:${apiPort}`,
    },
  },
});
