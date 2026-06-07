//==============================================================================
//
//  Googleスプレッドシートからランキングデータを読み込む [ranking_loader.cpp]
//  Author : Ryoto Kikuchi
//  Date   : 2026/2/23
//------------------------------------------------------------------------------
//
//==============================================================================
#include "ranking_loader.h"
#include <Windows.h>
#include <WinInet.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <string>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

//======================================================================
// GAS WebアプリURL（読み書き両方これを使う）
// ?time=XX.XX  → 書き込み
// ?action=read → 読み込み
//======================================================================
// 通常モード用URL
static const char* GAS_URL_NORMAL =
"https://script.google.com/macros/s/AKfycbwl1ob99I5NdpvV2sNFmdmYJ6XvmosPLw7PJHRnUtDcR7VgeD8XTylG3-at6Agdva0N8w/exec";

// チャレンジモード用URL
static const char* GAS_URL_CHALLENGE =
"https://script.google.com/macros/s/AKfycbxXgNGnASbfAgPJIe7PihzQyUhQWVwZpr_LrgX2ogLxSFp0htkXT6_auDZiAxN9fSi7/exec";

// 現在のモードに応じたURLを取得するヘルパー関数
static const char* GetCurrentGasUrl() {
    return RankingLoader::GetInstance().IsChallengeMode() ? GAS_URL_CHALLENGE : GAS_URL_NORMAL;
}

//======================================================================
// HttpGet WinInet で HTTPS GET（手動リダイレクト追跡）
//
// GASは script.google.com → script.googleusercontent.com へ
// 302リダイレクトするため、自動リダイレクトを無効にして
// Location ヘッダーを手動で追跡する。
//======================================================================
static bool HttpGet(const std::string& url, std::string& outResponse) {
    outResponse.clear();

    std::string currentUrl = url;
    const int MAX_REDIRECTS = 10;

    for (int redirect = 0; redirect < MAX_REDIRECTS; ++redirect) {

        // URL を UTF-8 → ワイド文字に変換
        int wlen = MultiByteToWideChar(CP_UTF8, 0, currentUrl.c_str(), -1, nullptr, 0);
        std::wstring wurl(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, currentUrl.c_str(), -1, &wurl[0], wlen);

        // セッション作成
        HINTERNET hSession = InternetOpenW(     // ブラウザ開く
            L"GameRanking/1.0",
            INTERNET_OPEN_TYPE_PRECONFIG,
            NULL, NULL, 0);
        if (!hSession) return false;

        // URLを開く（自動リダイレクト無効）
        HINTERNET hUrl = InternetOpenUrlW(
            hSession,
            wurl.c_str(),
            NULL, 0,
            INTERNET_FLAG_RELOAD |
            INTERNET_FLAG_NO_CACHE_WRITE |
            INTERNET_FLAG_SECURE |
            INTERNET_FLAG_NO_AUTO_REDIRECT,
            0);
        if (!hUrl) {
            InternetCloseHandle(hSession);
            return false;
        }

        // ステータスコード取得（200, 302, 404など）
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
            &statusCode, &statusSize, NULL);

#ifdef _DEBUG
        OutputDebugStringA(("HttpGet: Status " + std::to_string(statusCode) + "\n").c_str());
#endif

        //--- リダイレクト (301, 302, 303, 307, 308) ---
        if (statusCode >= 300 && statusCode < 400) {
            wchar_t locationBuf[2048] = {};
            DWORD locationSize = sizeof(locationBuf);

            // Location ヘッダーから新しいURLを取得
            if (HttpQueryInfoW(hUrl, HTTP_QUERY_LOCATION, locationBuf, &locationSize, NULL)) {
                // ワイド文字 → UTF-8 に戻す
                int mbLen = WideCharToMultiByte(CP_UTF8, 0, locationBuf, -1, nullptr, 0, nullptr, nullptr);
                std::string newUrl(mbLen, '\0');
                WideCharToMultiByte(CP_UTF8, 0, locationBuf, -1, &newUrl[0], mbLen, nullptr, nullptr);
                if (!newUrl.empty() && newUrl.back() == '\0') newUrl.pop_back();

                currentUrl = newUrl;  // 新しいURLで再試行
            }
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hSession);
            continue;   // リダイレクト先に再リクエスト
        }

        //--- 200 OK: レスポンス読み取り ---
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            outResponse += buffer;
            bytesRead = 0;
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hSession);
        return true;
    }

