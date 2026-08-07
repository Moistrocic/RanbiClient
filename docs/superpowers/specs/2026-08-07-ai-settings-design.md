# AI 设置（AI Settings）功能设计

日期：2026-08-07
状态：已批准

## 需求

1. Ranbi 菜单新增"AI设置"菜单项，与设置、武器设置等并列（新 tab），包含：
   - "基本设置"区域：Base url、Model、Token 三个文本框（样式与现有菜单一致）
   - "其他"区域：自动 AI 回复开关
2. 当有人 @ 自己时触发提醒音效（**既有功能，本设计不开发音效**）；若开启自动 AI 回复，则解析 @ 人与自己的 ID 并自动回复
3. AI 接口需兼容性高（OpenAI 兼容协议），发送频率最快 5 秒一次，本体与分身分开计时
4. 回复长度限制：80 个汉字或 128 个字母；**长度上限写入 system prompt 告知 AI，让 AI 尽可能简化回复**，客户端截断仅作兜底

## 决策记录

| 决策点 | 结论 |
|---|---|
| 实现方式 | 独立组件 `CAiClient`（ranbi 目录，仿现有组件模式）+ 新菜单 tab `RANBI_TAB_AI` |
| @ 解析规则 | 严格三段式 `名字1: 名字2: 文本`；名字2 必须等于本体名或分身名，**区分大小写**（`str_comp`）；文本可为空，空文本同样触发；@人 = 名字1 |
| 消息来源 | 排除服务器消息（m_ClientId < 0）与自己的消息（m_ClientId 等于本体/分身 id） |
| 对话上下文 | 当前单轮无状态；请求体采用 messages 数组结构，为后续多轮上下文与知识库检索预留扩展点 |
| 回复频道 | 跟随 @ 消息的频道（m_Team；m_Team < 0 按 0 处理） |
| 节流 | 本体/分身各自独立计时，最快 5 秒一次；同一时刻最多一个进行中请求 |
| 回复长度 | 提示词告知 AI（80 汉字/128 字母）；客户端加权截断兜底（汉字权重 1.6、其他 1，总权重 ≤ 128） |
| 音效 | 不开发（既有 @ 提醒音效） |

## 架构

```
CNetMsg_Sv_Chat → CAiClient::OnMessage（三段式解析，区分大小写）
→ 判定被 @（本体 m_aLocalIds[0] / 分身 m_aLocalIds[1]）且开关 rc_ai_auto_reply 开启
→ 节流检查（m_aNextReplyTime[0/1] 各自 5 秒）与请求槽检查
→ 构建 messages（system + user）→ HttpPostJson(<base_url>/chat/completions)
→ CAiClient::OnUpdate 每帧轮询请求 Done()/Result()
→ 解析 choices[0].message.content
→ 加权截断（≤128 权重）→ 发送回复（SendMsg(0/1) 指定连接，频道跟随 @ 消息）
```

## 组件 CAiClient

文件：`src/game/client/components/ranbi/ai_client.h` / `ai_client.cpp`

成员：
- `int64_t m_aNextReplyTime[NUM_DUMMIES]`：本体/分身各自的节流时间戳
- `std::shared_ptr<CHttpRequest> m_pRequest`：进行中的请求（单槽）
- `int m_ReplyDummy`：当前请求对应的说话连接（0/1）
- `int m_ReplyTeam`：当前请求对应的回复频道

`OnMessage(int MsgType, void *pRawMsg)`（主线程，每帧消息分发）：

1. 非 `NETMSGTYPE_SV_CHAT` 或 `m_ClientId < 0` → 返回
2. `m_ClientId` 等于本体或分身 id（自己发的消息）→ 返回
3. `rc_ai_auto_reply` 关闭 → 返回
4. 文本严格三段式解析：`名字1: 名字2: 文本`（第一个 `: ` 与第二个 `: ` 分割；无第二个 `: ` 则忽略）。**区分大小写**
5. 名字2 == 本体名（`m_aClients[m_aLocalIds[0]].m_aName`）→ Dummy=0；名字2 == 分身名（`m_aClients[m_aLocalIds[1]].m_aName`，`m_aLocalIds[1]` 有效时）→ Dummy=1；均不匹配 → 返回
6. 节流：`time_get() < m_aNextReplyTime[Dummy]` → 返回
7. 已有进行中请求（`m_pRequest` 非空）→ 返回（避免乱序）
8. 触发：`m_ReplyDummy = Dummy`、`m_ReplyTeam = max(m_Team, 0)`，构建请求发出

`OnUpdate()`（主线程，每帧）：

1. `m_pRequest` 为空 → 返回
2. `m_pRequest->Done()` 为 false → 返回（继续轮询）
3. 请求完成：`State()` 成功 且 `StatusCode()` 2xx → 解析 `ResultJson()`；否则静默丢弃
4. 解析 `choices[0].message.content`（json 遍历；缺失/非字符串 → 静默丢弃）
5. 加权截断（见下）→ 构造 `CNetMsg_Cl_Say` → `Client()->SendMsg(m_ReplyDummy, &Msg, MSGFLAG_VITAL)`（明确指定连接 0/1，不受 cl_dummy 切换影响），`m_Team` 填 m_ReplyTeam
6. `m_pRequest.reset()` 释放槽

