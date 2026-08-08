# AI 设置功能扩展（v2）设计

日期：2026-08-08
状态：已批准

## 需求

对现有 AI 设置功能（CAiClient）做 6 项扩展：
1. @ 检测改为：消息任意位置出现本体或分身名字（词边界）即触发
2. 回复前缀 `人名: `（不带 @ 符号，与服务器消息格式一致）
3. 新增菜单配置：回复频率、温度、上下文条数
4. 上下文队列：{说话者, 消息文本, 是否AI回复} FIFO，随请求发送，条数可配
5. 外部知识库：Storage 用户目录优先（换客户端不丢），关键词预筛选注入
6. debug 输出精简为单行摘要（说话人/回复给谁/回复/是否触发知识库/长度/token 数）

## 决策记录

| 决策点 | 结论 |
|---|---|
| 实现方式 | 方案 A：单组件扩展（CAiClient 内部完成，不动其他文件） |
| @ 检测 | 任意位置出现名字 + **词边界**（名字前后不能紧贴字母/数字）；区分大小写；排除服务器消息（m_ClientId < 0）与自己消息 |
| 触发上下文 | 完整消息文本（含说话者名与名字出现位置） |
| 回复前缀 | `人名: ` + AI 回复（@人 = 说话者 m_ClientId 的玩家名）；前缀不计入 128 权重截断 |
| 回复频率 | rc_ai_reply_interval：1~60 秒，默认 2（研究结论：客户端 SendChatQueued 与服务端 sv_chat_delay 下限均 1 秒，低于 1 秒会被服务端静默丢弃） |
| 温度 | rc_ai_temperature：0~20（0.0~2.0），默认 10（1.0），请求体 "temperature": x.x |
| 上下文条数 | rc_ai_context_count：0~20，默认 5；0 = 单轮 |
| 上下文队列 | FIFO {说话者名, 文本, 是否AI回复}；玩家消息 → role=user、content="说话者: 文本"；AI 回复 → role=assistant；失败时移除占位项保持 user/assistant 交替合法 |
| 知识库 | Storage 虚拟路径 `ranbi/knowledge/`，TYPE_ALL：用户目录优先（Windows `%APPDATA%/DDNet`、Linux `~/.local/share/ddnet` 一类），客户端 data 目录次选兜底；换客户端不丢；运行中修改无需重启 |
| 知识库筛选 | 关键词预筛选：提问含文件名（去 .txt，不区分大小写）或提问中任一连续 4+ 字符片段出现在文件内容中 → 命中注入 |
| debug 输出 | 成功：`[AI] 说话人: X | 回复给: 本体/分身 | 回复: xxx | 知识库: 是/否 | 长度: n/128 | tokens: n`；失败：`[AI失败] 原因`；移除现有全部详细日志 |

## 组件改动（src/game/client/components/ranbi/ai_client.cpp）

### @ 检测（ParseMention 重写）

```cpp
// 在消息中查找名字的所有出现位置，词边界匹配（区分大小写）
bool IsWordBoundary(char c);  // 前后字符非字母/数字即边界

// 对每个 dummy（D==1 需 DummyConnected）：
//   str_find 循环找 pName 出现位置，检查前后字符边界，命中即触发
```

- 一段式/三段式解析逻辑删除
- 触发文本 = 完整消息（OnMessage 直接传 pMsg->m_pMessage）

### 上下文队列

```cpp
struct CContextEntry
{
	char m_aSpeaker[16];   // 说话者名字
	char m_aText[256];     // 消息文本（存储时截断至 256 字符）
	bool m_IsReply;        // 是否 AI 回复
};
std::vector<CContextEntry> m_Context;  // FIFO，容量 = rc_ai_context_count
```

- 触发时 push {说话者, 消息文本, false}；AI 回复到达回填 {NULL/空说话者, 回复, true}
- 超出容量移除最旧项；rc_ai_context_count == 0 时不入队
- 请求组装：[system, (user, assistant)×N, user(当前)]；user content = "说话者: 文本"

