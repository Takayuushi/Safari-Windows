"""
Safari 26 Local Runner (Windows WebView2)
Launches the glassmorphic Safari UI directly on Windows.
"""
import os
import sys
import webview

class SafariAPI:
    def __init__(self, window):
        self.window = window

    def close(self):
        self.window.destroy()

    def minimize(self):
        self.window.minimize()

    def toggle_maximize(self):
        pass

def main():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    html_path = os.path.join(current_dir, "src", "index.html")

    window = webview.create_window(
        title="Safari",
        url=html_path,
        width=1280,
        height=840,
        min_size=(800, 500),
        frameless=True,
        easy_drag=True,
        background_color="#212124"
    )
    api = SafariAPI(window)
    window.js_api = api

    webview.start(debug=False)

if __name__ == "__main__":
    main()
