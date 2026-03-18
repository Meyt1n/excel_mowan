#include "analysisbridge.h"

#include <algorithm>

using namespace std;
using std::min;

namespace {

QString MatrixPreview(const QVector<QStringList>& rows, int max_rows = 4, int max_cols = 4) {
    if (rows.isEmpty()) return QStringLiteral("\u5f53\u524d\u6ca1\u6709\u53ef\u7528\u7684\u9009\u533a\u6570\u636e\u3002");

    QStringList lines;
    const int row_count = min(max_rows, static_cast<int>(rows.size()));
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
        return QStringLiteral("\u672a\u9009\u62e9\u6570\u636e\u533a\u57df");
    }

    return QStringLiteral("\u8303\u56f4\uff1a%1\n\u5355\u5143\u683c\uff1a%2\n\u5217\u5934\uff1a%3")
        .arg(snapshot.range_reference)
        .arg(snapshot.selected_cell_count)
        .arg(snapshot.column_headers.join(", "));
}

}  // namespace

void AnalysisBridge::SetPlotHandler(PlotHandler handler) {
    plot_handler_ = move(handler);
}

void AnalysisBridge::SetAiHandler(AiHandler handler) {
    ai_handler_ = move(handler);
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

    return QStringLiteral(
               "Matplotlib \u63a5\u53e3\u5df2\u9884\u7559\uff0c\u8fd8\u6ca1\u6709\u63a5\u5165\u771f\u5b9e Python \u7ed8\u56fe\u5b9e\u73b0\u3002\n\n%1\n\n\u5efa\u8bae\u547d\u4ee4\uff1a\n%2\n\n\u6570\u636e\u9884\u89c8\uff1a\n%3"
           )
        .arg(SnapshotSummary(request.selection))
        .arg(DescribePlotCommand(request))
        .arg(MatrixPreview(request.selection.display_values));
}

QString AnalysisBridge::RunAiAnalysis(const AiAnalysisRequest& request) const {
    if (ai_handler_) {
        return ai_handler_(request);
    }

    return QStringLiteral(
               "Ollama \u63a5\u53e3\u5df2\u9884\u7559\uff0c\u8fd8\u6ca1\u6709\u63a5\u5165\u771f\u5b9e\u7684\u5927\u6a21\u578b\u8c03\u7528\u3002\n\n%1\n\n\u5efa\u8bae\u547d\u4ee4\uff1a\n%2\n\n\u63d0\u793a\u8bcd\uff1a\n%3\n\n\u6570\u636e\u9884\u89c8\uff1a\n%4"
           )
        .arg(SnapshotSummary(request.selection))
        .arg(DescribeAiCommand(request))
        .arg(request.prompt)
        .arg(MatrixPreview(request.selection.display_values));
}

QString AnalysisBridge::DescribePlotCommand(const PlotRequest& request) const {
    const QString chart_type = request.chart_type.isEmpty() ? QStringLiteral("line") : request.chart_type;
    return QStringLiteral("python tools/plot_selection.py --range \"%1\" --chart %2 --title \"%3\"")
        .arg(request.selection.range_reference, chart_type, request.title);
}

QString AnalysisBridge::DescribeAiCommand(const AiAnalysisRequest& request) const {
    const QString model_name = request.model_name.isEmpty() ? QStringLiteral("qwen2.5:7b") : request.model_name;
    return QStringLiteral("ollama run %1 \"\u8bf7\u5206\u6790 %2 \u9009\u533a\u6570\u636e\"")
        .arg(model_name, request.selection.range_reference);
}
