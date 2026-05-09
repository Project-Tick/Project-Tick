// pre.js — runs BEFORE the user's job steps.
// Installs a BASH_ENV hook that tees each step's output to a log file.

const fs = require('fs');
const path = require('path');
const os = require('os');

function getInput(name) {
  return process.env[`INPUT_${name.toUpperCase().replace(/-/g, '_')}`] || '';
}

function appendEnv(key, value) {
  const envFile = process.env.GITHUB_ENV;
  if (!envFile) return;
  fs.appendFileSync(envFile, `${key}=${value}\n`);
}

const callbackUrl = getInput('callback_url');
const callbackToken = getInput('callback_token');
const pipelineId = getInput('pipeline_id');
const baseUrl = getInput('foreman_base_url') || 'https://builds.projecttick.net';
const apiToken = getInput('foreman_api_token');
const jobName = getInput('job_name');

// DEBUG: dump all INPUT_ env vars to understand what GHA passed us
console.log('=== Log Collector Pre-Step Debug ===');
console.log('INPUT_CALLBACK_URL:', JSON.stringify(process.env.INPUT_CALLBACK_URL));
console.log('INPUT_CALLBACK_TOKEN length:', (process.env.INPUT_CALLBACK_TOKEN || '').length);
console.log('INPUT_PIPELINE_ID:', JSON.stringify(process.env.INPUT_PIPELINE_ID));
console.log('INPUT_FOREMAN_API_TOKEN length:', (process.env.INPUT_FOREMAN_API_TOKEN || '').length);
console.log('INPUT_JOB_NAME:', JSON.stringify(process.env.INPUT_JOB_NAME));
console.log('Resolved values:');
console.log('  callbackUrl:', JSON.stringify(callbackUrl));
console.log('  pipelineId:', JSON.stringify(pipelineId));
console.log('  apiToken length:', apiToken.length);
console.log('=====================================');

if (!callbackUrl && !pipelineId) {
  console.log('No Foreman destination; log collector disabled.');
  process.exit(0);
}

const logDir = path.join(process.env.RUNNER_TEMP || os.tmpdir(), '_foreman_logs');
fs.mkdirSync(logDir, { recursive: true });

// Save config for main and post steps to read
const config = {
  callbackUrl,
  callbackToken,
  pipelineId,
  baseUrl,
  apiToken,
  jobName,
  logDir,
};
fs.writeFileSync(path.join(logDir, 'config.json'), JSON.stringify(config));

// Step counter
fs.writeFileSync(path.join(logDir, '.step'), '-1');

// BASH_ENV hook script — sourced before every bash step.
// Increments step counter and tees stdout/stderr to per-step log file.
const hookScript = `
if [ -d "${logDir}" ]; then
  _step_file="${logDir}/.step"
  _snum=$(( $(cat "$_step_file" 2>/dev/null || echo -1) + 1 ))
  echo "$_snum" > "$_step_file"
  _logf="${logDir}/step_\${_snum}.log"
  exec > >(tee -a "$_logf") 2> >(tee -a "$_logf" >&2)
fi
`;
fs.writeFileSync(path.join(logDir, 'hook.sh'), hookScript);

// Export env vars for subsequent steps
appendEnv('FOREMAN_LOG_DIR', logDir);
appendEnv('BASH_ENV', path.join(logDir, 'hook.sh'));

console.log(`Log collector pre-step: BASH_ENV hook installed at ${logDir}/hook.sh`);
console.log(`Job: ${jobName}, dest: ${callbackUrl || `admin via pipeline ${pipelineId}`}`);