## 请求构建（OpenAI 兼容）

- URL：`<rc_ai_base_url>/chat/completions`（base url 末尾不带 `/` 时拼接一个）
- Header：`Authorization: Bearer <rc_ai_token>`、`Content-Type: application/json`
- Body（EscapeJson 转义）：

```json
{"model":"<rc_ai_model>","messages":[
  {"role":"system","content":"你是游戏内聊天助手，回复要尽可能简短。每条回复必须控制在80个汉字以内，或128个字母以内（含标点），超长会被截断。"},
  {"role":"user","content":"<被@的文本>"}
]}
```

- messages 数组是**多轮对话扩展点**：后续在 user 前插入历史消息即可；**知识库扩展点**：system content 中注入检索结果
- 超时：30 秒（`Timeout` 属性）
- 请求失败 / 非 2xx / JSON 解析失败 / content 缺失：静默丢弃，不重试，释放槽

## 长度限制（兜底截断）

- 加权长度：CJK 汉字（U+4E00–U+9FFF）权重 1.6，其他字符权重 1，总权重上限 128
- 语义：80 汉字 × 1.6 = 128 ✓；128 字母 × 1 = 128 ✓，两种上限等价
- 实现：按 UTF-8 字符逐个累加（`str_utf8_decode` 或等价遍历），超限即停止（不会从多字节字符中间截断）
- 提示词已要求 AI 主动控制长度，截断仅在 AI 超长时兜底

## 配置变量（`src/engine/shared/config_variables_ranbi.h` 追加）

```cpp
MACRO_CONFIG_STR(RcAiBaseUrl, rc_ai_base_url, 256, "https://api.openai.com/v1", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcAiModel, rc_ai_model, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcAiToken, rc_ai_token, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiAutoReply, rc_ai_auto_reply, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

## 菜单（`src/game/client/components/ranbi/menus_ranbi.cpp`）

- `RANBI_TAB_AI = 4` 追加到 tab 常量，`NUMBER_OF_RANBI_TABS = 5`
- tab 名数组追加 `"AI Settings"`（`RCLocalize`）；`RenderRanbi` 分发加 `else if(s_CurTab == RANBI_TAB_AI) RenderRanbiAI(MainView);`
- 新增 `RenderRanbiAI(CUIRect MainView)`：与现有 tab 相同的布局骨架（左列 `Column`、`s_SectionBoxes` 分区、标题用 `s_HeadlineFontSize`）：
  - **"Basic Settings"（基本设置）区域**：三个文本框行，每行 `HSplitTop` + 左标签 + `DoEditBox`（`CLineInput` 直接绑定 config 字符串缓冲区，如 `CLineInput s_BaseUrlInput(g_Config.m_RcAiBaseUrl, sizeof(g_Config.m_RcAiBaseUrl))`，样式与 menus_settings.cpp 一致）
    - "Base url" → `rc_ai_base_url`
    - "Model" → `rc_ai_model`
    - "Token" → `rc_ai_token`（普通文本框，非密码掩码，与项目现状一致）
  - **"Other"（其他）区域**：开关"Auto AI reply"（自动 AI 回复）→ `DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAiAutoReply, ...)`
- 翻译文件 `data/ranbi/languages/simplified_chinese.txt` 追加：
  - `AI Settings` / `== AI设置`
  - `Basic Settings` / `== 基本设置`
  - `Base url` / `== Base url`（保留原文或译"接口地址"，按现有习惯）
  - `Model` / `== 模型`
  - `Token` / `== Token`
  - `Other` / `== 其他`
  - `Auto AI reply` / `== 自动 AI 回复`

## 注册与构建

1. `src/game/client/gameclient.h`：include `"components/ranbi/ai_client.h"` + 成员 `CAiClient m_AiClient;`
2. `src/game/client/gameclient.cpp`：`m_vpAll` 中 ranbi 组件区追加 `&m_AiClient, // RanbiClient`
3. `CMakeLists.txt`：GAME_CLIENT_SRC 列表（components/ranbi/ 段，字母序）追加 `ai_client.cpp` / `ai_client.h`

## 验证

- 编译：`cmake --build build/Debug -j 8`（-Werror，8 核）
- 手动实测清单：
  1. AI 设置 tab 显示正常，三个文本框可编辑保存，开关可切换
  2. 配置真实 OpenAI 兼容接口后，他人 @ 自己（本体名）→ 自动回复发出，频道正确
  3. 他人 @ 自己分身名 → 分身说话
  4. 5 秒内重复被 @ → 不重复请求/发送
  5. 空文本 @（`X: 我的名字: `）→ 仍触发
  6. 大小写不匹配的名字 → 不触发
  7. 回复超长时被截断（≤128 权重）
