import express from 'express';
import { spawn, execFile } from 'child_process';
import { readFileSync, writeFileSync, existsSync, mkdirSync, renameSync, readdirSync, rmSync } from 'fs';
import { join, resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import { randomUUID } from 'crypto';
import treeKill from 'tree-kill';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const ROOT = resolve(__dirname, '..');
const CONFIG_PATH = process.env.META_CONFIG_PATH || join(__dirname, 'meta-config.json');
const NODES_DIR = join(ROOT, 'src', 'nodes', 'nodes');
const BUILD_DIR = join(ROOT, 'build');
const CLIENT_DIR = join(ROOT, 'src', 'client');

const META_PORT = 9090;
const AUTO_DEPLOY = process.env.ANODE_AUTO_DEPLOY === 'true' || process.argv.includes('--auto-deploy');

// ── Config Management ──────────────────────────────────────────

const DEFAULT_CONFIG = {
  anodeServer: {
    port: 8080,
    address: '0.0.0.0',
    graphsDbPath: process.env.GRAPHS_DB_PATH || '../examples/graphs.db',
    logLevel: 'info',
    enableProfiler: true,
    postgresConf: '',
    appParams: '',
    autoStartClient: true,
  },
  sources: [],
};

function loadConfig() {
  if (!existsSync(CONFIG_PATH)) {
    writeFileSync(CONFIG_PATH, JSON.stringify(DEFAULT_CONFIG, null, 2));
  }
  return JSON.parse(readFileSync(CONFIG_PATH, 'utf-8'));
}

function saveConfig(config) {
  writeFileSync(CONFIG_PATH, JSON.stringify(config, null, 2));
}

let config = loadConfig();
if (!config.sources) config.sources = [];
mergeEnvSources();

// ── Source Helpers ─────────────────────────────────────────────

function buildAuthedUrl(source) {
  const url = new URL(source.url);
  if (source.token) {
    if (source.type === 'gitlab') {
      url.username = 'oauth2';
      url.password = source.token;
    } else {
      url.username = source.token;
      url.password = '';
    }
  }
  return url.toString();
}

function pluginNameFromUrl(url) {
  try {
    const pathname = new URL(url).pathname;
    const last = pathname.split('/').filter(Boolean).pop() || '';
    return last.replace(/\.git$/, '').toLowerCase().replace(/[^a-z0-9_]/g, '_');
  } catch {
    return '';
  }
}

function isValidPluginName(name) {
  return /^[a-z][a-z0-9_]*$/.test(name) && !name.includes('..');
}

// ── Token Resolution ──────────────────────────────────────────

function resolveSourceToken(source) {
  const envKey = `ANODE_SOURCE_${source.name.toUpperCase()}_TOKEN`;
  const envToken = process.env[envKey];
  if (envToken) return envToken;
  return source.token || '';
}

function resolveSourceForGit(source) {
  return { ...source, token: resolveSourceToken(source) };
}

// ── Env Source Merging ────────────────────────────────────────

function mergeEnvSources() {
  const raw = process.env.ANODE_SOURCES;
  if (!raw) return;

  let envSources;
  try {
    envSources = JSON.parse(raw);
  } catch (err) {
    console.error(`[auto-deploy] Failed to parse ANODE_SOURCES: ${err.message}`);
    return;
  }

  if (!Array.isArray(envSources)) {
    console.error('[auto-deploy] ANODE_SOURCES must be a JSON array');
    return;
  }

  let changed = false;
  for (const envSrc of envSources) {
    if (!envSrc.url) {
      console.warn('[auto-deploy] Skipping source without url:', envSrc);
      continue;
    }

    const name = envSrc.name || pluginNameFromUrl(envSrc.url);
    if (!isValidPluginName(name)) {
      console.warn(`[auto-deploy] Skipping source with invalid name: "${name}"`);
      continue;
    }

    const existing = config.sources.find(s => s.name === name);
    if (existing) {
      if (envSrc.url !== undefined) existing.url = envSrc.url;
      if (envSrc.type !== undefined) existing.type = envSrc.type;
      if (envSrc.branch !== undefined) existing.branch = envSrc.branch;
      if (envSrc.token !== undefined) existing.token = envSrc.token;
      console.log(`[auto-deploy] Updated existing source: ${name}`);
    } else {
      config.sources.push({
        id: randomUUID(),
        name,
        type: envSrc.type || 'github',
        url: envSrc.url,
        token: envSrc.token || '',
        branch: envSrc.branch || 'main',
      });
      console.log(`[auto-deploy] Added new source: ${name}`);
    }
    changed = true;
  }

  if (changed) {
    saveConfig(config);
    console.log('[auto-deploy] Config saved with merged sources');
  }
}

// ── Plugin Discovery ───────────────────────────────────────────

function discoverPlugins() {
  const plugins = [];
  let entries;
  try {
    entries = readdirSync(NODES_DIR, { withFileTypes: true });
  } catch {
    return plugins;
  }
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const name = entry.name;
    const rootCmake = join(NODES_DIR, name, 'CMakeLists.txt');
    const disabledDir = join(NODES_DIR, name, 'disabled');
    const disabledCmake = join(disabledDir, 'CMakeLists.txt');

    const hasRoot = existsSync(rootCmake);
    const hasDisabled = existsSync(disabledCmake);

    if (!hasRoot && !hasDisabled) continue;

    const hasGit = existsSync(join(NODES_DIR, name, '.git'));
    const source = hasGit
      ? (config.sources || []).find(s => s.name === name)
      : null;

    plugins.push({
      name,
      enabled: hasRoot,
      locked: name === 'common',
      source: source ? source.id : null,
    });
  }
  return plugins;
}

