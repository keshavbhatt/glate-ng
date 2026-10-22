// Draws Glate's main window into <div class="window" data-*>, so each mock only writes what
// is new. data-src / data-result: pane text ("" shows the placeholder); data-offline="1":
// offline mode on (pill and translate-button dot); data-caption="1": the "Translated on
// this device" caption; data-welcome="1": the first-run card in the result pane.
// Nothing here ships in the app.
(function () {
  const ic = (n, s) => `<svg class="ic${s ? ' s' : ''}"><use href="#i-${n}"/></svg>`;
  function render(w) {
    const d = w.dataset;
    const offline = d.offline === '1';
    const src = d.src ?? '';
    const result = d.result ?? '';
    const pill = offline
      ? `<span class="pill offline" title="Offline mode is on: translations happen on this computer. Click to manage languages.">${ic('laptop', 1)}Offline</span>`
      : '';
    const welcome = d.welcome === '1' ? `
      <div class="welcome"><div class="card">
        <h3>Welcome to Glate</h3>
        <p class="muted small">Translate text and listen to it, online or on this computer.</p>
        <ul>
          <li>${ic('type', 1)}<span>Type or paste on the left, then press <kbd>Shift</kbd>+<kbd>Enter</kbd> or the arrow.</span></li>
          <li>${ic('swap', 1)}<span>Pick both languages below; the middle button swaps them.</span></li>
          <li>${ic('laptop', 1)}<span>Turn on offline mode in Settings to translate without internet (Pro).</span></li>
        </ul>
        <span class="btn">${ic('sparkles', 1)}Take the tour</span>
      </div></div>` : '';
    const caption = d.caption === '1'
      ? `<div class="caption">${ic('laptop', 1)}Translated on this device, English to Spanish</div>` : '';
    w.innerHTML = `
      <div class="titlebar"><div class="lights"><i></i><i></i><i></i></div><span class="appicon"></span><div class="title">Glate</div><div class="wc"><span>&#x2212;</span><span>&#x25A1;</span><span>&#x2715;</span></div></div>
      <div class="bar top">
        <span class="ibtn" title="History">${ic('history')}</span>
        <span class="ibtn" title="Line by line">${ic('list')}</span>
        <span class="grow"></span>${pill}<span class="grow"></span>
        <span class="ibtn" data-id="help" title="Help">${ic('help')}</span>
        <span class="ibtn" title="Settings">${ic('sliders')}</span>
        <span class="ibtn" title="Account">${ic('user')}</span>
        <span class="ibtn" data-id="about" title="About Glate">${ic('info')}</span>
      </div>
      <div class="panes">
        <div class="pane">${src || '<span class="ph">Text to translate...</span>'}</div>
        <span class="go" title="${offline ? 'Translate offline' : 'Translate'} (Shift+Enter)">${ic('arrow-circle')}${offline ? '<i class="dot"></i>' : ''}</span>
        <div class="pane">${welcome || result || '<span class="ph">Translated text...</span>'}${caption}</div>
      </div>
      <div class="bar bottom">
        <span class="ibtn">${ic('volume')}</span><span class="ibtn">${ic('clipboard')}</span><span class="ibtn">${ic('x')}</span>
        <span class="combo">${ic('languages', 1)}<span class="grow">English</span>${ic('down', 1)}</span>
        <span class="counter">${src.replace(/<[^>]+>/g, '').length}/5000</span>
        <span class="grow"></span><span class="ibtn">${ic('swap')}</span><span class="grow"></span>
        <span class="combo">${ic('languages', 1)}<span class="grow">Spanish</span>${ic('down', 1)}</span>
        <span class="ibtn">${ic('copy')}</span><span class="ibtn">${ic('share')}</span><span class="ibtn">${ic('volume')}</span>
      </div>` + w.innerHTML;
  }
  document.addEventListener('DOMContentLoaded', () => document.querySelectorAll('.window[data-main]').forEach(render));
})();