#ifdef _DEBUG
    OutputDebugStringA("HttpGet: Too many redirects\n");
#endif
    return false;
}

//======================================================================
// LoadFromGoogleSheets GAS経由でランキングを読み込む
//
// GAS に ?action=read を送ると、スプレッドシートの全タイムを
// 改行区切りのテキストで返してくれる。
//======================================================================
bool RankingLoader::LoadFromGoogleSheets() {
    m_loading = true;
    m_loaded = false;
    m_entries.clear();

    std::string url = std::string(GetCurrentGasUrl()) + "?action=read";
    std::string response;

    // 最大3回リトライ
    bool success = false;
    for (int retry = 0; retry < 3; ++retry) {
        if (HttpGet(url, response)) {
            success = true;
            break;
        }
#ifdef _DEBUG
        OutputDebugStringA(("RankingLoader: Read retry " + std::to_string(retry + 1) + "\n").c_str());
#endif
        Sleep(500);
    }

    if (!success || response.empty()) {
#ifdef _DEBUG
        OutputDebugStringA("RankingLoader: Read FAILED\n");
#endif
        m_loading = false;
        return false;
    }

#ifdef _DEBUG
    OutputDebugStringA(("RankingLoader: Raw response:\n" + response + "\n").c_str());
#endif

    // レスポンスをパース（1行に1タイム）
    std::stringstream ss(response);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();   // \r 除去
        if (line.empty()) continue;

        try {
            double t = std::stod(line);
            if (t > 0.0) {
                m_entries.push_back({ t });
            }
        } catch (...) {}
    }

    // タイム昇順ソート（短い＝速い方が上位）
    if (!m_entries.empty()) {
        std::sort(m_entries.begin(), m_entries.end(),
            [](const RankingEntry& a, const RankingEntry& b) {
                return a.time < b.time;
            });
    }

    m_loaded = true;
    m_loading = false;

#ifdef _DEBUG
    OutputDebugStringA(("RankingLoader: Loaded " + std::to_string(m_entries.size()) + " entries\n").c_str());
#endif
    return true;
}

//======================================================================
// SubmitTime GAS経由でスプレッドシートにタイムを追記する
//
// GAS に ?time=XX.XX を送ると、スプレッドシートに1行追加される。
//======================================================================
bool RankingLoader::SubmitTime(double timeSec) {
    if (m_submitted) return true;   // 二重送信防止

    std::string url = std::string(GetCurrentGasUrl()) + "?time=" + std::to_string(timeSec);

#ifdef _DEBUG
    OutputDebugStringA(("RankingLoader: Submitting to " + url + "\n").c_str());
#endif

    std::string response;
    bool success = HttpGet(url, response);

    if (success) {
        m_submitted = true;
#ifdef _DEBUG
        OutputDebugStringA(("RankingLoader: Submit OK: " + response + "\n").c_str());
#endif
        return true;
    }

#ifdef _DEBUG
    OutputDebugStringA("RankingLoader: Submit FAILED\n");
#endif
    return false;
}

//======================================================================
// AddLocalEntry ローカルにエントリを追加
//
// 送信後にGASの反映を待たずに、自分のタイムを表示するために使う。
//======================================================================
void RankingLoader::AddLocalEntry(double timeSec) {
    if (timeSec > 0.0) {
        m_entries.push_back({ timeSec });

        // 再ソート
        std::sort(m_entries.begin(), m_entries.end(),
            [](const RankingEntry& a, const RankingEntry& b) {
                return a.time < b.time;
            });
    }
}

//======================================================================
// GetTopEntries 上位N件を取得
//======================================================================
std::vector<RankingEntry> RankingLoader::GetTopEntries(int count) const {
    std::vector<RankingEntry> top;
    int n = (std::min)(count, static_cast<int>(m_entries.size()));
    for (int i = 0; i < n; ++i) {
        top.push_back(m_entries[i]);
    }
    return top;
}