// ── SSE Manager ────────────────────────────────────────────────

class SSEManager {
  constructor(maxLines = 10000) {
    this.clients = new Set();
    this.buffer = [];
    this.maxLines = maxLines;
  }

  addClient(res) {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });
    // Send buffered lines
    for (const line of this.buffer) {
      res.write(`data: ${JSON.stringify(line)}\n\n`);
    }
    this.clients.add(res);
    res.on('close', () => this.clients.delete(res));
  }

  send(data) {
    this.buffer.push(data);
    if (this.buffer.length > this.maxLines) {
      this.buffer.shift();
    }
    const msg = `data: ${JSON.stringify(data)}\n\n`;
    for (const client of this.clients) {
      client.write(msg);
    }
  }

  clear() {
    this.buffer = [];
  }
}

// ── Git Manager ───────────────────────────────────────────────

class GitManager {
  constructor() {
    this.busy = false;
    this.sse = new SSEManager();
  }

  _run(args, cwd) {
    return new Promise((resolve, reject) => {
      this.sse.send({ type: 'system', text: `>>> git ${args.join(' ')}` });
      const proc = spawn('git', args, { cwd: cwd || ROOT });

      proc.stdout.on('data', (data) => {
        for (const line of data.toString().split('\n')) {
          if (line) this.sse.send({ type: 'stdout', text: line });
        }
      });
      proc.stderr.on('data', (data) => {
        for (const line of data.toString().split('\n')) {
          if (line) this.sse.send({ type: 'stderr', text: line });
        }
      });
      proc.on('close', (code) => {
        if (code === 0) resolve();
        else reject(new Error(`git exited with code ${code}`));
      });
    });
  }

  async clonePlugin(source) {
    if (this.busy) throw Object.assign(new Error('Git operation in progress'), { status: 409 });
    this.busy = true;
    const pluginDir = join(NODES_DIR, source.name);
    try {
      this.sse.send({ type: 'system', text: `>>> Cloning ${source.url} into ${source.name}/` });
      const args = ['clone', '--depth', '1'];
      if (source.branch) args.push('--branch', source.branch);
      args.push(buildAuthedUrl(source), pluginDir);
      await this._run(args);
      // Strip token from stored remote
      await this._run(['remote', 'set-url', 'origin', source.url], pluginDir);
      // Validate CMakeLists.txt
      if (!existsSync(join(pluginDir, 'CMakeLists.txt'))) {
        this.sse.send({ type: 'stderr', text: '>>> Warning: no CMakeLists.txt found in cloned repo' });
      }
      this.sse.send({ type: 'system', text: `>>> Clone complete` });
    } catch (err) {
      this.sse.send({ type: 'stderr', text: `>>> Clone failed: ${err.message}` });
      // Clean up partial clone
      try { rmSync(pluginDir, { recursive: true, force: true }); } catch {}
      throw err;
    } finally {
      this.busy = false;
    }
  }

