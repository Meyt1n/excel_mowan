# Excel Mowan

`Excel Mowan` 是一个用 C++17 实现的轻量级电子表格项目，包含核心计算引擎、命令行工具、CSV/DAT 文件处理和一个基于 Qt Widgets 的图形界面。

这个项目的重点不是复刻完整 Excel，而是把电子表格系统里最关键的几个问题拆开实现：

- 单元格原始内容如何存储
- 公式如何解析与求值
- 单元格依赖如何管理
- 数据如何在核心层、文件层和界面层之间流动

## 项目结构

### `core/`

核心计算层。

- `basic.h/.cpp`
  定义基础常量和数据类型：
  - `kMaxRows`、`kMaxCols`
  - `Address`
  - `Value`
  - `Cell`
  - `ParseRawValue`

- `formula.h/.cpp`
  负责公式系统：
  - 词法分析
  - 递归下降语法分析
  - AST 节点定义
  - 公式求值器 `Evaluator`

- `spreadsheet.h/.cpp`
  负责表格本体：
  - 单元格写入
  - 公式依赖收集
  - 循环引用检测
  - 增量标脏
  - 懒求值与缓存

### `io/`

文件格式层。

- `csv_file.h/.cpp`
  处理 CSV 行拆分和拼接，支持引号与转义。

- `dat_file.h/.cpp`
  处理自定义 DAT 格式，只存表格尺寸和非空单元格的原始内容。

### `app/`

命令行工具层。

- `console_main.cpp`
  从带尺寸头的文本输入读取表格，计算后输出格式化结果。

- `dat_tool.cpp`
  提供 `CSV <-> DAT` 转换。

- `bench_main.cpp`
  做简单性能统计和存储效率统计。

- `table_file_io.h/.cpp`
  放应用层共享的表格读写逻辑，统一处理：
  - CSV -> `SpreadsheetGrid`
  - 带尺寸头文本 -> `SpreadsheetGrid`
  - `SpreadsheetGrid` -> CSV
  - `SpreadsheetGrid` -> 控制台纯文本

### `gui/`

Qt 图形界面层。

- `spreadsheetmodel.h/.cpp`
  用 `QAbstractTableModel` 适配核心表格。

- `spreadsheetview.h/.cpp`
  扩展表格交互，支持直接输入、行内编辑和公式编辑体验。

- `mainwindow.h/.cpp`
  主窗口，负责菜单、公式栏、状态栏和批量编辑。

- `analysisbridge.h/.cpp`
  预留图表和 AI 分析桥接接口，把当前选区打包成请求对象。

## 整体实现流程

用户输入内容到一个单元格时，处理路径大致如下：

1. GUI 或命令行把字符串交给 `SpreadsheetGrid::SetCell`
2. `SetCell` 判断输入是空值、普通值还是公式
3. 如果是公式：
   - 调用 `NormalizeFormulaInput`
   - 用 `Parser` 解析成 AST
   - 遍历 AST 收集依赖
   - 检查是否形成循环引用
4. 写入成功后，把当前单元格的所有下游依赖标记为 `dirty`
5. 后续 `GetValue()` 时再按需递归求值，并把结果缓存起来

这套设计的核心思想是：

- 写入时做结构检查
- 读取时做按需计算
- 修改后只失效受影响的公式

## 表格用了哪些数据结构

