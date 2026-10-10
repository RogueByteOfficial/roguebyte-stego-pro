import express from 'express';
import { createServer as createViteServer } from 'vite';
import { execFile, exec } from 'child_process';
import util from 'util';
import path from 'path';
import fs from 'fs';

const execFileAsync = util.promisify(execFile);
const execAsync = util.promisify(exec);

async function startServer() {
  const app = express();
  const PORT = Number(process.env.PORT) || 3000;

  app.use(express.json({ limit: '50mb' }));
  app.use(express.urlencoded({ extended: true, limit: '50mb' }));

  const binPath = path.resolve(process.cwd(), 'build', 'rb-stego');

  // API endpoint to execute rb-stego commands directly
  app.post('/api/cli', async (req, res) => {
    try {
      const { args } = req.body;
      if (!Array.isArray(args)) {
        return res.status(400).json({ error: 'args must be an array of string arguments' });
      }

      // Check if binary exists
      if (!fs.existsSync(binPath)) {
        return res.status(500).json({
          error: 'Binary rb-stego not built yet. Run cmake build.',
          stdout: '',
          stderr: 'Binary not found at ' + binPath,
          exitCode: 1
        });
      }

      try {
        const { stdout, stderr } = await execFileAsync(binPath, args, {
          timeout: 10000,
          maxBuffer: 10 * 1024 * 1024
        });
        return res.json({ stdout, stderr, exitCode: 0 });
      } catch (err: any) {
        return res.json({
          stdout: err.stdout || '',
          stderr: err.stderr || err.message,
          exitCode: err.code || 1
        });
      }
    } catch (e: any) {
      return res.status(500).json({ error: e.message });
    }
  });

  // API endpoint to get real CTest execution results
  app.get('/api/test-results', async (req, res) => {
    try {
      const { stdout, stderr } = await execAsync('ctest --test-dir build --output-on-failure');
      res.json({ success: true, output: stdout, error: stderr });
    } catch (err: any) {
      res.json({ success: false, output: err.stdout || '', error: err.stderr || err.message });
    }
  });

  // API endpoint to read repository source files
  app.get('/api/file-content', (req, res) => {
    try {
      const relPath = (req.query.path as string) || '';
      // Defend against path traversal
      const safeRel = path.normalize(relPath).replace(/^(\.\.[\/\\])+/, '');
      const fullPath = path.resolve(process.cwd(), safeRel);

      if (!fullPath.startsWith(process.cwd())) {
        return res.status(403).json({ error: 'Access denied' });
      }

      if (!fs.existsSync(fullPath)) {
        return res.status(404).json({ error: 'File not found' });
      }

      const content = fs.readFileSync(fullPath, 'utf-8');
      res.json({ path: safeRel, content });
    } catch (err: any) {
      res.status(500).json({ error: err.message });
    }
  });

  // In development, mount Vite's connect instance as middleware
  if (process.env.NODE_ENV !== 'production') {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  } else {
    // Serve static files in production
    app.use(express.static(path.resolve(process.cwd(), 'dist')));
    app.get('*', (req, res) => {
      res.sendFile(path.resolve(process.cwd(), 'dist', 'index.html'));
    });
  }

  app.listen(PORT, '0.0.0.0', () => {
    console.log(`[RogueByte Stego Pro] Server running on http://0.0.0.0:${PORT}`);
  });
}

startServer().catch(err => {
  console.error('Failed to start server:', err);
});