  async updatePlugin(source) {
    if (this.busy) throw Object.assign(new Error('Git operation in progress'), { status: 409 });
    this.busy = true;
    const pluginDir = join(NODES_DIR, source.name);
    try {
      this.sse.send({ type: 'system', text: `>>> Updating ${source.name}` });
      const branch = source.branch || 'main';
      await this._run(['pull', buildAuthedUrl(source), branch], pluginDir);
      this.sse.send({ type: 'system', text: `>>> Update complete` });
    } catch (err) {
      this.sse.send({ type: 'stderr', text: `>>> Update failed: ${err.message}` });
      throw err;
    } finally {
      this.busy = false;
    }
  }

  removePlugin(name) {
    const pluginDir = join(NODES_DIR, name);
    if (!existsSync(pluginDir)) throw new Error('Plugin directory not found');
    this.sse.send({ type: 'system', text: `>>> Removing ${name}/` });
    rmSync(pluginDir, { recursive: true, force: true });
    this.sse.send({ type: 'system', text: `>>> Removed` });
  }

  testSource(source) {
    return new Promise((resolve, reject) => {
      const url = buildAuthedUrl(source);
      execFile('git', ['ls-remote', '--heads', url], { timeout: 15000 }, (err, stdout, stderr) => {
        if (err) return reject(new Error(stderr || err.message));
        const branches = stdout.split('\n')
          .filter(Boolean)
          .map(line => line.split('\t').pop().replace('refs/heads/', ''));
        resolve(branches);
      });
    });
  }
}

// ── Build Manager ──────────────────────────────────────────────

class BuildManager {
  constructor() {
    this.process = null;
    this.running = false;
    this.sse = new SSEManager();
  }

  start() {
    if (this.running) return false;

    if (!existsSync(BUILD_DIR)) {
      mkdirSync(BUILD_DIR, { recursive: true });
    }

    this.running = true;
    this.sse.clear();
    this.sse.send({ type: 'system', text: '>>> Build started' });

    const cmd = `cmake .. && make -j$(nproc)`;
    this.process = spawn('bash', ['-c', cmd], {
      cwd: BUILD_DIR,
      env: { ...process.env },
    });

    this.process.stdout.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stdout', text: line });
      }
    });

    this.process.stderr.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stderr', text: line });
      }
    });

    this.process.on('close', (code) => {
      this.running = false;
      this.process = null;
      this.sse.send({
        type: 'system',
        text: `>>> Build finished (exit code: ${code})`,
      });
    });

    return true;
  }

  waitForCompletion() {
    return new Promise((resolve) => {
      if (!this.running || !this.process) {
        return resolve(null);
      }
      this.process.on('close', (code) => {
        resolve(code);
      });
    });
  }
}

// ── Server Manager ─────────────────────────────────────────────

class ServerManager {
  constructor() {
    this.process = null;
    this.running = false;
    this.pid = null;
    this.sse = new SSEManager();
  }