| 结构 | 定义位置 | 作用 | 说明 |
| --- | --- | --- | --- |
| `Address` | `core/basic.h` | 表示单元格坐标 | 用 `row` / `col` 存储，支持 `A1 <-> (0,0)` 转换 |
| `Value` | `core/basic.h` | 表示单元格值 | 统一表示空值、数字、文本、错误 |
| `Cell` | `core/basic.h` | 表示单元格原始内容 | 保存 `raw` 和当前 `value` |
| `Node` | `core/formula.h` | 表示公式 AST 节点 | 支持数字、字符串、单元格、区域、单目、双目、函数 |
| `CellState` | `core/spreadsheet.h` | 表示内部运行状态 | 在 `Cell` 外增加 AST、依赖、脏标记和错误标记 |
| `unordered_map<int, CellState>` | `core/spreadsheet.h` | 稀疏存储表格 | 只存非空单元格，键为 `row * kMaxCols + col` |
| `unordered_map<int, vector<int>> reverse_deps_` | `core/spreadsheet.h` | 反向依赖图 | 记录“哪些公式依赖当前单元格” |
| `vector<unique_ptr<Node>>` | `core/formula.h` | 保存函数参数 | 管理 AST 子节点所有权 |

### 为什么用 `unordered_map` 存表格

项目最大支持 `32767 x 256` 个位置，但真实填值往往只占很小一部分。如果直接用二维数组：

- 空单元格也会占空间
- 大量内存会被浪费

所以这里用了稀疏存储：

- 键：`row * kMaxCols + col`
- 值：`CellState`

只有真正写入过的单元格才会进入 `cells_`。

### 为什么还要维护 `reverse_deps_`

例如：

- `C1 = A1 + B1`
- `D1 = C1 * 2`

当 `A1` 改变时，不只是 `C1` 失效，`D1` 也要跟着失效。  
因此系统除了给每个公式记录 `deps` 外，还维护反向依赖表 `reverse_deps_`，让 `MarkDependentsDirty()` 能沿着下游把缓存统一标脏。

## 单元格如何处理不同数据类型

项目里的值类型定义在 `ValueType`：

- `Empty`
- `Number`
- `Text`
- `Error`

核心结构是：

```cpp
struct Value {
    ValueType type = ValueType::Empty;
    double number = 0.0;
    std::string text;
};
```

### 1. 空值

原始输入为空，或者去掉首尾空白后为空，就保存为 `Value::Empty()`。

处理规则：

- 显示时输出空串
- 数值上下文里通常按 `0` 处理
- 区域统计里不计入数字数量

### 2. 数字

`ParseRawValue()` 会先裁剪首尾空白，再调用 `std::strtod`。

如果整串都能被解析成数字，就保存为：

- `type = Number`
- `number = 解析结果`

例如：

- `"12"`
- `"3.14"`
- `"  42 "`

都会被识别为数字。

### 3. 文本

如果去掉空白后无法完整解析成数字，就按文本处理。

例如：

- `"hello"`
- `"12abc"`
- `"订单-01"`

### 4. 错误

错误主要来自公式求值失败，例如：

- 除以 0
- 文本参与非法数值运算
- 区域函数遇到错误或文本
- 公式语法错误
- 循环引用

当前实现里会区分两类状态：

- `formula_error`：公式解析失败或循环引用
- `eval_error`：公式结构合法，但运行期求值失败

GUI 通过 `SpreadsheetGrid::HasError()` 决定是否把单元格显示成 `#NA`。

## 公式系统如何实现

### 公式标准化

输入以 `=` 开头时视为公式。  
在进入解析器之前，系统会调用 `NormalizeFormulaInput()`：

- 把字符串字面量之外的字母转成大写
- 保留字符串内部内容

例如：

```text
=sum(a1:b3)
```

会被标准化为：

```text
=SUM(A1:B3)
```

### 词法分析

`Parser::Lexer` 把公式切成 token，支持：

- 数字
- 字符串字面量
- 标识符
- `(` `)` `,` `:`
- `+ - * / %`

### 语法分析

`Parser` 使用递归下降解析：

- `ParseExpression` 处理 `+ -`
- `ParseTerm` 处理 `* / %`
- `ParseUnary` 处理单目 `+ -`
- `ParsePrimary` 处理数字、字符串、单元格、函数、括号

最终输出一棵 AST。

### AST 节点类型

`Node::Kind` 支持：

