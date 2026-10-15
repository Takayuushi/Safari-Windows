/**
 * SAFARI 26 FOR WINDOWS — CORE BROWSER CONTROLLER
 */

class SafariBrowser {
  constructor() {
    this.tabs = [];
    this.activeTabId = null;
    this.nextTabId = 1;
    this.settings = this.loadSettings();
    this.bookmarks = this.loadBookmarks();
    this.readingList = this.loadReadingList();
    this.history = this.loadHistory();
    this.downloads = [];

    this.initElements();
    this.initEvents();
    this.applySettings();

    // Start with initial tab
    this.createNewTab(this.settings.homepage || 'safari://start');
  }

  /* ------------------------------------------------------------------------
     SETTINGS & STORAGE
     ------------------------------------------------------------------------ */
  loadSettings() {
    const defaults = {
      theme: 'system',
      searchEngine: 'google',
      homepage: 'safari://start',
      newTabMode: 'start',
      defaultZoom: 100,
      showFullUrl: false,
      blockTrackers: true,
      hardwareAccel: true
    };
    try {
      const stored = localStorage.getItem('safari_settings');
      return stored ? { ...defaults, ...JSON.parse(stored) } : defaults;
    } catch {
      return defaults;
    }
  }

  saveSettings() {
    localStorage.setItem('safari_settings', JSON.stringify(this.settings));
    this.applySettings();
  }

  applySettings() {
    // Theme
    const root = document.documentElement;
    if (this.settings.theme === 'system') {
      const isDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
      root.setAttribute('data-theme', isDark ? 'dark' : 'light');
    } else {
      root.setAttribute('data-theme', this.settings.theme);
    }

    // Apply Zoom to webview slots
    const zoomFactor = (this.settings.defaultZoom || 100) / 100;
    const slots = document.getElementById('webview-slots');
    if (slots) slots.style.zoom = zoomFactor;
  }

  loadBookmarks() {
    try {
      const stored = localStorage.getItem('safari_bookmarks');
      return stored ? JSON.parse(stored) : [
        { title: 'Apple', url: 'https://www.apple.com' },
        { title: 'Google', url: 'https://www.google.com' },
        { title: 'YouTube', url: 'https://www.youtube.com' },
        { title: 'GitHub', url: 'https://github.com' }
      ];
    } catch { return []; }
  }

  saveBookmarks() {
    localStorage.setItem('safari_bookmarks', JSON.stringify(this.bookmarks));
    this.renderSidebarContent();
  }

  loadReadingList() {
    try {
      const stored = localStorage.getItem('safari_reading_list');
      return stored ? JSON.parse(stored) : [
        { title: 'WebKit Standards Documentation', url: 'https://webkit.org' },
        { title: 'Safari Technology Preview News', url: 'https://developer.apple.com/safari' }
      ];
    } catch { return []; }
  }

  saveReadingList() {
    localStorage.setItem('safari_reading_list', JSON.stringify(this.readingList));
    this.renderSidebarContent();
  }

  loadHistory() {
    try {
      const stored = localStorage.getItem('safari_history');
      return stored ? JSON.parse(stored) : [];
    } catch { return []; }
  }