  start(cfg) {
    if (this.running) return false;

    const serverBin = join(BUILD_DIR, 'anodeServer');
    if (!existsSync(serverBin)) {
      this.sse.send({ type: 'system', text: '>>> Error: build/anodeServer not found. Build first.' });
      return false;
    }

    this.sse.clear();
    this.running = true;

    const args = [
      '-p', String(cfg.port),
      '-a', cfg.address,
      '-g', cfg.graphsDbPath,
      '-l', cfg.logLevel,
    ];

    if (!cfg.enableProfiler) {
      args.push('--no-profiler');
    }

    if (cfg.postgresConf && cfg.postgresConf.trim()) {
      const confPath = join(BUILD_DIR, 'meta-postgres.conf');
      writeFileSync(confPath, cfg.postgresConf);
      args.push('--postgres', `@${confPath}`);
    }

    if (cfg.appParams && cfg.appParams.trim()) {
      const appParamsPath = join(BUILD_DIR, 'meta-app-params.conf');
      writeFileSync(appParamsPath, cfg.appParams);
      args.push('--config', `@${appParamsPath}`);
    }

    this.sse.send({ type: 'system', text: `>>> Starting anodeServer ${args.join(' ')}` });

    this.process = spawn(serverBin, args, {
      cwd: ROOT,
      env: { ...process.env },
    });
    this.pid = this.process.pid;

    this.process.stdout.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stdout', text: line });
      }
    });

    this.process.stderr.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stderr', text: line });
      }
    });

    this.process.on('close', (code) => {
      this.running = false;
      const pid = this.pid;
      this.pid = null;
      this.process = null;
      this.sse.send({
        type: 'system',
        text: `>>> Server stopped (PID ${pid}, exit code: ${code})`,
      });
    });

    return true;
  }

  stop() {
    if (!this.running || !this.process) return false;

    this.sse.send({ type: 'system', text: '>>> Stopping server...' });

    const pid = this.process.pid;
    treeKill(pid, 'SIGTERM');

    // Fallback to SIGKILL after 5s
    const timeout = setTimeout(() => {
      if (this.running && this.process) {
        treeKill(pid, 'SIGKILL');
      }
    }, 5000);

    this.process.on('close', () => clearTimeout(timeout));
    return true;
  }
}

// ── Client Manager ────────────────────────────────────────────

class ClientManager {
  constructor() {
    this.process = null;
    this.running = false;
    this.pid = null;
    this.sse = new SSEManager();
  }

  start() {
    if (this.running) return false;

    if (!existsSync(join(CLIENT_DIR, 'node_modules'))) {
      this.sse.send({ type: 'system', text: '>>> Error: src/client/node_modules not found. Run npm install first.' });
      return false;
    }

    this.sse.clear();
    this.running = true;

    this.sse.send({ type: 'system', text: '>>> Starting Vite dev server (npm run dev)' });

    this.process = spawn('npm', ['run', 'dev', '--', '--no-open'], {
      cwd: CLIENT_DIR,
      env: { ...process.env },
    });
    this.pid = this.process.pid;

    this.process.stdout.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stdout', text: line });
      }
    });

    this.process.stderr.on('data', (data) => {
      const lines = data.toString().split('\n');
      for (const line of lines) {
        if (line) this.sse.send({ type: 'stderr', text: line });
      }
    });

    this.process.on('close', (code) => {
      this.running = false;
      const pid = this.pid;
      this.pid = null;
      this.process = null;
      this.sse.send({
        type: 'system',
        text: `>>> Client dev server stopped (PID ${pid}, exit code: ${code})`,
      });
    });

    return true;
  }

  stop() {
    if (!this.running || !this.process) return false;

    this.sse.send({ type: 'system', text: '>>> Stopping client dev server...' });

    const pid = this.process.pid;
    treeKill(pid, 'SIGTERM');

    const timeout = setTimeout(() => {
      if (this.running && this.process) {
        treeKill(pid, 'SIGKILL');
      }
    }, 5000);

    this.process.on('close', () => clearTimeout(timeout));
    return true;
  }
}

// ── Express App ────────────────────────────────────────────────

const app = express();
app.use(express.json());
app.use(express.static(join(__dirname, 'public')));

const buildManager = new BuildManager();
const serverManager = new ServerManager();
const clientManager = new ClientManager();
const gitManager = new GitManager();

// ── Deploy Pipeline ──────────────────────────────────────────

const deploySSE = new SSEManager();
let deployStatus = 'idle';
let deployError = null;

