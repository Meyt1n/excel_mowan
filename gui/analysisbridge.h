#pragma once

#include <functional>

#include <QString>
#include <QStringList>
#include <QVector>

using namespace std;


struct AnalysisTableSnapshot {
    QString sheet_name;
    QString range_reference;
    QStringList column_headers;
    QVector<QStringList> raw_values;
    QVector<QStringList> display_values;
    int selected_cell_count = 0;
};

struct PlotRequest {
    QString chart_type;
    QString title;
    AnalysisTableSnapshot selection;
};

struct AiAnalysisRequest {
    QString model_name;
    QString prompt;
    AnalysisTableSnapshot selection;
};

class AnalysisBridge {
public:
    using PlotHandler = function<QString(const PlotRequest&)>;
    using AiHandler = function<QString(const AiAnalysisRequest&)>;

    void SetPlotHandler(PlotHandler handler);
    void SetAiHandler(AiHandler handler);

    bool HasPlotHandler() const;
    bool HasAiHandler() const;

    QString GeneratePlot(const PlotRequest& request) const;
    QString RunAiAnalysis(const AiAnalysisRequest& request) const;

    QString DescribePlotCommand(const PlotRequest& request) const;
    QString DescribeAiCommand(const AiAnalysisRequest& request) const;

private:
    PlotHandler plot_handler_;
    AiHandler ai_handler_;
};
