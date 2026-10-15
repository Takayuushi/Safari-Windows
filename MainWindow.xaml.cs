using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using Microsoft.Web.WebView2.Core;

namespace SafariApp
{
    public class BrowserTab
    {
        public int Id { get; set; }
        public string Title { get; set; } = "Start Page";
        public string Url { get; set; } = "https://www.google.com";
    }

    public partial class MainWindow : Window
    {
        private readonly List<BrowserTab> _tabs = new();
        private BrowserTab? _activeTab;
        private int _nextTabId = 1;

        public MainWindow()
        {
            InitializeComponent();
            Loaded += MainWindow_Loaded;
        }

        private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                // Initialize WebView2 with persistent browser user data folder
                string dataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SafariData");
                var env = await CoreWebView2Environment.CreateAsync(null, dataFolder);
                await WebBrowser.EnsureCoreWebView2Async(env);

                // Enable modern web settings
                WebBrowser.CoreWebView2.Settings.IsStatusBarEnabled = false;
                WebBrowser.CoreWebView2.Settings.AreDevToolsEnabled = true;

                // Start with initial tab
                CreateNewTab("https://www.google.com");
            }
            catch (Exception ex)
            {
                MessageBox.Show($"WebView2 failed to initialize: {ex.Message}", "Safari Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        /* ------------------------------------------------------------------------
           TAB MANAGEMENT
           ------------------------------------------------------------------------ */
        private void CreateNewTab(string url)
        {
            var tab = new BrowserTab
            {
                Id = _nextTabId++,
                Url = url,
                Title = "Loading…"
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

            if (WebBrowser.CoreWebView2 != null && WebBrowser.Source?.ToString() != tab.Url)
            {
                WebBrowser.CoreWebView2.Navigate(tab.Url);
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
            // Like Safari 26: Tab row only visible when 2 or more tabs exist
            TabBarRow.Visibility = _tabs.Count > 1 ? Visibility.Visible : Visibility.Collapsed;
            TabsPanel.Children.Clear();

            foreach (var tab in _tabs)
            {
                bool isActive = tab == _activeTab;

                var border = new Border
                {
                    Background = new SolidColorBrush(isActive ? Color.FromRgb(48, 48, 54) : Color.FromArgb(0, 0, 0, 0)),
                    CornerRadius = new CornerRadius(8),
                    Height = 30,
                    MinWidth = 120,
                    MaxWidth = 220,
                    Margin = new Thickness(0, 0, 4, 0),
                    Padding = new Thickness(8, 0, 6, 0),
                    Cursor = Cursors.Hand
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                // Letter Badge / Favicon
                string letter = string.IsNullOrWhiteSpace(tab.Title) ? "S" : tab.Title.Substring(0, 1).ToUpperInvariant();
                var badge = new Border
                {
                    Width = 18,
                    Height = 18,
                    CornerRadius = new CornerRadius(4),
                    Background = new SolidColorBrush(Color.FromRgb(0, 122, 255)),
                    Margin = new Thickness(0, 0, 6, 0),
                    Child = new TextBlock
                    {
                        Text = letter,
                        Foreground = Brushes.White,
                        FontSize = 10,
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
                    Foreground = new SolidColorBrush(Color.FromRgb(245, 245, 247)),
                    FontSize = 12,
                    VerticalAlignment = VerticalAlignment.Center,
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                Grid.SetColumn(titleText, 1);
                grid.Children.Add(titleText);

                // Close Button
                var btnClose = new Button
                {
                    Content = "×",
                    Foreground = new SolidColorBrush(Color.FromRgb(161, 161, 166)),
                    Background = Brushes.Transparent,
                    BorderThickness = new Thickness(0),
                    Width = 16,
                    Height = 16,
                    FontSize = 12,
                    Cursor = Cursors.Hand
                };
                btnClose.Click += (s, ev) =>
                {
                    ev.Handled = true;
                    CloseTab(tab);
                };
                Grid.SetColumn(btnClose, 2);
                grid.Children.Add(btnClose);

                border.Child = grid;
                border.MouseDown += (s, ev) =>
                {
                    ActivateTab(tab);
                };

                TabsPanel.Children.Add(border);
            }
        }

        /* ------------------------------------------------------------------------
           NAVIGATION & SMART SEARCH
           ------------------------------------------------------------------------ */
        private void Navigate(string rawInput)
        {
            if (string.IsNullOrWhiteSpace(rawInput)) return;

            string targetUrl;
            string trimmed = rawInput.Trim();

            if (trimmed.StartsWith("http://", StringComparison.OrdinalIgnoreCase) || 
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
                // Query search engine (Google)
                targetUrl = "https://www.google.com/search?q=" + Uri.EscapeDataString(trimmed);
            }

            if (_activeTab != null)
            {
                _activeTab.Url = targetUrl;
            }

            if (WebBrowser.CoreWebView2 != null)
            {
                WebBrowser.CoreWebView2.Navigate(targetUrl);
            }
        }

        private string GetDisplayUrl(string fullUrl)
        {
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
                TxtAddress.Text = _activeTab.Url;
            TxtAddress.SelectAll();
        }

        private void TxtAddress_LostFocus(object sender, RoutedEventArgs e)
        {
            if (_activeTab != null)
                TxtAddress.Text = GetDisplayUrl(_activeTab.Url);
        }

        /* ------------------------------------------------------------------------
           WEBVIEW2 EVENTS
           ------------------------------------------------------------------------ */
        private void WebBrowser_NavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
        {
            if (_activeTab != null)
            {
                _activeTab.Url = e.Uri;
                TxtAddress.Text = GetDisplayUrl(e.Uri);
            }
            BtnBack.IsEnabled = WebBrowser.CanGoBack;
            BtnForward.IsEnabled = WebBrowser.CanGoForward;
        }

        private void WebBrowser_NavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
        {
            if (_activeTab != null && WebBrowser.CoreWebView2 != null)
            {
                _activeTab.Title = WebBrowser.CoreWebView2.DocumentTitle;
                _activeTab.Url = WebBrowser.Source.ToString();
                Title = $"{_activeTab.Title} — Safari";
                TxtAddress.Text = GetDisplayUrl(_activeTab.Url);
            }
            BtnBack.IsEnabled = WebBrowser.CanGoBack;
            BtnForward.IsEnabled = WebBrowser.CanGoForward;
            UpdateTabBar();
        }

        /* ------------------------------------------------------------------------
           BUTTON HANDLERS
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
            CreateNewTab("https://www.google.com");
        }

        private void BtnSidebar_Click(object sender, RoutedEventArgs e)
        {
            ColSidebar.Width = ColSidebar.Width.Value == 0 ? new GridLength(240) : new GridLength(0);
        }

        private void BtnDownloads_Click(object sender, RoutedEventArgs e)
        {
            CreateNewTab("edge://downloads");
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

        private void LstBookmarks_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (LstBookmarks.SelectedItem is ListBoxItem item && item.Tag is string url)
            {
                Navigate(url);
            }
        }
    }
}