async function runDeployPipeline() {
  if (deployStatus === 'running') {
    deploySSE.send({ type: 'stderr', text: '>>> Deploy already in progress' });
    return { success: false, error: 'Deploy already in progress' };
  }

  deployStatus = 'running';
  deployError = null;
  deploySSE.clear();
  deploySSE.send({ type: 'system', text: '>>> Auto-deploy pipeline started' });

  try {
    // Step 1: Clone sources that are not yet installed
    const sourcesToClone = config.sources.filter(s => {
      return !existsSync(join(NODES_DIR, s.name, '.git'));
    });

    if (sourcesToClone.length > 0) {
      deploySSE.send({ type: 'system', text: `>>> ${sourcesToClone.length} source(s) to clone` });

      for (const source of sourcesToClone) {
        deploySSE.send({ type: 'system', text: `>>> Cloning ${source.name}...` });
        await gitManager.clonePlugin(resolveSourceForGit(source));
        deploySSE.send({ type: 'system', text: `>>> ${source.name} cloned successfully` });
      }
    } else {
      deploySSE.send({ type: 'system', text: '>>> All sources already installed, skipping clone' });
    }

    // Step 2: Build
    deploySSE.send({ type: 'system', text: '>>> Starting build...' });
    const buildStarted = buildManager.start();
    if (!buildStarted) {
      throw new Error('Build failed to start (already running?)');
    }

    const exitCode = await buildManager.waitForCompletion();
    if (exitCode !== 0) {
      throw new Error(`Build failed with exit code ${exitCode}`);
    }
    deploySSE.send({ type: 'system', text: '>>> Build completed successfully' });

    // Step 3: Start server
    deploySSE.send({ type: 'system', text: '>>> Starting server...' });
    const serverStarted = serverManager.start(config.anodeServer);
    if (!serverStarted) {
      throw new Error('Server failed to start (binary not found?)');
    }

    // Auto-start client if configured
    if (config.anodeServer.autoStartClient !== false && !clientManager.running) {
      deploySSE.send({ type: 'system', text: '>>> Starting client...' });
      clientManager.start();
    }

    deploySSE.send({ type: 'system', text: '>>> Auto-deploy pipeline completed successfully' });
    deployStatus = 'done';
    return { success: true };
  } catch (err) {
    deploySSE.send({ type: 'stderr', text: `>>> Deploy pipeline failed: ${err.message}` });
    deployStatus = 'failed';
    deployError = err.message;
    return { success: false, error: err.message };
  }
}

// Status
app.get('/api/status', (_req, res) => {
  const plugins = discoverPlugins();
  const sources = (config.sources || []).map(s => ({
    ...s,
    token: s.token ? '***' : '',
    installed: existsSync(join(NODES_DIR, s.name, '.git')),
  }));
  res.json({
    server: {
      running: serverManager.running,
      pid: serverManager.pid,
    },
    client: {
      running: clientManager.running,
      pid: clientManager.pid,
    },
    build: {
      running: buildManager.running,
    },
    git: {
      busy: gitManager.busy,
    },
    deploy: {
      status: deployStatus,
      error: deployError,
    },
    plugins,
    sources,
    config: config.anodeServer,
  });
});

// Plugins
app.get('/api/plugins', (_req, res) => {
  res.json(discoverPlugins());
});

app.post('/api/plugins/:name/enable', (req, res) => {
  const { name } = req.params;
  const pluginDir = join(NODES_DIR, name);
  const rootCmake = join(pluginDir, 'CMakeLists.txt');
  const disabledCmake = join(pluginDir, 'disabled', 'CMakeLists.txt');

  if (name === 'common') {
    return res.status(400).json({ error: 'Cannot modify core plugin' });
  }
  if (existsSync(rootCmake)) {
    return res.status(200).json({ message: 'Already enabled' });
  }
  if (!existsSync(disabledCmake)) {
    return res.status(404).json({ error: 'Plugin not found' });
  }

  renameSync(disabledCmake, rootCmake);
  res.json({ message: `Plugin ${name} enabled` });
});

app.post('/api/plugins/:name/disable', (req, res) => {
  const { name } = req.params;
  const pluginDir = join(NODES_DIR, name);
  const rootCmake = join(pluginDir, 'CMakeLists.txt');
  const disabledDir = join(pluginDir, 'disabled');
  const disabledCmake = join(disabledDir, 'CMakeLists.txt');

  if (name === 'common') {
    return res.status(400).json({ error: 'Cannot modify core plugin' });
  }
  if (!existsSync(rootCmake)) {
    return res.status(200).json({ message: 'Already disabled' });
  }

  if (!existsSync(disabledDir)) {
    mkdirSync(disabledDir, { recursive: true });
  }
  renameSync(rootCmake, disabledCmake);
  res.json({ message: `Plugin ${name} disabled` });
});