- `Number`
- `String`
- `Cell`
- `Range`
- `Unary`
- `Binary`
- `Func`

例如：

```text
=A1 + SUM(B1:B3)
```

会被表示成一个二元加法节点：

- 左边是 `Cell(A1)`
- 右边是 `Func(SUM, Range(B1:B3))`

### 求值器

`Evaluator` 会递归遍历 AST：

- `Cell` 通过 `EvalContext::get_cell` 回到表格取值
- `Range` 只作为区域函数参数
- `Binary` 处理算术运算
- `Func` 处理函数

当前支持函数：

- `SIN`
- `COS`
- `SQRT`
- `ABS`
- `SUM`
- `AVG`
- `MIN`
- `MAX`
- `COUNT`
- `ROUND`
- `POW`

## 依赖管理与重算机制

### 依赖收集

写入公式时，`CollectDependencies()` 会遍历 AST，把里面涉及的：

- 单元格引用
- 区域里的所有单元格

全部收集到 `deps`。

### 循环引用检测

`WouldCreateCycle()` 会检查“新依赖是否能回到自己”。  
如果形成回路，例如：

- `A1 = B1`
- `B1 = A1`

或更长链条的间接回路，就拒绝写入，并记录错误。

### 增量标脏

单元格修改后会做两件事：

1. 更新自己的依赖关系
2. 通过 `reverse_deps_` 把所有下游公式标记为 `dirty`

### 懒求值

`GetValue()` 最终会调用 `EvalCellInternal()`：

- 如果单元格不是脏的，直接返回缓存值
- 如果是脏的，才重新递归计算

这让系统避免了每次写入都全表重算。

## 不同数据类型在运算中的规则

### 数值上下文

`Evaluator::EnsureNumber()` 的规则是：

- 数字 -> 原样返回
- 空值 -> 视为 `0`
- 文本 / 错误 -> 返回错误

所以：

- `=A1 + 1`，如果 `A1` 为空，结果是 `1`
- `=A1 + 1`，如果 `A1` 是文本，结果报错

### `+` 的特殊规则

`+` 同时支持数值加法和字符串拼接。

只要左右任意一个操作数是文本，就按拼接处理：

- 文本直接拼接
- 数字先转成字符串
- 空值按空串处理

例如：

- `"ab" + "cd"` -> `"abcd"`
- `"ab" + 12` -> `"ab12"`

### 其他运算

`- * / %` 都要求数值上下文。

特别地：

- 除数为 0 -> 错误
- `%` 的右操作数为 0 -> 错误

## 区域函数如何处理数据

区域函数由以下辅助函数实现：

- `CalcRangeSum`
- `CalcRangeAvg`
- `CalcRangeMin`
- `CalcRangeMax`
- `CalcRangeCount`

处理流程：

1. 先把区域规范成左上到右下
2. 逐个单元格读取值
3. 按规则聚合

当前规则：

- 遇到错误 -> 返回错误
- 遇到文本 -> 返回错误
- 空值不参与数字统计
- `COUNT` 只统计数字单元格

## 文件格式设计

### CSV

`CsvFile` 只负责“单行字符串”和“字段列表”之间的转换，支持：

- 逗号分隔
- 引号包裹字段
- `""` 表示字段中的引号

### DAT

DAT 是自定义格式，目标是：

- 保存表格尺寸
- 只保存非空单元格
- 保存原始输入，而不是只保存计算结果

头部格式：

```text
EMW,1,rows,cols
```

后续每一行格式：

```text
地址,原始内容
```

例如：

```text
EMW,1,10,5
A1,123
B1,=A1*2
```

这样重新加载后，公式仍然保留，不会丢失成纯结果值。

## GUI 是怎么接到核心层上的

`SpreadsheetModel` 是 GUI 和核心之间的桥：

- `DisplayRole` 返回计算结果
- `EditRole` 返回原始输入
- `ToolTipRole` 同时展示输入与结果
- `TextAlignmentRole` 让数字右对齐