### 请求构建

```json
{"model":"...","temperature":1.0,"messages":[
  {"role":"system","content":"你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断\n[知识库: 文件名]\n内容...(命中时)"},
  {"role":"user","content":"Alvicean: 666"},
  {"role":"assistant","content":"..."},
  {"role":"user","content":"Merikaros: Hi!"}
]}
```

- 缓冲：aBody 32KB；EscapeJson 各段独立缓冲
- system prompt 中文原文（用户指定，已 EscapeJson）

### 知识库

```cpp
// 每次触发时：
// 1. Storage()->FindFiles(pFilename, "ranbi/knowledge", IStorage::TYPE_ALL, &Entries)
//    （pFilename 参数语义实现时确认；文件列表 set 自动去重，用户目录优先）
// 2. 对每个 .txt：Storage()->ReadFile("ranbi/knowledge/<file>", IStorage::TYPE_ALL, ...)
//    单文件上限 4KB
// 3. 命中规则（任一）：
//    a) 提问文本包含文件名（去 .txt，不区分大小写）
//    b) 提问文本任一连续 4+ 字符片段出现在文件内容中
// 4. 命中内容拼入 system（[知识库: <文件名>]\n<内容>），总注入上限 8KB
```

### 节流

- m_aNextReplyTime 间隔改用 g_Config.m_RcAiReplyInterval（本体/分身分开计时不变）

### debug 输出

```cpp
dbg_msg("ranbi_ai", "[AI] 说话人: %s | 回复给: %s | 回复: %s | 知识库: %s | 长度: %d/%d | tokens: %d",
    SpeakerName, Dummy == 0 ? "本体" : "分身", ReplyText, KnowledgeHit ? "是" : "否", Length, 128, CompletionTokens);
```

- tokens 从响应 JSON `usage.completion_tokens` 取（缺失时 0）
- 失败：`dbg_msg("ranbi_ai", "[AI失败] %s", 原因)`（状态/状态码/JSON 解析/content 缺失）

## 菜单（menus_ranbi.cpp）

Other 区域追加 3 个滑块（DoScrollbarOption，样式与现有一致）：

| 标签 | 变量 | 范围 |
|---|---|---|
| Reply interval (seconds) | rc_ai_reply_interval | 1~60 |
| Temperature | rc_ai_temperature | 0~20（显示 x.x） |
| Context count | rc_ai_context_count | 0~20 |

## 配置变量（config_variables_ranbi.h 追加）

```cpp
MACRO_CONFIG_INT(RcAiReplyInterval, rc_ai_reply_interval, 2, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiTemperature, rc_ai_temperature, 10, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiContextCount, rc_ai_context_count, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

## 翻译（simplified_chinese.txt 追加）

```
Reply interval (seconds) / == 回复间隔（秒）
Temperature / == 温度
Context count / == 上下文条数
```

## 研究依据（服务端刷屏检测）

- 客户端：`CChat::SendChatQueued`（chat.cpp:1292/1518）本地 1 秒间隔排队
- 服务端：`sv_chat_delay` 默认 1 秒（config_variables.h:546）+ 消息长度动态间隔（gamecontext.cpp:2311，`(31+Length)/32` tick）+ 团队聊天额外 1 秒（teams.cpp:113）
- 结论：回复间隔下限 1 秒（配置范围 1~60）

## 验证

- 编译（-Werror）
- 手动实测：
  1. 任意位置名字触发（词边界：短名字不匹配子串；标点旁/行首行尾命中）
  2. 回复带 `人名: ` 前缀
  3. 上下文多轮：AI 能引用前几轮、区分说话者；条数配置生效；0 = 单轮
  4. 知识库：用户目录放文件命中注入、换客户端不丢、运行中修改生效、不命中不注入
  5. 频率/温度配置生效
  6. debug 输出六字段齐全；失败输出一行原因