// Config
app.post('/api/config', (req, res) => {
  config.anodeServer = { ...config.anodeServer, ...req.body };
  saveConfig(config);
  res.json({ message: 'Config saved', config: config.anodeServer });
});

// Build
app.post('/api/build', (_req, res) => {
  if (buildManager.running) {
    return res.status(409).json({ error: 'Build already in progress' });
  }
  buildManager.start();
  res.json({ message: 'Build started' });
});

app.get('/api/build/stream', (_req, res) => {
  buildManager.sse.addClient(res);
});

// Server
app.post('/api/server/start', (_req, res) => {
  if (serverManager.running) {
    return res.status(409).json({ error: 'Server already running' });
  }
  const ok = serverManager.start(config.anodeServer);
  if (!ok) {
    return res.status(400).json({ error: 'Failed to start server (binary not found?)' });
  }
  // Auto-start client
  if (config.anodeServer.autoStartClient !== false && !clientManager.running) {
    clientManager.start();
  }
  res.json({ message: 'Server started' });
});

app.post('/api/server/stop', (_req, res) => {
  if (!serverManager.running) {
    return res.status(400).json({ error: 'Server not running' });
  }
  serverManager.stop();
  // Also stop client
  if (clientManager.running) {
    clientManager.stop();
  }
  res.json({ message: 'Server stopping' });
});

app.get('/api/server/stream', (_req, res) => {
  serverManager.sse.addClient(res);
});

// Client
app.post('/api/client/start', (_req, res) => {
  if (clientManager.running) {
    return res.status(409).json({ error: 'Client already running' });
  }
  const ok = clientManager.start();
  if (!ok) {
    return res.status(400).json({ error: 'Failed to start client (node_modules missing?)' });
  }
  res.json({ message: 'Client started' });
});

app.post('/api/client/stop', (_req, res) => {
  if (!clientManager.running) {
    return res.status(400).json({ error: 'Client not running' });
  }
  clientManager.stop();
  res.json({ message: 'Client stopping' });
});

app.get('/api/client/stream', (_req, res) => {
  clientManager.sse.addClient(res);
});

// ── Sources ───────────────────────────────────────────────────

app.get('/api/sources', (_req, res) => {
  const sources = (config.sources || []).map(s => ({
    ...s,
    token: s.token ? '***' : '',
    installed: existsSync(join(NODES_DIR, s.name, '.git')),
  }));
  res.json(sources);
});

app.post('/api/sources', (req, res) => {
  const { type, url, token, branch } = req.body;
  if (!url) return res.status(400).json({ error: 'URL is required' });

  let name = req.body.name || pluginNameFromUrl(url);
  if (!isValidPluginName(name)) {
    return res.status(400).json({ error: `Invalid plugin name: "${name}". Must match /^[a-z][a-z0-9_]*$/` });
  }
  if (config.sources.some(s => s.name === name)) {
    return res.status(409).json({ error: `Source with name "${name}" already exists` });
  }

  const source = {
    id: randomUUID(),
    name,
    type: type || 'github',
    url,
    token: token || '',
    branch: branch || 'main',
  };
  config.sources.push(source);
  saveConfig(config);
  res.json({ ...source, token: source.token ? '***' : '' });
});

app.put('/api/sources/:id', (req, res) => {
  const source = config.sources.find(s => s.id === req.params.id);
  if (!source) return res.status(404).json({ error: 'Source not found' });

  const { name, token, branch, url, type } = req.body;
  if (name !== undefined) {
    if (!isValidPluginName(name)) {
      return res.status(400).json({ error: `Invalid plugin name: "${name}"` });
    }
    if (name !== source.name && config.sources.some(s => s.name === name)) {
      return res.status(409).json({ error: `Source with name "${name}" already exists` });
    }
    source.name = name;
  }
  if (token !== undefined) source.token = token;
  if (branch !== undefined) source.branch = branch;
  if (url !== undefined) source.url = url;
  if (type !== undefined) source.type = type;
  saveConfig(config);
  res.json({ ...source, token: source.token ? '***' : '' });
});

