#include "analysisbridge.h"

#include <algorithm>

namespace {

QString MatrixPreview(const QVector<QStringList>& rows, int max_rows = 4, int max_cols = 4) {
    if (rows.isEmpty()) return QString::fromUtf8("当前没有可发送的数据。");

    QStringList lines;
    const int row_count = std::min(max_rows, static_cast<int>(rows.size()));
    for (int row = 0; row < row_count; ++row) {
        QStringList cells = rows.at(row);
        if (cells.size() > max_cols) {
            cells = cells.mid(0, max_cols);
            cells << "...";
        }
        lines << cells.join(" | ");
    }
    if (rows.size() > max_rows) {
        lines << "...";
    }
    return lines.join('\n');
}

QString SnapshotSummary(const AnalysisTableSnapshot& snapshot) {
    if (snapshot.selected_cell_count <= 0) {
        return QString::fromUtf8("未选择数据区域");
    }

    return QString::fromUtf8("范围: %1\n单元格: %2\n列头: %3")
        .arg(snapshot.range_reference)
        .arg(snapshot.selected_cell_count)
        .arg(snapshot.column_headers.join(", "));
}

}  // namespace

void AnalysisBridge::SetPlotHandler(PlotHandler handler) {
    plot_handler_ = std::move(handler);
}

void AnalysisBridge::SetAiHandler(AiHandler handler) {
    ai_handler_ = std::move(handler);
}

bool AnalysisBridge::HasPlotHandler() const {
    return static_cast<bool>(plot_handler_);
}

bool AnalysisBridge::HasAiHandler() const {
    return static_cast<bool>(ai_handler_);
}

QString AnalysisBridge::GeneratePlot(const PlotRequest& request) const {
    if (plot_handler_) {
        return plot_handler_(request);
    }

    return QString::fromUtf8(
               "Matplotlib 接口已预留，还没有接入真实 Python 实现。\n\n%1\n\n建议命令:\n%2\n\n数据预览:\n%3"
           )
        .arg(SnapshotSummary(request.selection))
        .arg(DescribePlotCommand(request))
        .arg(MatrixPreview(request.selection.display_values));
}

QString AnalysisBridge::RunAiAnalysis(const AiAnalysisRequest& request) const {
    if (ai_handler_) {
        return ai_handler_(request);
    }

    return QString::fromUtf8(
               "Ollama 接口已预留，还没有接入真实模型调用。\n\n%1\n\n建议命令:\n%2\n\n提示词:\n%3\n\n数据预览:\n%4"
           )
        .arg(SnapshotSummary(request.selection))
        .arg(DescribeAiCommand(request))
        .arg(request.prompt)
        .arg(MatrixPreview(request.selection.display_values));
}

QString AnalysisBridge::DescribePlotCommand(const PlotRequest& request) const {
    const QString chart_type = request.chart_type.isEmpty() ? QStringLiteral("line") : request.chart_type;
    return QString("python tools/plot_selection.py --range \"%1\" --chart %2 --title \"%3\"")
        .arg(request.selection.range_reference, chart_type, request.title);
}

QString AnalysisBridge::DescribeAiCommand(const AiAnalysisRequest& request) const {
    const QString model_name = request.model_name.isEmpty() ? QStringLiteral("qwen2.5:7b") : request.model_name;
    return QString("ollama run %1 \"请分析 %2 选区数据\"").arg(model_name, request.selection.range_reference);
}
