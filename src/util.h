#pragma once

#include <windows.h>
#include <string>

namespace safari {

std::string toUtf8(const std::wstring& wide);
std::wstring toUtf16(const std::string& utf8);

int dipToPx(int dip, UINT dpi);
int pxToDip(int px, UINT dpi);
RECT scaleRect(const RECT& rect, UINT dpi);

std::wstring normalizeInput(const std::wstring& input);
std::wstring extractDomain(const std::wstring& url);
bool isWebUrl(const std::wstring& input);

std::wstring getDownloadsFolder();
void revealFileInExplorer(const std::wstring& filePath);

void initGdiplus();
void shutdownGdiplus();

} // namespace safari
