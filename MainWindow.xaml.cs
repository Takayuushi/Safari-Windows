using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using Microsoft.Web.WebView2.Core;

namespace SafariApp
{
    public class BrowserTab
    {
        public int Id { get; set; }
        public string Title { get; set; } = "Start Page";
        public string Url { get; set; } = "safari://start";
        public string Domain { get; set; } = "Start Page";
        public bool IsLoading { get; set; }
    }

    public class HistoryRecord
    {
        public string Title { get; set; } = "";
        public string Url { get; set; } = "";
        public string Domain { get; set; } = "";
        public DateTime VisitedAt { get; set; } = DateTime.Now;
    }

    public class BookmarkRecord
    {
        public string Title { get; set; } = "";
        public string Url { get; set; } = "";
        public string Domain { get; set; } = "";
        public string Letter { get; set; } = "S";
        public Color Color { get; set; } = Color.FromRgb(0, 122, 255);
    }

    public class ReadingListRecord
    {
        public string Title { get; set; } = "";
        public string Url { get; set; } = "";
        public string Domain { get; set; } = "";
        public string Excerpt { get; set; } = "";
        public DateTime AddedAt { get; set; } = DateTime.Now;
    }

    public class DownloadEntry
    {
        public string FileName { get; set; } = "";
        public string FilePath { get; set; } = "";
        public long TotalBytes { get; set; }
        public long ReceivedBytes { get; set; }
        public string State { get; set; } = "InProgress";
        public ProgressBar? ProgressBarControl { get; set; }
        public TextBlock? StatusTextControl { get; set; }
    }

    public partial class MainWindow : Window
    {
        private readonly List<BrowserTab> _tabs = new();
        private BrowserTab? _activeTab;
        private int _nextTabId = 1;

        private readonly List<HistoryRecord> _history = new();
        private readonly List<BookmarkRecord> _bookmarks = new();
        private readonly List<ReadingListRecord> _readingList = new();
        private readonly List<DownloadEntry> _downloads = new();

        private string _activeSidebarSegment = "Bookmarks";

        public MainWindow()
        {
            InitializeComponent();
            InitData();
            Loaded += MainWindow_Loaded;
        }

        private void InitData()
        {
            // Seed authentic Safari Bookmarks
            _bookmarks.Add(new BookmarkRecord { Title = "Apple", Url = "https://www.apple.com", Domain = "apple.com", Letter = "A", Color = Color.FromRgb(51, 153, 255) });
            _bookmarks.Add(new BookmarkRecord { Title = "Google", Url = "https://www.google.com", Domain = "google.com", Letter = "G", Color = Color.FromRgb(234, 67, 53) });
            _bookmarks.Add(new BookmarkRecord { Title = "YouTube", Url = "https://www.youtube.com", Domain = "youtube.com", Letter = "Y", Color = Color.FromRgb(255, 0, 0) });
            _bookmarks.Add(new BookmarkRecord { Title = "GitHub", Url = "https://github.com", Domain = "github.com", Letter = "G", Color = Color.FromRgb(90, 60, 160) });
            _bookmarks.Add(new BookmarkRecord { Title = "Wikipedia", Url = "https://www.wikipedia.org", Domain = "wikipedia.org", Letter = "W", Color = Color.FromRgb(99, 110, 114) });
            _bookmarks.Add(new BookmarkRecord { Title = "Reddit", Url = "https://www.reddit.com", Domain = "reddit.com", Letter = "R", Color = Color.FromRgb(255, 69, 0) });
            _bookmarks.Add(new BookmarkRecord { Title = "LinkedIn", Url = "https://www.linkedin.com", Domain = "linkedin.com", Letter = "L", Color = Color.FromRgb(0, 119, 181) });
            _bookmarks.Add(new BookmarkRecord { Title = "Netflix", Url = "https://www.netflix.com", Domain = "netflix.com", Letter = "N", Color = Color.FromRgb(229, 9, 20) });

            // Seed Reading List
            _readingList.Add(new ReadingListRecord { Title = "WebKit — Open Source Web Browser Engine", Url = "https://webkit.org", Domain = "webkit.org", Excerpt = "WebKit is the browser engine used by Safari, Mail, App Store, and many other macOS, iOS, and Linux apps." });
            _readingList.Add(new ReadingListRecord { Title = "Safari Design and User Experience", Url = "https://developer.apple.com/design/human-interface-guidelines/safari", Domain = "developer.apple.com", Excerpt = "Learn how to build responsive, fluid web experiences following Apple's human interface guidelines." });
        }

        private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                string dataFolder = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SafariData");
                var env = await CoreWebView2Environment.CreateAsync(null, dataFolder);
                await WebBrowser.EnsureCoreWebView2Async(env);

                WebBrowser.CoreWebView2.Settings.IsStatusBarEnabled = false;
                WebBrowser.CoreWebView2.Settings.AreDevToolsEnabled = true;

