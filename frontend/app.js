// L9: DOM 交互。下一阶段只需将 mockCompletion 替换为网关请求。
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
let busy = false; let count = 0;
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
function trace(step, state) { const row = document.querySelector(`[data-step="${step}"]`); row.className = `trace-step ${state}`; row.querySelector('.step-state').textContent = state === 'done' ? '已完成' : state === 'active' ? '处理中' : step === 0 ? '待发送' : '待处理'; }
function addMessage(role, text) { $('#welcome')?.remove(); const item = document.createElement('div'); item.className = `message ${role}`; const label = document.createElement('small'); label.textContent = role === 'user' ? '你' : '✳ Factory · 模拟回复'; const content = document.createElement('p'); content.textContent = text; item.append(label, content); $('#messages').append(item); $('#messages').scrollTop = $('#messages').scrollHeight; }
// 教学桩：不连接 Java、C++ 或 GPU，不产生真实模型 Token 计数。
async function mockCompletion(prompt) {
  trace(0, 'active'); await wait(180); trace(0, 'done'); trace(1, 'active');
  await wait(280); trace(1, 'done'); trace(2, 'active'); await wait(450);
  if (/网关|java/i.test(prompt)) return 'Java 网关是 L8 平台与网关层的入口。\n\n它接收请求、分配请求标识，后续会负责鉴权、限流与用量记录，再把工作交给 L7 推理服务。\n\n今天这一步由浏览器模拟；下一阶段，我们会让这个页面真正调用 Java 的 /v1/chat/completions 接口。';
  if (/gpu|请求|链路/i.test(prompt)) return '一次请求，会沿着系统逐层向下：\n\nL9 浏览器 → L8 Java 网关 → L7 C++ 推理服务 → L4 CUDA 运行时 → L1 GPU 硬件。\n\n多 GPU 实验还会涉及 L6 通信框架和 L2 互联。当前仅演示前三个阶段，没有执行真实 GPU 计算。\n\n试着观察右侧轨迹：每一层都有自己的职责和验收信号。';
  return '今天从一个最小闭环开始：输入问题 → 构造请求 → 展示响应。\n\n先读 frontend/app.js 中的提交事件和 mockCompletion，理解页面如何从“等待”变成“完成”。接着修改一条预设回复，重新发送并观察变化。\n\n这是固定教学回复，不是真实模型生成。等页面流程清楚后，再把模拟函数替换成 Java 网关请求。';
}
$('#chat-form').addEventListener('submit', async event => {
  event.preventDefault(); const prompt = $('#prompt').value.trim(); if (!prompt || busy) return;
  busy = true; $('#send').disabled = true; $('#clear-chat').disabled = true; $('#send').textContent = '处理中…';
  [0, 1, 2].forEach(step => trace(step, '')); $('#elapsed').textContent = '—';
  $('#request-id').textContent = 'REQ / ' + (crypto.randomUUID ? crypto.randomUUID().slice(0, 8) : Date.now().toString(36));
  $('#trace-status').textContent = '模拟请求进行中'; addMessage('user', prompt); $('#prompt').value = ''; const started = performance.now();
  try { const reply = await mockCompletion(prompt); addMessage('assistant', reply); trace(2, 'done'); count++; $('#request-count').textContent = `${count} 次`; $('#elapsed').textContent = `${Math.round(performance.now() - started)} ms`; $('#trace-status').textContent = '模拟请求已完成'; }
  catch { addMessage('assistant', '演示发生错误，请重新发送。'); $('#trace-status').textContent = '请求失败'; }
  finally { busy = false; $('#send').disabled = false; $('#clear-chat').disabled = false; $('#send').textContent = '发送请求 ↑'; $('#prompt').focus(); }
});
$('#prompt').addEventListener('keydown', event => { if (event.key === 'Enter' && !event.shiftKey && !event.isComposing) { event.preventDefault(); $('#chat-form').requestSubmit(); } });
document.querySelectorAll('[data-prompt]').forEach(button => button.addEventListener('click', () => { if (busy) return; $('#prompt').value = button.dataset.prompt; $('#chat-form').requestSubmit(); }));
const welcomeMarkup = $('#messages').innerHTML;
$('#clear-chat').addEventListener('click', () => { if (busy) return; $('#messages').innerHTML = welcomeMarkup; document.querySelectorAll('[data-prompt]').forEach(button => button.addEventListener('click', () => { $('#prompt').value = button.dataset.prompt; $('#chat-form').requestSubmit(); })); [0, 1, 2].forEach(step => trace(step, '')); $('#trace-status').textContent = '等待请求'; $('#request-id').textContent = 'REQ / —'; $('#elapsed').textContent = '—'; });
