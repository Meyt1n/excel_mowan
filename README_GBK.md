# excel_mowan

一个面向课程设计的轻量电子表格项目（控制台 + GUI），核心目标是：
- 支持单元格文本、数字、公式计算
- 支持单元格引用、区域函数、循环引用检测
- 支持 CSV 输入输出和自定义 DAT 存取
- 保持代码结构清晰，便于课堂答辩讲解

## 1. 源码文件说明（逐文件）

### 1.1 `core/`：表格核心计算层

`core/basic.h`
- 定义基础常量 `kMaxRows`、`kMaxCols`
- 定义地址类型 `Address`（如 `A1`）、值类型 `Value`、单元格 `Cell`
- 声明原始输入解析函数 `ParseRawValue`

`core/basic.cpp`
- 实现地址合法性检查、地址字符串互转（`A1` <-> 行列下标）
- 实现 `Value` 构造与字符串化输出（数值保留 15 位有效数字）
- 实现原始输入解析：空串、数字、文本判定

`core/formula.h`
- 定义公式 AST：`Node`、`Range`
- 声明公式解析器 `Parser`、求值器 `Evaluator`
- 声明公式求值上下文 `EvalContext`（取单元格值、区域函数回调）
- 声明公式输入标准化函数 `NormalizeFormulaInput`

`core/formula.cpp`
- 实现词法分析、语法分析（支持 `+ - * / %`、括号、函数、单元格/区域引用）
- 实现公式求值逻辑（数值/文本、错误传播、空值处理）
- 实现函数：`SQRT`、`ABS`、`SUM`、`AVG`、`MIN`、`MAX`、`COUNT`、`ROUND`、`POW`
- 实现公式标准化：仅将公式中引号外字母转大写（保留字符串字面量内容）

`core/spreadsheet.h`
- 声明核心网格类 `SpreadsheetGrid`
- 提供接口：`SetCell`、`GetValue`、`GetRaw`、`RecalcAll`、`Clear`、`ForEachCell`

`core/spreadsheet.cpp`
- 实现单元格写入、公式编译、依赖收集与循环引用检测
- 实现整表重算和递归求值
- 实现区域计算辅助（`SUM/AVG/MIN/MAX/COUNT`）
- 统一从 `SetCell` 入口进行公式标准化与错误处理

### 1.2 `io/`：文件格式层

`io/csv_file.h`
- 声明 CSV 行拆分/拼接工具

`io/csv_file.cpp`
- 实现 CSV 解析与生成
- 支持引号与转义规则（`""`）

`io/dat_file.h`
- 声明 DAT 文件读写接口：`DatFile::Save/Load`

`io/dat_file.cpp`
- 实现 DAT 保存：头信息 + 非空单元格原始内容
- 实现 DAT 加载：读取并恢复到 `SpreadsheetGrid`
- DAT 头格式为 `EMW,1,rows,cols`

### 1.3 `app/`：控制台应用层

`app/table_file_io.h`
- 声明应用层文件流程函数（CSV -> Grid，Grid -> CSV）

`app/table_file_io.cpp`
- 实现按行读取、CSV 解析入表格、表格结果导出 CSV
- 负责行列裁剪到系统最大规模（`32767 x 256`）

`app/console_main.cpp`
- 控制台主程序入口（可从文件或标准输入读取 CSV）
- 执行重算并输出结果 CSV
- 向 `stderr` 输出计算耗时 `calc_ms=...`

`app/dat_tool.cpp`
- DAT 转换工具入口
- `save`：CSV -> DAT
- `load`：DAT -> CSV（加载后重算）

`app/bench_main.cpp`
- 基准测试工具入口
- 读取 3 个案例，计算平均耗时 `avg_time_ms`
- 统计存储效率 `storage_efficiency`（DAT/CSV 大小比）

### 1.4 `gui/`：Qt 图形界面层

`gui/CMakeLists.txt`
- GUI 工程构建脚本
- 指定 Qt Widgets 依赖和参与编译的源文件

`gui/main.cpp`
- Qt 程序入口
- 创建 `QApplication`，设置应用名与字体，启动主窗口

`gui/mainwindow.h`
- 主窗口类声明
- 定义公式栏、表格视图、状态栏与公式编辑模式状态

`gui/mainwindow.cpp`
- 实现主界面布局与样式
- 实现公式栏与表格选择联动
- 实现公式编辑模式：公式栏输入 `=` 开头时，点击单元格可插入地址（如 `B3`）
- 实现提交逻辑：在公式编辑模式下回写到最初开始编辑的单元格

`gui/spreadsheetmodel.h`
- Qt `QAbstractTableModel` 适配层声明
- 连接 Qt 模型接口与 `SpreadsheetGrid`

`gui/spreadsheetmodel.cpp`
- 实现网格数据展示与编辑写回
- `DisplayRole` 显示计算值，`EditRole` 显示原始输入
- 写入后触发重算并刷新视图

`gui/spreadsheetview.h`
- 自定义 `QTableView` 声明

`gui/spreadsheetview.cpp`
- 实现“选中单元格后直接打字开始编辑”的交互增强
- 保留 Qt 原生按键行为（方向键、功能键等）

## 2. 根目录其他文件说明

`README.md`
- 本说明文档（UTF-8）

`README_GBK.md`
- 与 `README.md` 内容保持一致的副本（文件名保留兼容习惯，内容统一使用 UTF-8）

`input.csv`、`output.csv`、`input.dat`、`output.dat`、`output_from_dat.csv`
- 示例输入/输出文件，便于本地快速验证

`console_main.exe`、`dat_tool.exe`、`bench_main.exe`、`myxls.exe`
- 本地编译生成的可执行文件（可能随你的编译环境变化）

## 3. 当前已实现功能清单

- 单元格支持文本、数字、公式三类输入
- 公式支持运算符：`+ - * / %`
- 公式支持单元格引用与区域引用：如 `A1`、`A1:B3`
- 公式函数支持：
- `SQRT(x)`、`ABS(x)`
- `SUM(range)`、`AVG(range)`、`MIN(range)`、`MAX(range)`、`COUNT(range)`
- `ROUND(x)`、`ROUND(x,n)`、`POW(x,y)`
- 循环引用检测（直接/间接）与错误传播
- CSV 读写（含引号转义）
- DAT 保存与加载（保存原始单元格内容）
- GUI 中公式栏引用插入与直接打字编辑交互

## 4. 编译与运行（常用）

控制台计算器：
```bash
g++ -std=c++17 -O2 -o console_main.exe \
  app/console_main.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

DAT 工具：
```bash
g++ -std=c++17 -O2 -o dat_tool.exe \
  app/dat_tool.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

基准工具：
```bash
g++ -std=c++17 -O2 -o bench_main.exe \
  app/bench_main.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

GUI：
- 使用 `gui/CMakeLists.txt`，在 Qt Creator 或 CMake 环境中构建 `excel_mowan_gui`

## 5. 防止中文乱码说明

- 本项目文档统一使用 **UTF-8** 编码。
- 若 IDE 打开后中文异常，请将文件编码切换为 `UTF-8` 后重新载入。
- 在 Windows 终端查看中文时，建议先执行：`chcp 65001`