                // Hook downloads for the native popover mini panel
                WebBrowser.CoreWebView2.DownloadStarting += CoreWebView2_DownloadStarting;

                // Start with initial tab pointing to Safari Start Page
                CreateNewTab("safari://start");
                RefreshSidebarList();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Engine initialization failed: {ex.Message}", "Safari Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        /* ------------------------------------------------------------------------
           START PAGE HTML GENERATOR
           ------------------------------------------------------------------------ */
        private string GetStartPageHtml()
        {
            return @"<!doctype html>
<html>
<head>
<meta charset=""utf-8"">
<meta name=""viewport"" content=""width=device-width,initial-scale=1"">
<title>Start Page</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;min-height:100%;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI Variable Text','Segoe UI',sans-serif;color:#f5f5f7}
  body{
    background:
      radial-gradient(1100px 640px at 18% -8%,rgba(76,201,176,.2),transparent 62%),
      radial-gradient(900px 560px at 92% 0%,rgba(110,120,230,.18),transparent 58%),
      linear-gradient(180deg,#242428,#161619);
    background-attachment:fixed;
    display:flex;flex-direction:column;align-items:center;padding:54px 24px;
  }
  .container{max-width:820px;width:100%}
  h2{font-size:18px;font-weight:700;letter-spacing:-.01em;margin:0 0 22px;color:#f5f5f7}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(92px,1fr));gap:22px 14px}
  .fav{display:flex;flex-direction:column;align-items:center;gap:9px;text-decoration:none;color:#f5f5f7;transition:transform .15s ease}
  .fav:hover{transform:translateY(-2px)}
  .icon{width:64px;height:64px;border-radius:15px;display:grid;place-items:center;color:#fff;font-size:26px;font-weight:600;
        box-shadow:0 3px 10px rgba(0,0,0,.35),inset 0 1px 1px rgba(255,255,255,.2)}
  .label{max-width:92px;text-align:center;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;color:#e1e1e6}
  .privacy{margin-top:46px;background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.08);border-radius:12px;padding:16px 20px;display:flex;align-items:center;gap:14px}
  .shield{width:28px;height:28px;fill:#2997ff}
  .priv-title{font-size:13px;font-weight:600;margin:0 0 2px}
  .priv-sub{font-size:12px;color:#8e8e93;margin:0}
</style>
</head>
<body>
<div class=""container"">
  <h2>Favorites</h2>
  <div class=""grid"">
    <a class=""fav"" href=""https://www.apple.com""><div class=""icon"" style=""background:linear-gradient(135deg,#3a88e9,#1d63bf)"">A</div><div class=""label"">Apple</div></a>
    <a class=""fav"" href=""https://www.google.com""><div class=""icon"" style=""background:linear-gradient(135deg,#ea4335,#c5221f)"">G</div><div class=""label"">Google</div></a>
    <a class=""fav"" href=""https://www.youtube.com""><div class=""icon"" style=""background:linear-gradient(135deg,#ff0000,#cc0000)"">Y</div><div class=""label"">YouTube</div></a>
    <a class=""fav"" href=""https://github.com""><div class=""icon"" style=""background:linear-gradient(135deg,#6e5494,#403259)"">G</div><div class=""label"">GitHub</div></a>
    <a class=""fav"" href=""https://www.wikipedia.org""><div class=""icon"" style=""background:linear-gradient(135deg,#636e72,#2d3436)"">W</div><div class=""label"">Wikipedia</div></a>
    <a class=""fav"" href=""https://www.reddit.com""><div class=""icon"" style=""background:linear-gradient(135deg,#ff4500,#cc3700)"">R</div><div class=""label"">Reddit</div></a>
    <a class=""fav"" href=""https://www.linkedin.com""><div class=""icon"" style=""background:linear-gradient(135deg,#0077b5,#005582)"">L</div><div class=""label"">LinkedIn</div></a>
    <a class=""fav"" href=""https://www.netflix.com""><div class=""icon"" style=""background:linear-gradient(135deg,#e50914,#b20710)"">N</div><div class=""label"">Netflix</div></a>
  </div>

  <div class=""privacy"">
    <svg class=""shield"" viewBox=""0 0 24 24""><path d=""M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-1 6h2v2h-2V7zm0 4h2v6h-2v-6z""/></svg>
    <div>
      <div class=""priv-title"">Privacy Report</div>
      <div class=""priv-sub"">Safari Intelligent Tracking Prevention is actively protecting your web privacy.</div>
    </div>
  </div>
</div>
</body>
</html>";
        }

        /* ------------------------------------------------------------------------
           TAB MANAGEMENT (AUTHENTIC SAFARI TAB BAR)
           ------------------------------------------------------------------------ */
        private void CreateNewTab(string url)
        {
            var tab = new BrowserTab
            {
                Id = _nextTabId++,
                Url = url,
                Title = (url == "safari://start" || url == "about:blank") ? "Start Page" : "Loading…"
            };

            _tabs.Add(tab);
            ActivateTab(tab);
            UpdateTabBar();
        }

        private void ActivateTab(BrowserTab tab)
        {
            _activeTab = tab;
            TxtAddress.Text = GetDisplayUrl(tab.Url);
            Title = $"{tab.Title} — Safari";

            if (WebBrowser.CoreWebView2 != null)
            {
                if (tab.Url == "safari://start" || tab.Url == "about:blank")
                {
                    WebBrowser.NavigateToString(GetStartPageHtml());
                }
                else if (WebBrowser.Source?.ToString() != tab.Url)
                {
                    WebBrowser.CoreWebView2.Navigate(tab.Url);
                }
            }

            UpdateTabBar();
        }

        private void CloseTab(BrowserTab tab)
        {
            int index = _tabs.IndexOf(tab);
            if (index == -1) return;

            _tabs.RemoveAt(index);

            if (_tabs.Count == 0)
            {
                Close();
                return;
            }

            int nextIndex = Math.Max(0, index - 1);
            ActivateTab(_tabs[nextIndex]);
            UpdateTabBar();
        }

        private void UpdateTabBar()
        {
            TabsPanel.Children.Clear();

            foreach (var tab in _tabs)
            {
                bool isActive = tab == _activeTab;

                // Authentic macOS Safari Tab Card
                var tabBorder = new Border
                {
                    Background = new SolidColorBrush(isActive ? Color.FromRgb(52, 52, 58) : Color.FromArgb(0, 0, 0, 0)),
                    BorderBrush = new SolidColorBrush(isActive ? Color.FromRgb(68, 68, 76) : Color.FromArgb(40, 255, 255, 255)),
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(7),
                    Height = 31,
                    MinWidth = 140,
                    MaxWidth = 230,
                    Margin = new Thickness(0, 0, 5, 0),
                    Padding = new Thickness(10, 0, 6, 0),
                    Cursor = Cursors.Hand
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                // Site icon / Letter badge
                string letter = (tab.Title == "Start Page" || string.IsNullOrWhiteSpace(tab.Title)) ? "S" : tab.Title.Substring(0, 1).ToUpperInvariant();
                var badge = new Border
                {
                    Width = 16,
                    Height = 16,
                    CornerRadius = new CornerRadius(4),
                    Background = new SolidColorBrush(isActive ? Color.FromRgb(0, 122, 255) : Color.FromRgb(85, 85, 92)),
                    Margin = new Thickness(0, 0, 7, 0),
                    VerticalAlignment = VerticalAlignment.Center,
                    Child = new TextBlock
                    {
                        Text = letter,
                        Foreground = Brushes.White,
                        FontSize = 9.5,
                        FontWeight = FontWeights.Bold,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center
                    }
                };
                Grid.SetColumn(badge, 0);
                grid.Children.Add(badge);

                // Tab Title
                var titleText = new TextBlock
                {
                    Text = tab.Title,
                    Foreground = new SolidColorBrush(isActive ? Color.FromRgb(245, 245, 247) : Color.FromRgb(161, 161, 166)),
                    FontWeight = isActive ? FontWeights.Medium : FontWeights.Normal,
                    FontSize = 12,
                    VerticalAlignment = VerticalAlignment.Center,
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                Grid.SetColumn(titleText, 1);
                grid.Children.Add(titleText);

                // Close Button (macOS circular hover effect)
                var btnClose = new Button
                {
                    Width = 16,
                    Height = 16,
                    Background = Brushes.Transparent,
                    BorderThickness = new Thickness(0),
                    Cursor = Cursors.Hand,
                    ToolTip = "Close Tab",
                    Margin = new Thickness(4, 0, 0, 0),
                    Template = CreateCloseButtonTemplate()
                };
                btnClose.Click += (s, ev) =>
                {
                    ev.Handled = true;
                    CloseTab(tab);
                };
                Grid.SetColumn(btnClose, 2);
                grid.Children.Add(btnClose);

                tabBorder.Child = grid;

                // Mouse events for tab switching and hover effect
                tabBorder.MouseEnter += (s, ev) =>
                {
                    if (tab != _activeTab)
                        tabBorder.Background = new SolidColorBrush(Color.FromArgb(40, 255, 255, 255));
                };
                tabBorder.MouseLeave += (s, ev) =>
                {
                    if (tab != _activeTab)
                        tabBorder.Background = new SolidColorBrush(Color.FromArgb(0, 0, 0, 0));
                };
                tabBorder.MouseDown += (s, ev) =>
                {
                    ActivateTab(tab);
                };

                TabsPanel.Children.Add(tabBorder);
            }
        }

        private ControlTemplate CreateCloseButtonTemplate()
        {
            var template = new ControlTemplate(typeof(Button));
            var borderFactory = new FrameworkElementFactory(typeof(Border));
            borderFactory.Name = "closeCircle";
            borderFactory.SetValue(Border.CornerRadiusProperty, new CornerRadius(8));
            borderFactory.SetValue(Border.BackgroundProperty, Brushes.Transparent);

            var textFactory = new FrameworkElementFactory(typeof(TextBlock));
            textFactory.SetValue(TextBlock.TextProperty, "×");
            textFactory.SetValue(TextBlock.ForegroundProperty, new SolidColorBrush(Color.FromRgb(161, 161, 166)));
            textFactory.SetValue(TextBlock.FontSizeProperty, 12.0);
            textFactory.SetValue(TextBlock.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            textFactory.SetValue(TextBlock.VerticalAlignmentProperty, VerticalAlignment.Center);
            textFactory.SetValue(TextBlock.MarginProperty, new Thickness(0, -2, 0, 0));

            borderFactory.AppendChild(textFactory);
            template.VisualTree = borderFactory;

            var trigger = new Trigger { Property = UIElement.IsMouseOverProperty, Value = true };
            trigger.Setters.Add(new Setter(Border.BackgroundProperty, new SolidColorBrush(Color.FromRgb(72, 72, 80)), "closeCircle"));
            template.Triggers.Add(trigger);

            return template;
        }

        /* ------------------------------------------------------------------------
           NAVIGATION & SMART SEARCH
           ------------------------------------------------------------------------ */
        private void Navigate(string rawInput)
        {
            if (string.IsNullOrWhiteSpace(rawInput)) return;

            string targetUrl;
            string trimmed = rawInput.Trim();

            if (trimmed.Equals("safari://start", StringComparison.OrdinalIgnoreCase) || 
                trimmed.Equals("about:blank", StringComparison.OrdinalIgnoreCase) ||
                trimmed.Equals("Start Page", StringComparison.OrdinalIgnoreCase))
            {
                targetUrl = "safari://start";
            }
            else if (trimmed.StartsWith("http://", StringComparison.OrdinalIgnoreCase) || 
                     trimmed.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
            {
                targetUrl = trimmed;
            }
            else if (trimmed.Contains('.') && !trimmed.Contains(' ') || trimmed.StartsWith("localhost", StringComparison.OrdinalIgnoreCase))
            {
                targetUrl = "https://" + trimmed;
            }
            else
            {
                // Default search engine (Google)
                targetUrl = "https://www.google.com/search?q=" + Uri.EscapeDataString(trimmed);
            }

            if (_activeTab != null)
            {
                _activeTab.Url = targetUrl;
            }

            if (WebBrowser.CoreWebView2 != null)
            {
                if (targetUrl == "safari://start")
                {
                    WebBrowser.NavigateToString(GetStartPageHtml());
                    if (_activeTab != null)
                    {
                        _activeTab.Title = "Start Page";
                        TxtAddress.Text = "Start Page";
                        Title = "Start Page — Safari";
                    }
                    UpdateTabBar();
                }
                else
                {
                    WebBrowser.CoreWebView2.Navigate(targetUrl);
                }
            }
        }

        private string GetDisplayUrl(string fullUrl)
        {
            if (fullUrl == "safari://start" || fullUrl == "about:blank" || string.IsNullOrWhiteSpace(fullUrl))
                return "Start Page";

            try
            {
                var uri = new Uri(fullUrl);
                string host = uri.Host;
                if (host.StartsWith("www.", StringComparison.OrdinalIgnoreCase))
                    host = host.Substring(4);
                return host;
            }
            catch
            {
                return fullUrl;
            }
        }

        private void TxtAddress_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                Navigate(TxtAddress.Text);
                WebBrowser.Focus();
            }
            else if (e.Key == Key.Escape)
            {
                if (_activeTab != null)
                    TxtAddress.Text = GetDisplayUrl(_activeTab.Url);
                WebBrowser.Focus();
            }
        }

        private void TxtAddress_GotFocus(object sender, RoutedEventArgs e)
        {
            if (_activeTab != null)
                TxtAddress.Text = _activeTab.Url == "safari://start" ? "" : _activeTab.Url;
            TxtAddress.SelectAll();
        }

        private void TxtAddress_LostFocus(object sender, RoutedEventArgs e)
        {
            if (_activeTab != null)
                TxtAddress.Text = GetDisplayUrl(_activeTab.Url);
        }

        /* ------------------------------------------------------------------------
           WEBVIEW EVENTS & HISTORY RECORDING
           ------------------------------------------------------------------------ */
        private void WebBrowser_NavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
        {
            if (_activeTab != null)
            {
                _activeTab.Url = e.Uri;
                TxtAddress.Text = GetDisplayUrl(e.Uri);
                _activeTab.IsLoading = true;
            }
            BtnBack.IsEnabled = WebBrowser.CanGoBack;
            BtnForward.IsEnabled = WebBrowser.CanGoForward;
        }

        private void WebBrowser_NavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
        {
            if (_activeTab != null && WebBrowser.CoreWebView2 != null)
            {
                string title = WebBrowser.CoreWebView2.DocumentTitle;
                string url = WebBrowser.Source.ToString();

                if (!string.IsNullOrWhiteSpace(title))
                {
                    _activeTab.Title = title;
                }
                _activeTab.Url = url;
                _activeTab.IsLoading = false;
                Title = $"{_activeTab.Title} — Safari";
                TxtAddress.Text = GetDisplayUrl(url);

                // Record in History if it's a real web page
                if (!url.StartsWith("data:") && !url.StartsWith("safari:") && !url.StartsWith("about:"))
                {
                    var record = new HistoryRecord
                    {
                        Title = string.IsNullOrWhiteSpace(title) ? GetDisplayUrl(url) : title,
                        Url = url,
                        Domain = GetDisplayUrl(url),
                        VisitedAt = DateTime.Now
                    };
                    _history.Insert(0, record);

                    // Update sidebar if History tab is visible
                    if (_activeSidebarSegment == "History")
                    {
                        RefreshSidebarList();
                    }
                }
            }

            BtnBack.IsEnabled = WebBrowser.CanGoBack;
            BtnForward.IsEnabled = WebBrowser.CanGoForward;
            UpdateTabBar();
        }

        private void WebBrowser_SourceChanged(object? sender, CoreWebView2SourceChangedEventArgs e)
        {
            if (_activeTab != null && WebBrowser.CoreWebView2 != null)
            {
                _activeTab.Url = WebBrowser.Source.ToString();
                TxtAddress.Text = GetDisplayUrl(_activeTab.Url);
            }
        }

        /* ------------------------------------------------------------------------
           AUTHENTIC SAFARI 3-SEGMENT SIDEBAR
           ------------------------------------------------------------------------ */
        private void BtnSidebar_Click(object sender, RoutedEventArgs e)
        {
            ColSidebar.Width = ColSidebar.Width.Value == 0 ? new GridLength(270) : new GridLength(0);
        }

        private void BtnSegment_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is string tag)
            {
                _activeSidebarSegment = tag;

                // Update tab indicator backgrounds
                BtnSegmentBookmarks.Background = tag == "Bookmarks" ? new SolidColorBrush(Color.FromRgb(56, 56, 64)) : Brushes.Transparent;
                BtnSegmentReadingList.Background = tag == "ReadingList" ? new SolidColorBrush(Color.FromRgb(56, 56, 64)) : Brushes.Transparent;
                BtnSegmentHistory.Background = tag == "History" ? new SolidColorBrush(Color.FromRgb(56, 56, 64)) : Brushes.Transparent;

                // Update search placeholder
                TxtSidebarSearchPlaceholder.Text = $"Search {tag}";

                // Show/hide history action
                HistoryActionRow.Visibility = tag == "History" ? Visibility.Visible : Visibility.Collapsed;

                RefreshSidebarList();
            }
        }

        private void TxtSidebarSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            TxtSidebarSearchPlaceholder.Visibility = string.IsNullOrEmpty(TxtSidebarSearch.Text) ? Visibility.Visible : Visibility.Collapsed;
            RefreshSidebarList();
        }

        private void BtnClearHistory_Click(object sender, RoutedEventArgs e)
        {
            _history.Clear();
            RefreshSidebarList();
        }

        private void RefreshSidebarList()
        {
            SidebarItemsPanel.Children.Clear();
            string filter = TxtSidebarSearch.Text.Trim().ToLowerInvariant();

            if (_activeSidebarSegment == "Bookmarks")
            {
                var filtered = _bookmarks.Where(b => string.IsNullOrEmpty(filter) || b.Title.ToLowerInvariant().Contains(filter) || b.Domain.ToLowerInvariant().Contains(filter));
                foreach (var item in filtered)
                {
                    SidebarItemsPanel.Children.Add(CreateBookmarkRow(item));
                }
            }
            else if (_activeSidebarSegment == "ReadingList")
            {
                var filtered = _readingList.Where(r => string.IsNullOrEmpty(filter) || r.Title.ToLowerInvariant().Contains(filter) || r.Domain.ToLowerInvariant().Contains(filter) || r.Excerpt.ToLowerInvariant().Contains(filter));
                foreach (var item in filtered)
                {
                    SidebarItemsPanel.Children.Add(CreateReadingListRow(item));
                }
            }
            else if (_activeSidebarSegment == "History")
            {
                var filtered = _history.Where(h => string.IsNullOrEmpty(filter) || h.Title.ToLowerInvariant().Contains(filter) || h.Domain.ToLowerInvariant().Contains(filter));
                if (!_history.Any())
                {
                    var emptyText = new TextBlock
                    {
                        Text = "No browsing history",
                        Foreground = new SolidColorBrush(Color.FromRgb(142, 142, 147)),
                        FontSize = 12,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        Margin = new Thickness(0, 30, 0, 0)
                    };
                    SidebarItemsPanel.Children.Add(emptyText);
                }
                else
                {
                    foreach (var item in filtered)
                    {
                        SidebarItemsPanel.Children.Add(CreateHistoryRow(item));
                    }
                }
            }
        }

        private UIElement CreateBookmarkRow(BookmarkRecord item)
        {
            var border = new Border
            {
                Background = Brushes.Transparent,
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(8, 6, 8, 6),
                Margin = new Thickness(0, 0, 0, 2),
                Cursor = Cursors.Hand
            };

            var grid = new Grid();
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });

            // Icon squircle
            var iconBox = new Border
            {
                Width = 24,
                Height = 24,
                CornerRadius = new CornerRadius(5),
                Background = new SolidColorBrush(item.Color),
                Margin = new Thickness(0, 0, 10, 0),
                Child = new TextBlock
                {
                    Text = item.Letter,
                    Foreground = Brushes.White,
                    FontSize = 12,
                    FontWeight = FontWeights.Bold,
                    HorizontalAlignment = HorizontalAlignment.Center,
                    VerticalAlignment = VerticalAlignment.Center
                }
            };
            Grid.SetColumn(iconBox, 0);
            grid.Children.Add(iconBox);

            var textStack = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
            var title = new TextBlock { Text = item.Title, Foreground = new SolidColorBrush(Color.FromRgb(245, 245, 247)), FontSize = 12.5, FontWeight = FontWeights.Medium, TextTrimming = TextTrimming.CharacterEllipsis };
            var domain = new TextBlock { Text = item.Domain, Foreground = new SolidColorBrush(Color.FromRgb(142, 142, 147)), FontSize = 11, TextTrimming = TextTrimming.CharacterEllipsis };
            textStack.Children.Add(title);
            textStack.Children.Add(domain);
            Grid.SetColumn(textStack, 1);
            grid.Children.Add(textStack);

            border.Child = grid;
            border.MouseEnter += (s, e) => border.Background = new SolidColorBrush(Color.FromArgb(40, 255, 255, 255));
            border.MouseLeave += (s, e) => border.Background = Brushes.Transparent;
            border.MouseDown += (s, e) => Navigate(item.Url);

            return border;
        }

        private UIElement CreateReadingListRow(ReadingListRecord item)
        {
            var border = new Border
            {
                Background = Brushes.Transparent,
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(8, 8, 8, 8),
                Margin = new Thickness(0, 0, 0, 4),
                Cursor = Cursors.Hand
            };

            var stack = new StackPanel();
            var title = new TextBlock { Text = item.Title, Foreground = new SolidColorBrush(Color.FromRgb(245, 245, 247)), FontSize = 12.5, FontWeight = FontWeights.Medium, TextTrimming = TextTrimming.CharacterEllipsis };
            var domain = new TextBlock { Text = item.Domain, Foreground = new SolidColorBrush(Color.FromRgb(41, 151, 255)), FontSize = 11, Margin = new Thickness(0, 2, 0, 2) };
            var excerpt = new TextBlock { Text = item.Excerpt, Foreground = new SolidColorBrush(Color.FromRgb(142, 142, 147)), FontSize = 11.5, TextWrapping = TextWrapping.Wrap, MaxHeight = 34, TextTrimming = TextTrimming.CharacterEllipsis };

            stack.Children.Add(title);
            stack.Children.Add(domain);
            stack.Children.Add(excerpt);

            border.Child = stack;
            border.MouseEnter += (s, e) => border.Background = new SolidColorBrush(Color.FromArgb(40, 255, 255, 255));
            border.MouseLeave += (s, e) => border.Background = Brushes.Transparent;
            border.MouseDown += (s, e) => Navigate(item.Url);

            return border;
        }

        private UIElement CreateHistoryRow(HistoryRecord item)
        {
            // Exact Safari layout as seen in Pic 1: Domain/Title on left, timestamp on right
            var border = new Border
            {
                Background = Brushes.Transparent,
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(8, 6, 8, 6),
                Margin = new Thickness(0, 0, 0, 2),
                Cursor = Cursors.Hand
            };

            var grid = new Grid();
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var title = new TextBlock
            {
                Text = item.Domain,
                Foreground = new SolidColorBrush(Color.FromRgb(245, 245, 247)),
                FontSize = 12.5,
                VerticalAlignment = VerticalAlignment.Center,
                TextTrimming = TextTrimming.CharacterEllipsis
            };
            Grid.SetColumn(title, 0);
            grid.Children.Add(title);

            var time = new TextBlock
            {
                Text = item.VisitedAt.ToString("h:mm tt"),
                Foreground = new SolidColorBrush(Color.FromRgb(142, 142, 147)),
                FontSize = 11.5,
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(8, 0, 0, 0)
            };
            Grid.SetColumn(time, 1);
            grid.Children.Add(time);

            border.Child = grid;
            border.MouseEnter += (s, e) => border.Background = new SolidColorBrush(Color.FromArgb(40, 255, 255, 255));
            border.MouseLeave += (s, e) => border.Background = Brushes.Transparent;
            border.MouseDown += (s, e) => Navigate(item.Url);

            return border;
        }

        /* ------------------------------------------------------------------------
           DOWNLOADS MINI PANEL POPOVER (AUTHENTIC SAFARI EXPERIENCE)
           ------------------------------------------------------------------------ */
        private void BtnDownloads_Click(object sender, RoutedEventArgs e)
        {
            // Open/toggle the floating mini popover directly under the button!
            DownloadsPopup.IsOpen = !DownloadsPopup.IsOpen;
            RefreshDownloadsList();
        }

        private void CoreWebView2_DownloadStarting(object? sender, CoreWebView2DownloadStartingEventArgs e)
        {
            string fileName = System.IO.Path.GetFileName(e.ResultFilePath);
            var entry = new DownloadEntry
            {
                FileName = fileName,
                FilePath = e.ResultFilePath,
                TotalBytes = (long)(e.DownloadOperation.TotalBytesToReceive ?? 0),
                ReceivedBytes = 0,
                State = "InProgress"
            };

            _downloads.Insert(0, entry);
            DownloadBadge.Visibility = Visibility.Visible;
            DownloadsPopup.IsOpen = true;
            RefreshDownloadsList();

            e.DownloadOperation.BytesReceivedChanged += (s, args) =>
            {
                Dispatcher.Invoke(() =>
                {
                    entry.ReceivedBytes = (long)e.DownloadOperation.BytesReceived;
                    if (entry.ProgressBarControl != null && entry.TotalBytes > 0)
                    {
                        entry.ProgressBarControl.Value = (double)entry.ReceivedBytes / entry.TotalBytes * 100.0;
                    }
                    if (entry.StatusTextControl != null)
                    {
                        string mbRec = (entry.ReceivedBytes / 1048576.0).ToString("0.1");
                        string mbTot = entry.TotalBytes > 0 ? (entry.TotalBytes / 1048576.0).ToString("0.1") : "—";
                        entry.StatusTextControl.Text = $"{mbRec} MB of {mbTot} MB";
                    }
                });
            };

            e.DownloadOperation.StateChanged += (s, args) =>
            {
                Dispatcher.Invoke(() =>
                {
                    entry.State = e.DownloadOperation.State.ToString();
                    if (e.DownloadOperation.State == CoreWebView2DownloadState.Completed)
                    {
                        if (entry.ProgressBarControl != null) entry.ProgressBarControl.Visibility = Visibility.Collapsed;
                        if (entry.StatusTextControl != null)
                        {
                            string sizeStr = (e.DownloadOperation.BytesReceived / 1048576.0).ToString("0.1");
                            entry.StatusTextControl.Text = $"Completed • {sizeStr} MB";
                            entry.StatusTextControl.Foreground = new SolidColorBrush(Color.FromRgb(48, 209, 88));
                        }
                    }
                    else if (e.DownloadOperation.State == CoreWebView2DownloadState.Interrupted)
                    {
                        if (entry.StatusTextControl != null)
                        {
                            entry.StatusTextControl.Text = "Download interrupted";
                            entry.StatusTextControl.Foreground = new SolidColorBrush(Color.FromRgb(255, 69, 58));
                        }
                    }

                    // Turn off download badge if all completed
                    if (!_downloads.Any(d => d.State == "InProgress"))
                    {
                        DownloadBadge.Visibility = Visibility.Collapsed;
                    }
                });
            };
        }

        private void RefreshDownloadsList()
        {
            DownloadsListPanel.Children.Clear();

            if (_downloads.Count == 0)
            {
                DownloadsListPanel.Children.Add(TxtNoDownloads);
                return;
            }

            foreach (var item in _downloads)
            {
                var card = new Border
                {
                    Background = new SolidColorBrush(Color.FromRgb(46, 46, 52)),
                    CornerRadius = new CornerRadius(8),
                    Padding = new Thickness(10),
                    Margin = new Thickness(0, 0, 0, 8)
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                // Document / File icon
                var fileIconBorder = new Border
                {
                    Width = 32,
                    Height = 32,
                    CornerRadius = new CornerRadius(6),
                    Background = new SolidColorBrush(Color.FromRgb(60, 60, 68)),
                    Margin = new Thickness(0, 0, 10, 0),
                    VerticalAlignment = VerticalAlignment.Center,
                    Child = new System.Windows.Shapes.Path
                    {
                        Data = Geometry.Parse("M6,2 H14 L20,8 V22 H6 Z M14,2 V8 H20"),
                        Stroke = new SolidColorBrush(Color.FromRgb(161, 161, 166)),
                        StrokeThickness = 1.4,
                        Width = 14,
                        Height = 16,
                        Stretch = Stretch.Uniform,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center
                    }
                };
                Grid.SetColumn(fileIconBorder, 0);
                grid.Children.Add(fileIconBorder);

                // File info
                var infoStack = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
                var nameText = new TextBlock
                {
                    Text = item.FileName,
                    Foreground = new SolidColorBrush(Color.FromRgb(245, 245, 247)),
                    FontWeight = FontWeights.Medium,
                    FontSize = 12.5,
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                infoStack.Children.Add(nameText);

                // Progress Bar
                var pbar = new ProgressBar
                {
                    Height = 4,
                    Margin = new Thickness(0, 4, 0, 4),
                    Background = new SolidColorBrush(Color.FromRgb(30, 30, 34)),
                    Foreground = new SolidColorBrush(Color.FromRgb(0, 122, 255)),
                    BorderThickness = new Thickness(0),
                    Visibility = item.State == "InProgress" ? Visibility.Visible : Visibility.Collapsed
                };
                item.ProgressBarControl = pbar;
                infoStack.Children.Add(pbar);

                // Subtitle / Status
                var statusText = new TextBlock
                {
                    Text = item.State == "Completed" ? $"Completed • {(item.ReceivedBytes / 1048576.0):0.1} MB" : "Downloading…",
                    Foreground = item.State == "Completed" ? new SolidColorBrush(Color.FromRgb(48, 209, 88)) : new SolidColorBrush(Color.FromRgb(142, 142, 147)),
                    FontSize = 11
                };
                item.StatusTextControl = statusText;
                infoStack.Children.Add(statusText);

                Grid.SetColumn(infoStack, 1);
                grid.Children.Add(infoStack);

                // Action button: Show in Folder (Magnifier Icon)
                var btnShowInFolder = new Button
                {
                    Width = 26,
                    Height = 26,
                    Background = Brushes.Transparent,
                    BorderThickness = new Thickness(0),
                    Cursor = Cursors.Hand,
                    ToolTip = "Show in File Explorer",
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(6, 0, 0, 0),
                    Content = new System.Windows.Shapes.Path
                    {
                        Data = Geometry.Parse("M10,10 L14,14 M6,10 A4,4 0 1 1 10,6 A4,4 0 0 1 6,10 Z"),
                        Stroke = new SolidColorBrush(Color.FromRgb(161, 161, 166)),
                        StrokeThickness = 1.4,
                        Width = 12,
                        Height = 12,
                        Stretch = Stretch.Uniform
                    }
                };
                btnShowInFolder.Click += (s, e) =>
                {
                    try
                    {
                        if (File.Exists(item.FilePath))
                        {
                            Process.Start("explorer.exe", $"/select,\"{item.FilePath}\"");
                        }
                        else
                        {
                            string downloadsDir = System.IO.Path.GetDirectoryName(item.FilePath) ?? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                            Process.Start("explorer.exe", downloadsDir);
                        }
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Could not open file location: {ex.Message}", "Safari", MessageBoxButton.OK, MessageBoxImage.Warning);
                    }
                };
                Grid.SetColumn(btnShowInFolder, 2);
                grid.Children.Add(btnShowInFolder);

                card.Child = grid;
                DownloadsListPanel.Children.Add(card);
            }
        }

        private void BtnClearDownloads_Click(object sender, RoutedEventArgs e)
        {
            _downloads.RemoveAll(d => d.State != "InProgress");
            RefreshDownloadsList();
        }

        private void BtnOpenDownloadsFolder_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string downloadsPath = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
                Process.Start("explorer.exe", downloadsPath);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to open Downloads folder: {ex.Message}", "Safari", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        /* ------------------------------------------------------------------------
           TOOLBAR & TITLEBAR BUTTON HANDLERS
           ------------------------------------------------------------------------ */
        private void BtnClose_Click(object sender, RoutedEventArgs e) => Close();
        private void BtnMin_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
        private void BtnMax_Click(object sender, RoutedEventArgs e) => 
            WindowState = (WindowState == WindowState.Maximized) ? WindowState.Normal : WindowState.Maximized;

        private void BtnBack_Click(object sender, RoutedEventArgs e)
        {
            if (WebBrowser.CanGoBack) WebBrowser.GoBack();
        }

        private void BtnForward_Click(object sender, RoutedEventArgs e)
        {
            if (WebBrowser.CanGoForward) WebBrowser.GoForward();
        }

        private void BtnReload_Click(object sender, RoutedEventArgs e)
        {
            WebBrowser.Reload();
        }

        private void BtnNewTab_Click(object sender, RoutedEventArgs e)
        {
            CreateNewTab("safari://start");
        }

        private void BtnShare_Click(object sender, RoutedEventArgs e)
        {
            if (_activeTab != null)
            {
                Clipboard.SetText(_activeTab.Url);
                MessageBox.Show("Page URL copied to clipboard!", "Safari Share", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnOverview_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show($"Open Tabs ({_tabs.Count}):\n" + string.Join("\n", _tabs.Select(t => $"• {t.Title} ({t.Url})")), "Safari Tab Overview");
        }
    }
}