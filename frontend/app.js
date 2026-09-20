// L9 → L8 Java → L7 C++：服务状态与阶段耗时均来自真实 HTTP 响应。
const $ = selector => document.querySelector(selector);
const weeks = [
  ['L8', '建立系统骨架', 'Java 网关与 Mock Runtime 跑通端到端请求。'],
  ['L4', 'C++ 内存与生命周期', '实现请求、响应与缓冲区，验证资源正确释放。'],
  ['L3 / L4', '并发与网络服务', 'Java 调用 C++，验证有界队列、超时与优雅退出。'],
  ['L4', '第一批 CUDA 算子', '向量加法与归约，比较 CPU 和 GPU 结果。'],
  ['L4', '矩阵与异步执行', '实现矩阵乘法，记录正确性与性能基线。'],
  ['L4', '用证据定位性能', '观察请求时间线，找出并验证首要瓶颈。'],
  ['L2', '理解 GPU 拓扑', '检查双 GPU 访问能力，比较传输性能。'],
  ['L6', '多 GPU 集合通信', '验证 AllReduce 结果与分布式训练同步开销。'],
  ['L7', '走进推理主链路', '实现简化注意力与采样，理解输入到输出。'],
  ['L7', '缓存与批处理', '实现缓存管理、Token 预算与多请求调度。'],
  ['L8', '完整服务集成', '贯通鉴权、限流、超时传播与幂等计量。'],
  ['L5', '部署与系统答辩', '完成部署、压测、故障实验与端到端演示。']
];
function readSaved(key, fallback) { try { return JSON.parse(localStorage.getItem(key)) ?? fallback; } catch { return fallback; } }
function save(key, value) { try { localStorage.setItem(key, JSON.stringify(value)); return true; } catch { return false; } }
let completed = readSaved('tf-weeks', []);
if (!Array.isArray(completed)) completed = [];
completed = [...new Set(completed.filter(n => Number.isInteger(n) && n >= 0 && n < 12))];
function updateProgress() {
  $('#side-progress').style.width = `${completed.length / 12 * 100}%`;
  $('#side-count').textContent = `${completed.length} / 12 周完成`;
  $('#roadmap-count').textContent = `${completed.length} / 12`;
}
weeks.forEach(([layer, title, goal], index) => {
  const card = document.createElement('article'); card.className = 'card week-card';
  card.innerHTML = `<span class="eyebrow">WEEK ${String(index + 1).padStart(2, '0')} / ${layer}</span><h2>${title}</h2><p>${goal}</p><label><input type="checkbox"> 已达到本周验收目标</label>`;
  const checkbox = card.querySelector('input'); checkbox.checked = completed.includes(index);
  checkbox.addEventListener('change', () => { completed = checkbox.checked ? [...completed, index] : completed.filter(n => n !== index); updateProgress(); if (!save('tf-weeks', completed)) { checkbox.parentElement.lastChild.textContent = ' 当前会话已记录，浏览器存储不可用'; } });
  $('#week-grid').append(card);
});
updateProgress();
function navigate(view) {
  if (!['playground', 'roadmap', 'notes'].includes(view)) view = 'playground';
  document.querySelectorAll('.page').forEach(page => page.hidden = page.id !== view);
  document.querySelectorAll('.nav').forEach(button => { const active = button.dataset.view === view; button.classList.toggle('active', active); if (active) button.setAttribute('aria-current', 'page'); else button.removeAttribute('aria-current'); });
  $('#page-name').textContent = { playground: '推理实验室', roadmap: '学习路线', notes: '实验笔记' }[view];
}
document.querySelectorAll('.nav').forEach(button => button.addEventListener('click', () => { location.hash = button.dataset.view; }));
$('#open-roadmap').addEventListener('click', () => { location.hash = 'roadmap'; });
window.addEventListener('hashchange', () => navigate(location.hash.slice(1))); navigate(location.hash.slice(1));
const savedNote = readSaved('tf-note', ''); $('#note-text').value = typeof savedNote === 'string' ? savedNote : '';
$('#note-text').addEventListener('input', () => { $('#save-status').textContent = '有未保存的修改'; });
$('#save-note').addEventListener('click', () => { $('#save-status').textContent = save('tf-note', $('#note-text').value) ? '已保存到当前浏览器 · ' + new Date().toLocaleTimeString('zh-CN') : '保存失败：浏览器存储不可用，请复制备份'; });
let busy = false; let count = 0; let runtimeName = '运行时';
function showRuntime(mode) {
  runtimeName = mode === 'cpp-cpu-demo' ? 'C++ CPU 演示' : 'Java Mock';
  $('#runtime-name').textContent = runtimeName;
}
async function refreshReadiness() {
  $('#connection-status').textContent = '检查服务状态…';
  try {
    const response = await fetch('/ready', { signal: AbortSignal.timeout(3000) });
    const data = await response.json();
    showRuntime(data.runtime);
    $('#connection-status').textContent = response.ok ? `已连接 · ${runtimeName}` : '网关在线 · 下游未就绪';
  } catch { $('#connection-status').textContent = '网关不可用'; }
}
$('#refresh-status').addEventListener('click', refreshReadiness);
refreshReadiness();
function trace(step, state) { const row = document.querySelector(`[data-step="${step}"]`); row.className = `trace-step ${state}`; row.querySelector('.step-state').textContent = state === 'done' ? '已完成' : state === 'active' ? '处理中' : step === 0 ? '待发送' : '待处理'; }
function addMessage(role, text) { $('#welcome')?.remove(); const item = document.createElement('div'); item.className = `message ${role}`; const label = document.createElement('small'); label.textContent = role === 'user' ? '你' : role === 'error' ? '请求错误' : `✳ ${runtimeName}`; const content = document.createElement('p'); content.textContent = text; item.append(label, content); $('#messages').append(item); $('#messages').scrollTop = $('#messages').scrollHeight; }
async function gatewayCompletion(prompt) {
  trace(0, 'done'); trace(1, 'active');
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 10000);
  try {
    const response = await fetch('/v1/chat/completions', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, signal: controller.signal,
      body: JSON.stringify({ model: 'factory-mock-v1', messages: [{ role: 'user', content: prompt }], stream: false })
    });
    const requestId = response.headers.get('X-Request-Id');
    $('#request-id').textContent = requestId || '未收到请求标识';
    $('#request-id').title = requestId || '';
    let data;
    try { data = await response.json(); } catch { throw new Error('响应不是 JSON，请从 Java 网关地址打开页面。'); }
    if (!response.ok) {
      const error = new Error(data.error?.message || `网关返回 HTTP ${response.status}`);
      error.downstream = String(data.error?.code || '').includes('runtime');
      throw error;
    }
    const reply = data.choices?.[0]?.message?.content;
    if (typeof reply !== 'string') throw new Error('网关响应结构不正确');
    trace(1, 'done'); trace(2, 'done');
    showRuntime(data.metadata.runtime);
    $('#connection-status').textContent = `已连接 · ${runtimeName}`;
    $('#queue-time').textContent = `${Number(data.metadata.queue_ms).toFixed(2)} ms`;
    $('#compute-time').textContent = `${Number(data.metadata.compute_ms).toFixed(2)} ms`;
    $('#trace-status').textContent = `已完成 · Java 服务端 ${Number(data.metadata?.server_ms || 0).toFixed(1)} ms`;
    return reply;
  } catch (error) {
    if (error.name === 'AbortError') throw new Error('请求超过 10 秒，已取消等待；请检查 Java 网关。');
    if (error instanceof TypeError) throw new Error('无法连接 Java 网关，请确认服务已启动。');
    throw error;
  } finally { clearTimeout(timer); }
}
$('#chat-form').addEventListener('submit', async event => {
  event.preventDefault(); const prompt = $('#prompt').value.trim(); if (!prompt || busy) return;
  busy = true; $('#send').disabled = true; $('#clear-chat').disabled = true; $('#send').textContent = '处理中…';
  [0, 1, 2].forEach(step => trace(step, '')); $('#elapsed').textContent = '—';
  $('#queue-time').textContent = '—'; $('#compute-time').textContent = '—';
  $('#request-id').textContent = '等待服务端标识';
  $('#request-id').title = '';
  $('#trace-status').textContent = '等待 Java 网关响应'; addMessage('user', prompt); $('#prompt').value = ''; const started = performance.now();
  try { const reply = await gatewayCompletion(prompt); addMessage('assistant', reply); count++; $('#request-count').textContent = `${count} 次`; $('#elapsed').textContent = `${Math.round(performance.now() - started)} ms`; }
  catch (error) { addMessage('error', error.message); trace(1, error.downstream ? 'done' : ''); document.querySelector(`[data-step="${error.downstream ? 2 : 1}"] .step-state`).textContent = '失败'; $('#trace-status').textContent = '请求失败'; $('#prompt').value = prompt; refreshReadiness(); }
  finally { busy = false; $('#send').disabled = false; $('#clear-chat').disabled = false; $('#send').textContent = '发送请求 ↑'; $('#prompt').focus(); }
});
$('#prompt').addEventListener('keydown', event => { if (event.key === 'Enter' && !event.shiftKey && !event.isComposing) { event.preventDefault(); $('#chat-form').requestSubmit(); } });
document.querySelectorAll('[data-prompt]').forEach(button => button.addEventListener('click', () => { if (busy) return; $('#prompt').value = button.dataset.prompt; $('#chat-form').requestSubmit(); }));
const welcomeMarkup = $('#messages').innerHTML;
$('#clear-chat').addEventListener('click', () => { if (busy) return; $('#messages').innerHTML = welcomeMarkup; document.querySelectorAll('[data-prompt]').forEach(button => button.addEventListener('click', () => { $('#prompt').value = button.dataset.prompt; $('#chat-form').requestSubmit(); })); [0, 1, 2].forEach(step => trace(step, '')); $('#trace-status').textContent = '等待请求'; $('#request-id').textContent = 'REQ / —'; $('#elapsed').textContent = '—'; });