  addHistoryEntry(title, url) {
    if (url.startsWith('safari://')) return;
    this.history.unshift({ title: title || url, url, time: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) });
    if (this.history.length > 200) this.history.pop();
    localStorage.setItem('safari_history', JSON.stringify(this.history));
    this.renderSidebarContent();
  }

  /* ------------------------------------------------------------------------
     DOM INITIALIZATION
     ------------------------------------------------------------------------ */
  initElements() {
    this.el = {
      shell: document.getElementById('safari-shell'),
      tabBar: document.getElementById('tab-bar'),
      tabsContainer: document.getElementById('tabs-container'),
      webviewSlots: document.getElementById('webview-slots'),
      addressPill: document.getElementById('smart-search-field'),
      addressDisplay: document.getElementById('address-display'),
      addressInput: document.getElementById('address-input'),
      sslLock: document.getElementById('ssl-lock-icon'),
      btnReload: document.getElementById('btn-reload'),
      btnBack: document.getElementById('btn-back'),
      btnForward: document.getElementById('btn-forward'),
      btnNewTab: document.getElementById('btn-new-tab'),
      btnTabOverview: document.getElementById('btn-tab-overview'),
      btnToggleSidebar: document.getElementById('btn-toggle-sidebar'),
      sidebar: document.getElementById('safari-sidebar'),
      btnDownloads: document.getElementById('btn-downloads'),
      downloadsPopover: document.getElementById('downloads-popover'),
      downloadsList: document.getElementById('downloads-list'),
      btnPageMenu: document.getElementById('btn-page-menu'),
      pageMenuDropdown: document.getElementById('page-action-dropdown'),
      settingsModal: document.getElementById('settings-modal'),
      tabOverviewOverlay: document.getElementById('tab-overview-overlay'),
      overviewGrid: document.getElementById('overview-grid'),
      readerOverlay: document.getElementById('reader-view-overlay'),
      readerContent: document.getElementById('reader-article-content'),
      btnReaderExit: document.getElementById('btn-reader-exit'),
      findBar: document.getElementById('find-bar'),
      findInput: document.getElementById('find-input'),
      findCounter: document.getElementById('find-counter')
    };
  }

  initEvents() {
    // Window control buttons
    document.getElementById('btn-window-close').addEventListener('click', () => {
      if (window.__TAURI__) window.__TAURI__.window.appWindow.close();
      else if (window.pywebview) window.pywebview.api.close();
      else window.close();
    });
    document.getElementById('btn-window-min').addEventListener('click', () => {
      if (window.__TAURI__) window.__TAURI__.window.appWindow.minimize();
      else if (window.pywebview) window.pywebview.api.minimize();
    });
    document.getElementById('btn-window-max').addEventListener('click', () => {
      if (window.__TAURI__) window.__TAURI__.window.appWindow.toggleMaximize();
      else if (window.pywebview) window.pywebview.api.toggle_maximize();
    });

    // Navigation & New Tab
    this.el.btnNewTab.addEventListener('click', () => this.createNewTab('safari://start'));
    this.el.btnBack.addEventListener('click', () => this.navigateActiveTab(-1));
    this.el.btnForward.addEventListener('click', () => this.navigateActiveTab(1));
    this.el.btnReload.addEventListener('click', () => this.reloadActiveTab());

    // Address Bar interaction
    this.el.addressPill.addEventListener('click', () => this.focusAddressBar());
    this.el.addressInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        this.commitAddress(this.el.addressInput.value);
      } else if (e.key === 'Escape') {
        this.blurAddressBar();
      }
    });
    this.el.addressInput.addEventListener('blur', () => this.blurAddressBar());

    // Sidebar Toggle
    this.el.btnToggleSidebar.addEventListener('click', () => {
      this.el.sidebar.classList.toggle('hidden');
    });

    // Sidebar Segmented control
    document.querySelectorAll('.segment-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.segment-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        const tab = btn.getAttribute('data-tab');
        document.querySelectorAll('.sidebar-content').forEach(c => c.classList.add('hidden'));
        const target = document.getElementById(`sidebar-content-${tab}`);
        if (target) target.classList.remove('hidden');
      });
    });

    // Sidebar Filter
    document.getElementById('sidebar-filter-input').addEventListener('input', (e) => {
      this.filterSidebar(e.target.value.toLowerCase());
    });

    // Clear History button
    document.getElementById('btn-clear-history').addEventListener('click', () => {
      this.history = [];
      localStorage.removeItem('safari_history');
      this.renderSidebarContent();
    });

    // Page menu dropdown
    this.el.btnPageMenu.addEventListener('click', (e) => {
      e.stopPropagation();
      this.el.pageMenuDropdown.classList.toggle('hidden');
    });
    document.addEventListener('click', () => {
      this.el.pageMenuDropdown.classList.add('hidden');
      this.el.downloadsPopover.classList.add('hidden');
    });

    // Downloads Popover
    this.el.btnDownloads.addEventListener('click', (e) => {
      e.stopPropagation();
      this.el.downloadsPopover.classList.toggle('hidden');
    });
    document.getElementById('btn-clear-downloads').addEventListener('click', () => {
      this.downloads = [];
      this.renderDownloads();
    });

    // Zoom buttons in page menu
    document.getElementById('btn-zoom-in').addEventListener('click', (e) => {
      e.stopPropagation();
      this.adjustZoom(10);
    });
    document.getElementById('btn-zoom-out').addEventListener('click', (e) => {
      e.stopPropagation();
      this.adjustZoom(-10);
    });

    // Bookmark & Reading list actions from page menu
    document.getElementById('btn-menu-bookmark').addEventListener('click', () => {
      const tab = this.getActiveTab();
      if (tab) {
        this.bookmarks.push({ title: tab.title, url: tab.url });
        this.saveBookmarks();
      }
    });
    document.getElementById('btn-menu-readinglist').addEventListener('click', () => {
      const tab = this.getActiveTab();
      if (tab) {
        this.readingList.push({ title: tab.title, url: tab.url });
        this.saveReadingList();
      }
    });
    document.getElementById('btn-menu-reader').addEventListener('click', () => {
      this.toggleReaderMode();
    });
    this.el.btnReaderExit.addEventListener('click', () => {
      this.el.readerOverlay.classList.add('hidden');
    });

    // Tab Overview (Exposé)
    this.el.btnTabOverview.addEventListener('click', () => this.toggleTabOverview());
    document.getElementById('btn-close-overview').addEventListener('click', () => this.toggleTabOverview());

    // Settings Modal
    document.getElementById('btn-menu-settings').addEventListener('click', () => {
      this.openSettingsModal();
    });
    document.getElementById('btn-close-settings').addEventListener('click', () => {
      this.closeSettingsModal();
    });

    // Settings Navigation
    document.querySelectorAll('.settings-nav-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.settings-nav-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        const tab = btn.getAttribute('data-tab');
        document.querySelectorAll('.settings-pane').forEach(p => p.classList.add('hidden'));
        const pane = document.getElementById(`pane-${tab}`);
        if (pane) pane.classList.remove('hidden');
      });
    });

    // Settings inputs live binding
    document.getElementById('setting-theme').addEventListener('change', (e) => {
      this.settings.theme = e.target.value;
      this.saveSettings();
    });
    document.getElementById('setting-search-engine').addEventListener('change', (e) => {
      this.settings.searchEngine = e.target.value;
      this.saveSettings();
    });
    document.getElementById('setting-homepage').addEventListener('change', (e) => {
      this.settings.homepage = e.target.value;
      this.saveSettings();
    });
    document.getElementById('setting-default-zoom').addEventListener('change', (e) => {
      this.settings.defaultZoom = parseInt(e.target.value, 10);
      this.saveSettings();
    });

    // Keyboard Shortcuts (Ctrl+T, Ctrl+W, Ctrl+R, Ctrl+F, Ctrl+Shift+\)
    window.addEventListener('keydown', (e) => {
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 't') {
        e.preventDefault();
        this.createNewTab('safari://start');
      } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'w') {
        e.preventDefault();
        if (this.activeTabId) this.closeTab(this.activeTabId);
      } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'r') {
        e.preventDefault();
        this.reloadActiveTab();
      } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'l') {
        e.preventDefault();
        this.focusAddressBar();
      } else if (e.ctrlKey && e.shiftKey && e.key === '|') {
        e.preventDefault();
        this.toggleTabOverview();
      } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'f') {
        e.preventDefault();
        this.toggleFindBar();
      }
    });

    this.renderSidebarContent();
  }

  /* ------------------------------------------------------------------------
     TAB MANAGEMENT
     ------------------------------------------------------------------------ */
  createNewTab(url = 'safari://start') {
    const id = this.nextTabId++;
    const tab = {
      id,
      url,
      title: url === 'safari://start' ? 'Start Page' : 'New Tab',
      history: [url],
      historyIndex: 0,
      zoom: this.settings.defaultZoom || 100
    };

    this.tabs.push(tab);

    // Create webview iframe or start page DOM
    const slot = document.createElement('div');
    slot.id = `webview-slot-${id}`;
    slot.className = 'webview-frame';
    this.el.webviewSlots.appendChild(slot);

    this.renderTabs();
    this.activateTab(id);
    this.loadUrlInTab(tab, url);
  }

  activateTab(id) {
    this.activeTabId = id;
    const tab = this.getActiveTab();
    if (!tab) return;

    // Show active webview slot
    document.querySelectorAll('.webview-frame').forEach(slot => {
      slot.classList.toggle('active', slot.id === `webview-slot-${id}`);
    });

    // Update UI address bar & title
    this.updateAddressBar(tab);
    document.title = `${tab.title} — Safari`;

    this.renderTabs();
  }

  closeTab(id) {
    const index = this.tabs.findIndex(t => t.id === id);
    if (index === -1) return;

    // Remove DOM element
    const slot = document.getElementById(`webview-slot-${id}`);
    if (slot) slot.remove();

    this.tabs.splice(index, 1);

    if (this.tabs.length === 0) {
      // Reopen a fresh start page if all tabs closed
      this.createNewTab('safari://start');
      return;
    }

    // Switch to adjacent tab
    const nextIndex = Math.max(0, index - 1);
    this.activateTab(this.tabs[nextIndex].id);
    this.renderTabs();
  }

  getActiveTab() {
    return this.tabs.find(t => t.id === this.activeTabId);
  }

  renderTabs() {
    // Safari 26 rule: only show the tab bar if there are 2 or more tabs!
    if (this.tabs.length > 1) {
      this.el.tabBar.classList.remove('hidden');
    } else {
      this.el.tabBar.classList.add('hidden');
    }

    this.el.tabsContainer.innerHTML = '';
    this.tabs.forEach(tab => {
      const tabEl = document.createElement('div');
      tabEl.className = `tab-item ${tab.id === this.activeTabId ? 'active' : ''}`;
      
      // Calculate letter badge & hue
      const domain = this.extractDomain(tab.url);
      const letter = domain ? domain.charAt(0).toUpperCase() : 'S';
      const hue = this.calculateHue(domain || 'safari');

      tabEl.innerHTML = `
        <div class="tab-badge" style="background: hsl(${hue}, 52%, 46%);">${letter}</div>
        <span class="tab-title" title="${tab.title}">${tab.title}</span>
        <button class="tab-close-btn" title="Close Tab">
          <svg viewBox="0 0 12 12" style="width:8px;height:8px;stroke:currentColor;stroke-width:1.8;"><path d="M2.5 2.5l7 7M9.5 2.5l-7 7"/></svg>
        </button>
      `;

      tabEl.addEventListener('click', (e) => {
        if (!e.target.closest('.tab-close-btn')) {
          this.activateTab(tab.id);
        }
      });

      tabEl.querySelector('.tab-close-btn').addEventListener('click', (e) => {
        e.stopPropagation();
        this.closeTab(tab.id);
      });

      this.el.tabsContainer.appendChild(tabEl);
    });
  }

  /* ------------------------------------------------------------------------
     NAVIGATION & CONTENT
     ------------------------------------------------------------------------ */
  loadUrlInTab(tab, url) {
    tab.url = url;
    const slot = document.getElementById(`webview-slot-${tab.id}`);
    if (!slot) return;

    if (url === 'safari://start') {
      tab.title = 'Start Page';
      slot.innerHTML = this.renderStartPageHtml();
      this.attachStartPageEvents(slot);
      this.updateAddressBar(tab);
      this.renderTabs();
      return;
    }

    if (url === 'safari://downloads') {
      tab.title = 'Downloads';
      slot.innerHTML = this.renderDownloadsPageHtml();
      this.updateAddressBar(tab);
      this.renderTabs();
      return;
    }

    // Normal web URL: render in secure responsive iframe
    tab.title = this.extractDomain(url) || url;
    this.updateAddressBar(tab);
    this.renderTabs();

    slot.innerHTML = `
      <iframe src="${url}" class="webview-frame active" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowfullscreen sandbox="allow-same-origin allow-scripts allow-forms allow-popups allow-modals"></iframe>
    `;

    const iframe = slot.querySelector('iframe');
    iframe.onload = () => {
      try {
        if (iframe.contentDocument && iframe.contentDocument.title) {
          tab.title = iframe.contentDocument.title;
        }
      } catch {
        // Cross-origin fallback to domain
        tab.title = this.extractDomain(url) || url;
      }
      this.addHistoryEntry(tab.title, url);
      this.updateAddressBar(tab);
      this.renderTabs();
    };
  }

  commitAddress(input) {
    const normalized = this.normalizeInput(input);
    const tab = this.getActiveTab();
    if (tab && normalized) {
      tab.history.push(normalized);
      tab.historyIndex = tab.history.length - 1;
      this.loadUrlInTab(tab, normalized);
    }
    this.blurAddressBar();
  }

  normalizeInput(raw) {
    const trimmed = (raw || '').trim();
    if (!trimmed) return 'safari://start';

    if (trimmed.startsWith('safari://') || trimmed.startsWith('http://') || trimmed.startsWith('https://')) {
      return trimmed;
    }

    // Check if it's a domain name (e.g. google.com or localhost:3000)
    const isDomain = trimmed.includes('.') && !trimmed.includes(' ') || trimmed.startsWith('localhost');
    if (isDomain) {
      return `https://${trimmed}`;
    }

    // Query search engine
    const query = encodeURIComponent(trimmed);
    const engine = this.settings.searchEngine || 'google';
    switch (engine) {
      case 'duckduckgo': return `https://duckduckgo.com/?q=${query}`;
      case 'bing': return `https://www.bing.com/search?q=${query}`;
      case 'ecosia': return `https://www.ecosia.org/search?q=${query}`;
      case 'google':
      default:
        return `https://www.google.com/search?q=${query}`;
    }
  }

  updateAddressBar(tab) {
    if (!tab) return;
    const isStart = tab.url === 'safari://start';
    const isSecure = tab.url.startsWith('https://');

    this.el.sslLock.style.display = isSecure ? 'inline-flex' : 'none';

    if (isStart) {
      this.el.addressDisplay.textContent = 'Search or enter website name';
      this.el.addressInput.value = '';
    } else {
      const displayUrl = this.settings.showFullUrl ? tab.url : (this.extractDomain(tab.url) || tab.url);
      this.el.addressDisplay.textContent = displayUrl;
      this.el.addressInput.value = tab.url;
    }

    this.el.btnBack.disabled = tab.historyIndex <= 0;
    this.el.btnForward.disabled = tab.historyIndex >= tab.history.length - 1;
  }

  focusAddressBar() {
    this.el.addressPill.classList.add('focused');
    this.el.addressInput.focus();
    this.el.addressInput.select();
  }

  blurAddressBar() {
    this.el.addressPill.classList.remove('focused');
    const tab = this.getActiveTab();
    if (tab) this.updateAddressBar(tab);
  }

  navigateActiveTab(delta) {
    const tab = this.getActiveTab();
    if (!tab) return;
    const targetIdx = tab.historyIndex + delta;
    if (targetIdx >= 0 && targetIdx < tab.history.length) {
      tab.historyIndex = targetIdx;
      this.loadUrlInTab(tab, tab.history[targetIdx]);
    }
  }

  reloadActiveTab() {
    const tab = this.getActiveTab();
    if (tab) this.loadUrlInTab(tab, tab.url);
  }

  adjustZoom(deltaPercent) {
    const tab = this.getActiveTab();
    if (!tab) return;
    tab.zoom = Math.max(50, Math.min(250, (tab.zoom || 100) + deltaPercent));
    const slot = document.getElementById(`webview-slot-${tab.id}`);
    if (slot) slot.style.zoom = tab.zoom / 100;
    document.getElementById('zoom-level-text').textContent = `${tab.zoom}%`;
  }

  /* ------------------------------------------------------------------------
     START PAGE & FAVORITES
     ------------------------------------------------------------------------ */
  renderStartPageHtml() {
    const favorites = [
      { name: 'Apple', url: 'https://www.apple.com', letter: 'A', hue: 210 },
      { name: 'Google', url: 'https://www.google.com', letter: 'G', hue: 8 },
      { name: 'YouTube', url: 'https://www.youtube.com', letter: 'Y', hue: 0 },
      { name: 'Wikipedia', url: 'https://www.wikipedia.org', letter: 'W', hue: 220 },
      { name: 'GitHub', url: 'https://github.com', letter: 'G', hue: 250 },
      { name: 'Reddit', url: 'https://www.reddit.com', letter: 'R', hue: 18 },
      { name: 'Netflix', url: 'https://www.netflix.com', letter: 'N', hue: 355 },
      { name: 'Downloads', url: 'safari://downloads', letter: 'D', hue: 180 }
    ];

    return `
      <div class="safari-start-page">
        <div class="start-page-inner">
          <h2 class="start-section-title">Favorites</h2>
          <div class="favorites-grid">
            ${favorites.map(f => `
              <div class="favorite-tile" data-url="${f.url}">
                <div class="tile-icon" style="background: hsl(${f.hue}, 52%, 46%);">${f.letter}</div>
                <span class="tile-label">${f.name}</span>
              </div>
            `).join('')}
          </div>
        </div>
      </div>
    `;
  }

  attachStartPageEvents(slot) {
    slot.querySelectorAll('.favorite-tile').forEach(tile => {
      tile.addEventListener('click', () => {
        const url = tile.getAttribute('data-url');
        const tab = this.getActiveTab();
        if (tab && url) {
          tab.history.push(url);
          tab.historyIndex = tab.history.length - 1;
          this.loadUrlInTab(tab, url);
        }
      });
    });
  }

  /* ------------------------------------------------------------------------
     READER MODE
     ------------------------------------------------------------------------ */
  toggleReaderMode() {
    const tab = this.getActiveTab();
    if (!tab) return;
    const overlay = this.el.readerOverlay;
    if (!overlay.classList.contains('hidden')) {
      overlay.classList.add('hidden');
      return;
    }

    // Render Clean Reader Article
    this.el.readerContent.innerHTML = `
      <h1>${tab.title}</h1>
      <p style="color:var(--text-secondary);font-size:14px;margin-bottom:24px;">Published from ${this.extractDomain(tab.url)}</p>
      <p>This is a distraction-free view formatted by Safari Reader. Text typography is optimized for readability, with zero advertisements, sidebars, or layout clutter.</p>
      <p>Safari Reader is built directly into Safari's Smart Search field. Customize your font size or theme using the palette buttons above to switch between White, Sepia, Gray, and Dark mode.</p>
    `;
    overlay.classList.remove('hidden');

    // Attach theme picker events
    overlay.querySelectorAll('.theme-dot').forEach(dot => {
      dot.onclick = () => {
        overlay.querySelectorAll('.theme-dot').forEach(d => d.classList.remove('active'));
        dot.classList.add('active');
        overlay.setAttribute('data-reader-theme', dot.getAttribute('data-theme'));
      };
    });
  }

  /* ------------------------------------------------------------------------
     TAB OVERVIEW (EXPOSÉ)
     ------------------------------------------------------------------------ */
  toggleTabOverview() {
    const isHidden = this.el.tabOverviewOverlay.classList.contains('hidden');
    if (isHidden) {
      this.el.overviewGrid.innerHTML = '';
      this.tabs.forEach(tab => {
        const card = document.createElement('div');
        card.className = 'overview-card';
        card.innerHTML = `
          <div class="overview-card-header">
            <span class="overview-card-title">${tab.title}</span>
            <button class="icon-btn mini close-card-btn" title="Close"><svg viewBox="0 0 12 12" style="width:8px;height:8px;stroke:currentColor;stroke-width:2;"><path d="M2.5 2.5l7 7M9.5 2.5l-7 7"/></svg></button>
          </div>
          <div class="overview-thumbnail">${this.extractDomain(tab.url) || 'Start Page'}</div>
        `;
        card.addEventListener('click', (e) => {
          if (!e.target.closest('.close-card-btn')) {
            this.activateTab(tab.id);
            this.el.tabOverviewOverlay.classList.add('hidden');
          }
        });
        card.querySelector('.close-card-btn').addEventListener('click', (e) => {
          e.stopPropagation();
          this.closeTab(tab.id);
          card.remove();
        });
        this.el.overviewGrid.appendChild(card);
      });
      this.el.tabOverviewOverlay.classList.remove('hidden');
    } else {
      this.el.tabOverviewOverlay.classList.add('hidden');
    }
  }

  /* ------------------------------------------------------------------------
     SIDEBAR CONTENT & FILTERING
     ------------------------------------------------------------------------ */
  renderSidebarContent() {
    // Bookmarks
    const bookmarksEl = document.getElementById('sidebar-content-bookmarks');
    bookmarksEl.innerHTML = this.bookmarks.map((b, i) => `
      <div class="sidebar-item" data-url="${b.url}">
        <span class="sidebar-item-title">${b.title}</span>
        <button class="icon-btn mini" onclick="safari.deleteBookmark(${i})">×</button>
      </div>
    `).join('') || '<div class="empty-state" style="padding:20px;text-align:center;color:var(--text-tertiary);">No Bookmarks</div>';

    // Reading List
    const readingListEl = document.getElementById('sidebar-content-reading-list');
    readingListEl.innerHTML = this.readingList.map((r, i) => `
      <div class="sidebar-item" data-url="${r.url}">
        <span class="sidebar-item-title">${r.title}</span>
        <button class="icon-btn mini" onclick="safari.deleteReadingListItem(${i})">×</button>
      </div>
    `).join('') || '<div class="empty-state" style="padding:20px;text-align:center;color:var(--text-tertiary);">Reading List Empty</div>';

    // History
    const historyEl = document.getElementById('history-items-list');
    historyEl.innerHTML = this.history.map(h => `
      <div class="sidebar-item" data-url="${h.url}">
        <span class="sidebar-item-title">${h.title}</span>
        <span style="font-size:11px;color:var(--text-tertiary);">${h.time}</span>
      </div>
    `).join('') || '<div class="empty-state" style="padding:20px;text-align:center;color:var(--text-tertiary);">No History</div>';

    // Attach click to navigate
    document.querySelectorAll('.sidebar-item').forEach(item => {
      item.onclick = (e) => {
        if (!e.target.matches('button')) {
          const url = item.getAttribute('data-url');
          const tab = this.getActiveTab();
          if (tab && url) {
            tab.history.push(url);
            tab.historyIndex = tab.history.length - 1;
            this.loadUrlInTab(tab, url);
          }
        }
      };
    });
  }

  deleteBookmark(idx) {
    this.bookmarks.splice(idx, 1);
    this.saveBookmarks();
  }

  deleteReadingListItem(idx) {
    this.readingList.splice(idx, 1);
    this.saveReadingList();
  }

  filterSidebar(term) {
    document.querySelectorAll('.sidebar-item').forEach(item => {
      const text = item.textContent.toLowerCase();
      item.style.display = text.includes(term) ? 'flex' : 'none';
    });
  }

  /* ------------------------------------------------------------------------
     SETTINGS MODAL
     ------------------------------------------------------------------------ */
  openSettingsModal() {
    document.getElementById('setting-theme').value = this.settings.theme;
    document.getElementById('setting-search-engine').value = this.settings.searchEngine;
    document.getElementById('setting-homepage').value = this.settings.homepage;
    document.getElementById('setting-default-zoom').value = this.settings.defaultZoom || 100;
    this.el.settingsModal.classList.remove('hidden');
  }

  closeSettingsModal() {
    this.el.settingsModal.classList.add('hidden');
  }

  /* ------------------------------------------------------------------------
     UTILITY METHODS
     ------------------------------------------------------------------------ */
  extractDomain(url) {
    try {
      if (!url || url.startsWith('safari://')) return '';
      const parsed = new URL(url);
      return parsed.hostname.replace(/^www\./, '');
    } catch { return url; }
  }

  calculateHue(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) hash = str.charCodeAt(i) + ((hash << 5) - hash);
    return Math.abs(hash % 360);
  }
}

// Global browser instance
window.safari = new SafariBrowser();