这样用户在 GUI 里可以同时获得：

- 编辑态看到原始公式
- 显示态看到计算结果
- 悬停时看到“输入”和“结果”的对应关系

`SpreadsheetView` 则继续补齐交互体验：

- 直接键入开始编辑
- 双击进入行内编辑
- 公式编辑时点选单元格自动插入引用

## 命令行工具

### `console_main`

输入格式：

```text
rows cols
row1
row2
...
```

每一行仍按 CSV 规则拆字段。

输出格式：

- 数字保留两位小数
- 空值和错误输出为 `0.00`
- 文本原样输出
- 字段之间用空格分隔

### `dat_tool`

```bash
dat_tool.exe save input.csv output.dat
dat_tool.exe load input.dat output.csv
```

### `bench_main`

```bash
bench_main.exe case1.csv case2.csv case3.csv
```

输出：

- `avg_time_ms`
- `storage_efficiency`

## 编译方式

### 控制台工具

```bash
g++ -std=c++17 -O2 -o console_main.exe \
  app/console_main.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

### DAT 工具

```bash
g++ -std=c++17 -O2 -o dat_tool.exe \
  app/dat_tool.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

### 基准工具

```bash
g++ -std=c++17 -O2 -o bench_main.exe \
  app/bench_main.cpp app/table_file_io.cpp \
  core/basic.cpp core/formula.cpp core/spreadsheet.cpp \
  io/csv_file.cpp io/dat_file.cpp
```

### GUI

GUI 使用 `gui/CMakeLists.txt` 构建，需要 Qt5 或 Qt6 Widgets。

## 这次整理掉的冗余

这次主要清理了三类冗余：

1. `console_main.cpp` 中重复的网格装载和输出逻辑
   现在统一复用 `app/table_file_io.cpp`

2. `MainWindow` 里没有实际行为的空函数
   这些空壳调用已经移除

3. GUI 中重复的选区边界计算逻辑
   现在统一提成一个公共小工具

同时，项目说明统一到这一份 `README.md`，避免两份完整文档长期漂移。

## 答辩时可以怎么概括

如果你需要一句适合答辩时的总结，可以直接这样说：

> 这个项目采用“核心计算层 + 文件格式层 + 应用层 + Qt 表现层”的分层结构。表格本体使用 `unordered_map` 做稀疏存储，公式先解析成 AST，再通过依赖图和反向依赖图实现循环检测、按需重算和增量失效。单元格值统一抽象成 `Empty / Number / Text / Error` 四种类型，并在公式求值、区域统计、CSV/DAT 持久化以及 GUI 展示中使用一致的处理规则。
## Performance Optimization

The project now uses incremental update paths instead of full-grid refresh after every edit.

### Before

- Every edit triggered `RecalcAll()`
- The Qt model emitted one `dataChanged` covering the entire grid
- Display code often queried the same cell twice through `GetValue()` and `HasError()`

### After

- The core keeps a reverse dependency graph in `reverse_deps_`
- `CollectAffectedCells()` walks only the changed cell and its downstream dependents
- `SpreadsheetModel::setData()` refreshes only affected addresses
- Adjacent changed cells on the same row are merged into a single `dataChanged` segment
- `EvaluatedCell` returns both value and error state in one call, avoiding duplicate evaluation
- `clearAll()` now uses `beginResetModel()/endResetModel()`

### Benchmark Outputs

`bench_main` now reports two extra metrics in addition to the original ones:

- `incremental_update_avg_ms`
- `incremental_avg_affected_cells`

These are intended to show the cost of editing a small number of cells after the sheet has already been loaded.

### Defense Summary

You can describe the optimization as moving from a "full recompute + full repaint" model to an
"incremental recompute + local repaint" model, based on:

- sparse storage with `unordered_map`
- dependency tracking
- reverse dependency traversal
- lazy evaluation
- localized Qt model notifications