app.delete('/api/sources/:id', (req, res) => {
  const idx = config.sources.findIndex(s => s.id === req.params.id);
  if (idx === -1) return res.status(404).json({ error: 'Source not found' });
  config.sources.splice(idx, 1);
  saveConfig(config);
  res.json({ message: 'Source removed' });
});

app.post('/api/sources/:id/test', async (req, res) => {
  const source = config.sources.find(s => s.id === req.params.id);
  if (!source) return res.status(404).json({ error: 'Source not found' });
  try {
    const branches = await gitManager.testSource(resolveSourceForGit(source));
    res.json({ branches });
  } catch (err) {
    res.status(400).json({ error: err.message });
  }
});

app.post('/api/sources/:id/install', async (req, res) => {
  const source = config.sources.find(s => s.id === req.params.id);
  if (!source) return res.status(404).json({ error: 'Source not found' });
  if (existsSync(join(NODES_DIR, source.name, '.git'))) {
    return res.status(409).json({ error: 'Already installed' });
  }
  try {
    res.json({ message: 'Clone started' });
    await gitManager.clonePlugin(resolveSourceForGit(source));
  } catch (err) {
    if (!res.headersSent) {
      res.status(err.status || 500).json({ error: err.message });
    }
  }
});

app.post('/api/sources/:id/update', async (req, res) => {
  const source = config.sources.find(s => s.id === req.params.id);
  if (!source) return res.status(404).json({ error: 'Source not found' });
  if (!existsSync(join(NODES_DIR, source.name, '.git'))) {
    return res.status(400).json({ error: 'Not installed' });
  }
  try {
    res.json({ message: 'Update started' });
    await gitManager.updatePlugin(resolveSourceForGit(source));
  } catch (err) {
    if (!res.headersSent) {
      res.status(err.status || 500).json({ error: err.message });
    }
  }
});

app.post('/api/sources/:id/remove', (req, res) => {
  const source = config.sources.find(s => s.id === req.params.id);
  if (!source) return res.status(404).json({ error: 'Source not found' });
  try {
    gitManager.removePlugin(source.name);
    res.json({ message: 'Plugin removed' });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.get('/api/sources/stream', (_req, res) => {
  gitManager.sse.addClient(res);
});

// ── Deploy ──────────────────────────────────────────────────────

app.post('/api/deploy', (_req, res) => {
  if (deployStatus === 'running') {
    return res.status(409).json({ error: 'Deploy already in progress' });
  }
  runDeployPipeline();
  res.json({ message: 'Deploy pipeline started' });
});

app.get('/api/deploy/status', (_req, res) => {
  res.json({ status: deployStatus, error: deployError });
});

app.get('/api/deploy/stream', (_req, res) => {
  deploySSE.addClient(res);
});

// ── Cleanup ─────────────────────────────────────────────────────

function cleanup() {
  if (clientManager.running && clientManager.process) {
    treeKill(clientManager.process.pid, 'SIGKILL');
  }
  if (serverManager.running && serverManager.process) {
    treeKill(serverManager.process.pid, 'SIGKILL');
  }
}

process.on('SIGTERM', () => { cleanup(); process.exit(0); });
process.on('SIGINT', () => { cleanup(); process.exit(0); });

// ── Start ──────────────────────────────────────────────────────

app.listen(META_PORT, () => {
  console.log(`AnodeServer Meta running at http://localhost:${META_PORT}`);

  if (AUTO_DEPLOY) {
    console.log('[auto-deploy] Auto-deploy enabled, starting pipeline...');
    runDeployPipeline().then(result => {
      if (result.success) {
        console.log('[auto-deploy] Pipeline completed successfully');
      } else {
        console.error(`[auto-deploy] Pipeline failed: ${result.error}`);
      }
    });
  }
});
