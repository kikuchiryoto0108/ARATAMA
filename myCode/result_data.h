//==============================================================================
//
//  リザルトデータ管理 [result_data.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/2/23
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef RESULT_DATA_H
#define RESULT_DATA_H

class ResultData {
private:
    double m_clearTime = 0.0;   // クリアタイム（秒）
    bool   m_hasData = false;   // データがセットされたか

    ResultData() = default;

public:
    static ResultData& GetInstance() {
        static ResultData instance;
        return instance;
    }

    ResultData(const ResultData&) = delete;
    ResultData& operator=(const ResultData&) = delete;

    // セッター / ゲッター
    void SetClearTime(double time) { m_clearTime = time; m_hasData = true; }
    double GetClearTime() const { return m_clearTime; }
    bool HasData() const { return m_hasData; }

    // リセット（再プレイ時用）
    void Reset() { m_clearTime = 0.0; m_hasData = false; }
};

#endif // RESULT_DATA_H