# 自定义系统提示词 设计

日期：2026-08-08
状态：已批准

## 需求

AI 设置菜单新增选项，允许用户自定义发送给 AI 的 system prompt。

## 决策

| 决策点 | 结论 |
|---|---|
| 空值行为 | 自定义提示词留空时使用内置默认提示词（现有中文 prompt 原样保留）；非空时使用自定义内容 |
| 菜单位置 | Basic Settings 区域，Token 文本框之后 |
| 缓冲长度 | 512 字符（MACRO_CONFIG_STR） |

## 改动

1. `src/engine/shared/config_variables_ranbi.h` 追加：

```cpp
MACRO_CONFIG_STR(RcAiSystemPrompt, rc_ai_system_prompt, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

2. `src/game/client/components/ranbi/ai_client.cpp` SendRequest 中 aSystem 拼装改为：

```cpp
// RANBICLIENT m_RcAiSystemPrompt
const char *pPrompt = g_Config.m_RcAiSystemPrompt[0] != '\0' ? g_Config.m_RcAiSystemPrompt : "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断";
str_copy(aSystem, pPrompt, sizeof(aSystem));
```

3. `src/game/client/components/ranbi/menus_ranbi.cpp` RenderRanbiAI 的 Basic Settings 区域（Token 文本框之后）追加一行：

```cpp
Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
Button.VSplitMid(&Label, &Button);
Ui()->DoLabel(&Label, RCLocalize("System prompt"), s_FontSize, TEXTALIGN_ML);
static CLineInput s_SystemPromptInput(g_Config.m_RcAiSystemPrompt, sizeof(g_Config.m_RcAiSystemPrompt));
Ui()->DoEditBox(&s_SystemPromptInput, &Button, s_EditBoxFontSize);
```

（`static CLineInput` 绑定 config 缓冲，沿用既有修复经验）

4. `data/ranbi/languages/simplified_chinese.txt` 追加：

```
System prompt
== 系统提示词
```

## 验证

- 编译（-Werror）
- 手动实测：留空 → 默认提示词行为不变；填写自定义（如"你是毒舌玩家"）→ 回复风格变化；重启客户端配置保留
