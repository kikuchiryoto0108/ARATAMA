//==============================================================================
//
//  Googleスプレッドシートからランキングデータを読み込む [ranking_loader.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/2/23
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef RANKING_LOADER_H
#define RANKING_LOADER_H

#include <vector>
#include <string>

// ランキング1件分のデータ
struct RankingEntry {
    double time;
};

class RankingLoader {
private:
    std::vector<RankingEntry> m_entries;    // 読み込んだランキングデータ
    bool m_loaded = false;                  // 読み込み完了フラグ
    bool m_loading = false;                 // 読み込み中フラグ
    bool m_submitted = false;               // 送信済みフラグ（二重送信防止）
    bool m_isChallenge = false;             // チャレンジモードフラグ

    RankingLoader() = default;

public:
    static RankingLoader& GetInstance() {
        static RankingLoader instance;
        return instance;
    }

    RankingLoader(const RankingLoader&) = delete;
    RankingLoader& operator=(const RankingLoader&) = delete;

    // チャレンジモード設定
    void SetChallengeMode(bool isChallenge) { m_isChallenge = isChallenge; }
    bool IsChallengeMode() const { return m_isChallenge; }

    // GAS経由でスプレッドシートからランキングを読み込む
    bool LoadFromGoogleSheets();

    // GAS経由でスプレッドシートにタイムを追記する
    bool SubmitTime(double timeSec);

    // タイム昇順でトップN件を取得
    std::vector<RankingEntry> GetTopEntries(int count) const;

    // ローカルにエントリを追加（GAS反映を待たずに表示用）
    void AddLocalEntry(double timeSec);

    // 状態取得
    bool IsLoaded() const { return m_loaded; }
    bool IsLoading() const { return m_loading; }
    bool IsSubmitted() const { return m_submitted; }

    // リセット（リザルト再表示時用）
    void Reset() { m_loaded = false; m_loading = false; m_submitted = false; m_entries.clear(); }
};

#endif // RANKING_LOADER_H
