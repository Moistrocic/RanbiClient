#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/components/ranbi/ranbi_client.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <vector>

static constexpr int RANBI_TAB_SETTINGS = 0;
static constexpr int RANBI_TAB_WEAPONS_SETTINGS = 1;
static constexpr int RANBI_TAB_DDNET_MORE = 2;
static constexpr int RANBI_TAB_INFO = 3;
static constexpr int NUMBER_OF_RANBI_TABS = 4;

static const float s_FontSize = 14.0f;
static const float s_EditBoxFontSize = 12.0f;
static const float s_LineSize = 20.0f;
static const float s_ColorPickerLineSize = 25.0f;
static const float s_HeadlineFontSize = 20.0f;
static const float s_HeadlineHeight = s_HeadlineFontSize + 0.0f;
static const float s_Margin = 10.0f;
static const float s_MarginSmall = 5.0f;
static const float s_MarginExtraSmall = 2.5f;
static const float s_MarginBetweenSections = 30.0f;
static const float s_MarginBetweenViews = 30.0f;
static const float s_ColorPickerLabelSize = 13.0f;
static const float s_ColorPickerLineSpacing = 5.0f;

void CMenus::RenderRanbiSettings(CUIRect MainView)
{
	CUIRect Column, LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;

	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, s_MarginBetweenViews);
	LeftView.VSplitLeft(s_MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(s_MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = s_MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column = LeftView;

	// Dummy
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Dummy"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcCursorCopy, RCLocalize("Cursor copy"), &g_Config.m_RcCursorCopy, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Render
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Render"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcEmotionDeferredRender, RCLocalize("Emotion deferred render"), &g_Config.m_RcEmotionDeferredRender, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcHookCollDeferredRender, RCLocalize("Hook coll deferred render"), &g_Config.m_RcHookCollDeferredRender, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Spectator
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Spectator"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcShowSpectatorSkin, RCLocalize("Show spectator skin"), &g_Config.m_RcShowSpectatorSkin, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcShowAllPlayers, RCLocalize("Receive all players (may reduce frame rate)"), &g_Config.m_RcShowAllPlayers, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Skip three tiles
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Skip three tiles notify"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcShowEnableSkipThreeTiles, RCLocalize("Enable skip three tiles notify"), &g_Config.m_RcShowEnableSkipThreeTiles, &Column, s_LineSize);

	if(g_Config.m_RcShowEnableSkipThreeTiles)
	{
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcShowEnableSkipThreeTilesInfoSize, &g_Config.m_RcShowEnableSkipThreeTilesInfoSize, &Button, RCLocalize("Size"), 0, 50);

		static CButtonContainer s_SkipThreeTilesInfoColor;
		DoLine_ColorPicker(&s_SkipThreeTilesInfoColor, s_ColorPickerLineSize, s_ColorPickerLabelSize, s_ColorPickerLineSpacing, &Column, RCLocalize("Color"), &g_Config.m_RcShowEnableSkipThreeTilesInfoColor, ColorRGBA(0.0f, 0.0f, 0.0f), false);

		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcShowEnableSkipThreeTilesInfoPositionX, &g_Config.m_RcShowEnableSkipThreeTilesInfoPositionX, &Button, RCLocalize("Position X"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcShowEnableSkipThreeTilesInfoPositionY, &g_Config.m_RcShowEnableSkipThreeTilesInfoPositionY, &Button, RCLocalize("Position Y"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		static CLineInput s_LeftText(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionLeftText, sizeof(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionLeftText));
		Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, RCLocalize("Left text"), s_FontSize, TEXTALIGN_ML);
		Ui()->DoEditBox(&s_LeftText, &Button, s_EditBoxFontSize);

		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		static CLineInput s_RightText(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionRightText, sizeof(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionRightText));
		Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, RCLocalize("Right text"), s_FontSize, TEXTALIGN_ML);
		Ui()->DoEditBox(&s_RightText, &Button, s_EditBoxFontSize);

		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		static CLineInput s_DJLeftText(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionDJLeftText, sizeof(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionDJLeftText));
		Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, RCLocalize("DJ left text"), s_FontSize, TEXTALIGN_ML);
		Ui()->DoEditBox(&s_DJLeftText, &Button, s_EditBoxFontSize);

		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		static CLineInput s_DJRightText(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionDJRightText, sizeof(g_Config.m_RcShowEnableSkipThreeTilesInfoPositionDJRightText));
		Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, RCLocalize("DJ right text"), s_FontSize, TEXTALIGN_ML);
		Ui()->DoEditBox(&s_DJRightText, &Button, s_EditBoxFontSize);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Scoreboard
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Scoreboard"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcScoreboardShowLove, RCLocalize("Scoreboard show love"), &g_Config.m_RcScoreboardShowLove, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcScoreboardShowPoints, RCLocalize("Scoreboard show ddrace points"), &g_Config.m_RcScoreboardShowPoints, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcScoreboardCopyName, RCLocalize("Scoreboard copy name"), &g_Config.m_RcScoreboardCopyName, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Chat
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Chat"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcChatSkipRepeatHistory, RCLocalize("Chat skip repeat history"), &g_Config.m_RcChatSkipRepeatHistory, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcChatShowPoints, RCLocalize("Chat show ddrace points"), &g_Config.m_RcChatShowPoints, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	LeftView = Column;
	Column = RightView;

	// Nameplates
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Nameplates"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowPoints, RCLocalize("Show ddrace points"), &g_Config.m_RcNameplatesShowPoints, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowPositionX, RCLocalize("Show position X"), &g_Config.m_RcNameplatesShowPositionX, &Column, s_LineSize);

	if(g_Config.m_RcNameplatesShowPositionX)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowPositionXMatched, RCLocalize("Show position X matched"), &g_Config.m_RcNameplatesShowPositionXMatched, &Column, s_LineSize);

		static CButtonContainer s_NameplatesShowPositionXMatchedColor;
		DoLine_ColorPicker(&s_NameplatesShowPositionXMatchedColor, s_ColorPickerLineSize, s_ColorPickerLabelSize, s_ColorPickerLineSpacing, &Column, RCLocalize("Position X matched color"), &g_Config.m_RcNameplatesShowPositionXMatchedColor, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowFinished, RCLocalize("Show finished"), &g_Config.m_RcNameplatesShowFinished, &Column, s_LineSize);
	Column.HSplitTop(s_LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesShowFinishedSize, &g_Config.m_RcNameplatesShowFinishedSize, &Button, RCLocalize("Finished size"), 0, 50);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowDummyCopyStatus, RCLocalize("Show dummy copy status"), &g_Config.m_RcNameplatesShowDummyCopyStatus, &Column, s_LineSize);
	Column.HSplitTop(s_LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesShowDummyCopyStatusSize, &g_Config.m_RcNameplatesShowDummyCopyStatusSize, &Button, RCLocalize("Dummy copy status size"), 0, 50);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowHammerFlyStatus, RCLocalize("Show hammer fly status"), &g_Config.m_RcNameplatesShowHammerFlyStatus, &Column, s_LineSize);
	Column.HSplitTop(s_LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesShowHammerFlyStatusSize, &g_Config.m_RcNameplatesShowHammerFlyStatusSize, &Button, RCLocalize("Hammer fly status size"), 0, 50);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesShowDummyResetStatus, RCLocalize("Show dummy reset status"), &g_Config.m_RcNameplatesShowDummyResetStatus, &Column, s_LineSize);
	Column.HSplitTop(s_LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesShowDummyResetStatusSize, &g_Config.m_RcNameplatesShowDummyResetStatusSize, &Button, RCLocalize("Dummy reset status size"), 0, 50);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHidden, RCLocalize("Range hidden"), &g_Config.m_RcNameplatesRangeHidden, &Column, s_LineSize);

	if(g_Config.m_RcNameplatesRangeHidden)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenName, RCLocalize("Range hidden name"), &g_Config.m_RcNameplatesRangeHiddenName, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenPoints, RCLocalize("Range hidden points"), &g_Config.m_RcNameplatesRangeHiddenPoints, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenPositionX, RCLocalize("Range hidden position X"), &g_Config.m_RcNameplatesRangeHiddenPositionX, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenFinished, RCLocalize("Range hidden finished"), &g_Config.m_RcNameplatesRangeHiddenFinished, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenStatus, RCLocalize("Range hidden status"), &g_Config.m_RcNameplatesRangeHiddenStatus, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenHookStatus, RCLocalize("Range hidden hook status"), &g_Config.m_RcNameplatesRangeHiddenHookStatus, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenLove, RCLocalize("Range hidden love"), &g_Config.m_RcNameplatesRangeHiddenLove, &Column, s_LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcNameplatesRangeHiddenOthers, RCLocalize("Range hidden others"), &g_Config.m_RcNameplatesRangeHiddenOthers, &Column, s_LineSize);

		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesRangeHiddenRadius, &g_Config.m_RcNameplatesRangeHiddenRadius, &Button, RCLocalize("Range hidden radius"), -10, 10);
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesRangeHiddenAngleStart, &g_Config.m_RcNameplatesRangeHiddenAngleStart, &Button, RCLocalize("Range hidden angle start"), 0, 360);
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcNameplatesRangeHiddenAngleEnd, &g_Config.m_RcNameplatesRangeHiddenAngleEnd, &Button, RCLocalize("Range hidden angle end"), 0, 360);
	}

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Fun
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Fun"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAutoChangeSkin, RCLocalize("Auto change skin"), &g_Config.m_RcAutoChangeSkin, &Column, s_LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcTalkingHidden, RCLocalize("Talking hidden"), &g_Config.m_RcTalkingHidden, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	RightView = Column;

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + s_MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

static int s_SelectedWeapon = -1;

static const char *s_apWeaponNames[] = {
	"Hammer", "Gun", "Shotgun", "Grenade", "Laser", "Ninja"};
static constexpr int s_NumWeaponNames = 6;

static const char *GetWeaponName(int WeaponID)
{
	if(WeaponID >= 0 && WeaponID < s_NumWeaponNames)
		return RCLocalize(s_apWeaponNames[WeaponID]);
	return "";
}

void CMenus::RenderRanbiWeaponsSettings(CUIRect MainView)
{
	static int s_LastSelected = -1;
	CUIRect LeftView, RightView, Label, Button;
	auto &vWeapons = GameClient()->m_RanbiClient.m_vWeapons;

	MainView.VSplitMid(&LeftView, &RightView, s_MarginBetweenViews);
	LeftView.VSplitLeft(s_MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(s_MarginSmall, &RightView, nullptr);

	CUIRect Column = LeftView;

	Column.HSplitTop(s_Margin, nullptr, &Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Weapon Settings"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	static CButtonContainer s_ReaderButtonSwitchRecentWeapon, s_ClearButtonSwitchRecentWeapon;
	DoLine_KeyReader(Column, s_ReaderButtonSwitchRecentWeapon, s_ClearButtonSwitchRecentWeapon, RCLocalize("Switch recent weapon"), "+switch_recent_weapon");

	Column.HSplitTop(s_MarginExtraSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcShowWeaponsAngle, RCLocalize("Show weapons angle"), &g_Config.m_RcShowWeaponsAngle, &Column, s_LineSize);

	if(g_Config.m_RcShowWeaponsAngle)
	{
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcWeaponsAngleInnerRadius, &g_Config.m_RcWeaponsAngleInnerRadius, &Button, RCLocalize("Weapons angle inner radius"), 0, 800);
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcWeaponsAngleRadius, &g_Config.m_RcWeaponsAngleRadius, &Button, RCLocalize("Weapons angle outer radius"), 0, 800);
		Column.HSplitTop(s_LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_RcWeaponsAngleFontSize, &g_Config.m_RcWeaponsAngleFontSize, &Button, RCLocalize("Weapons angle font size"), 6, 24);
	}

	Column.HSplitTop(s_MarginExtraSmall, nullptr, &Column);

	// Edit panel - uses independent s_EditWeapon, loaded from selected row on click
	static SWeaponInfo s_EditWeapon;
	static bool s_AngleInit = false;

	if(s_SelectedWeapon != s_LastSelected)
	{
		s_LastSelected = s_SelectedWeapon;
		if(s_SelectedWeapon >= 0 && s_SelectedWeapon < (int)vWeapons.size())
			s_EditWeapon = vWeapons[s_SelectedWeapon];
		s_AngleInit = false;
	}

	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Weapon Edit"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	static CUi::SDropDownState s_DropDownState;
	static CScrollRegion s_DropDownScrollRegion;
	s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
	CUIRect DropDownRect;
	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &DropDownRect, &Column);
	DropDownRect.VSplitMid(&Label, &DropDownRect);
	Ui()->DoLabel(&Label, RCLocalize("Weapon"), s_FontSize, TEXTALIGN_ML);
	const char *apLocalWeaponNames[] = {
		RCLocalize("Hammer"), RCLocalize("Gun"), RCLocalize("Shotgun"),
		RCLocalize("Grenade"), RCLocalize("Laser"), RCLocalize("Ninja")};
	const int WeaponSelectedNew = Ui()->DoDropDown(&DropDownRect, s_EditWeapon.m_WeaponID, apLocalWeaponNames, s_NumWeaponNames, s_DropDownState);
	if(WeaponSelectedNew != s_EditWeapon.m_WeaponID)
		s_EditWeapon.m_WeaponID = WeaponSelectedNew;

	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	static char s_aAngleMin[32];
	static char s_aAngleMax[32];
	if(!s_AngleInit || s_SelectedWeapon != s_LastSelected)
	{
		str_format(s_aAngleMin, sizeof(s_aAngleMin), "%.2f", s_EditWeapon.m_AngleStart);
		str_format(s_aAngleMax, sizeof(s_aAngleMax), "%.2f", s_EditWeapon.m_AngleEnd);
		s_AngleInit = true;
	}

	static CLineInput s_AngleMinInput(s_aAngleMin, sizeof(s_aAngleMin));
	static CLineInput s_AngleMaxInput(s_aAngleMax, sizeof(s_aAngleMax));

	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Button.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, RCLocalize("Angle start"), s_FontSize, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_AngleMinInput, &Button, s_EditBoxFontSize))
		s_EditWeapon.m_AngleStart = str_tofloat(s_aAngleMin);

	Column.HSplitTop(s_MarginExtraSmall, nullptr, &Column);

	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Button.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, RCLocalize("Angle end"), s_FontSize, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_AngleMaxInput, &Button, s_EditBoxFontSize))
		s_EditWeapon.m_AngleEnd = str_tofloat(s_aAngleMax);

	Column.HSplitTop(s_MarginExtraSmall, nullptr, &Column);

	static CButtonContainer s_WeaponColor;
	DoLine_ColorPicker(&s_WeaponColor, s_ColorPickerLineSize, s_ColorPickerLabelSize, s_ColorPickerLineSpacing, &Column, RCLocalize("Color"), &s_EditWeapon.m_Color, ColorRGBA(1.0f, 1.0f, 1.0f), false);

	Column.HSplitTop(s_MarginExtraSmall, nullptr, &Column);

	static CLineInput s_NoteInput(s_EditWeapon.m_aNote, sizeof(s_EditWeapon.m_aNote));
	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Button.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, RCLocalize("Note"), s_FontSize, TEXTALIGN_ML);
	Ui()->DoEditBox(&s_NoteInput, &Button, s_EditBoxFontSize);

	Column.HSplitTop(s_Margin, nullptr, &Column);

	static CButtonContainer s_AddButton, s_ModifyButton, s_DeleteButton;
	Column.HSplitTop(s_LineSize, &Button, &Column);
	CUIRect BtnAdd, BtnModify, BtnDelete;
	float BtnW = (Button.w - s_MarginSmall * 2.0f) / 3.0f;
	Button.VSplitLeft(BtnW, &BtnAdd, &Button);
	Button.VSplitLeft(s_MarginSmall, nullptr, &Button);
	Button.VSplitLeft(BtnW, &BtnModify, &Button);
	Button.VSplitLeft(s_MarginSmall, nullptr, &Button);
	BtnDelete = Button;

	if(DoButton_Menu(&s_AddButton, RCLocalize("Add"), 0, &BtnAdd))
	{
		vWeapons.push_back(s_EditWeapon);
		s_SelectedWeapon = (int)vWeapons.size() - 1;
		s_EditWeapon = SWeaponInfo{};
		s_AngleInit = false;
	}
	if(DoButton_Menu(&s_ModifyButton, RCLocalize("Modify"), 0, &BtnModify) && s_SelectedWeapon >= 0)
	{
		vWeapons[s_SelectedWeapon] = s_EditWeapon;
		s_LastSelected = -1;
	}
	if(DoButton_Menu(&s_DeleteButton, RCLocalize("Delete"), 0, &BtnDelete) && s_SelectedWeapon >= 0)
	{
		vWeapons.erase(vWeapons.begin() + s_SelectedWeapon);
		s_SelectedWeapon = -1;
		s_LastSelected = -1;
		s_EditWeapon = SWeaponInfo{};
		s_AngleInit = false;
	}

	// Right: weapon list
	Column = RightView;

	CUIRect ListBox;
	Column.HSplitTop(s_Margin, nullptr, &Column);
	ListBox = Column;

	float HeaderH = s_LineSize + s_MarginExtraSmall;
	float RowH = s_LineSize + s_MarginExtraSmall;
	float ColWeapon = 90.0f;
	float ColAngle = 170.0f;
	float ColColor = 50.0f;

	CUIRect Header, Row;
	ListBox.HSplitTop(HeaderH, &Header, &ListBox);

	Header.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_T, 5.0f);

	float ColSpacing = s_FontSize * 0.5f;

	CUIRect Cell;
	Header.VSplitLeft(ColWeapon, &Cell, &Header);
	Header.VSplitLeft(ColSpacing, nullptr, &Header);
	Ui()->DoLabel(&Cell, RCLocalize("Weapon"), s_FontSize, TEXTALIGN_ML);
	Header.VSplitLeft(ColAngle, &Cell, &Header);
	Header.VSplitLeft(ColSpacing, nullptr, &Header);
	Ui()->DoLabel(&Cell, RCLocalize("Angle range"), s_FontSize, TEXTALIGN_ML);
	Header.VSplitLeft(ColColor, &Cell, &Header);
	Header.VSplitLeft(ColSpacing, nullptr, &Header);
	Ui()->DoLabel(&Cell, RCLocalize("Color"), s_FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&Header, RCLocalize("Note"), s_FontSize, TEXTALIGN_ML);

	// Build sorted indices: group by WeaponID, then sort by AngleStart within each group
	static std::vector<int> s_SortedIndices;
	s_SortedIndices.resize(vWeapons.size());
	for(int i = 0; i < (int)vWeapons.size(); i++)
		s_SortedIndices[i] = i;
	std::sort(s_SortedIndices.begin(), s_SortedIndices.end(), [&vWeapons](int a, int b) {
		if(vWeapons[a].m_WeaponID != vWeapons[b].m_WeaponID)
			return vWeapons[a].m_WeaponID < vWeapons[b].m_WeaponID;
		return vWeapons[a].m_AngleStart < vWeapons[b].m_AngleStart;
	});

	for(int si = 0; si < (int)s_SortedIndices.size(); si++)
	{
		int i = s_SortedIndices[si];
		ListBox.HSplitTop(RowH, &Row, &ListBox);

		static std::vector<CButtonContainer> s_aListButtons;
		if((int)s_aListButtons.size() <= si)
			s_aListButtons.resize(si + 1);
		if(Ui()->DoButtonLogic(&s_aListButtons[si], 0, &Row, BUTTONFLAG_LEFT))
			s_SelectedWeapon = i;

		if(s_SelectedWeapon == i)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
		else if(si % 2 == 0)
			Row.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f), IGraphics::CORNER_NONE, 0.0f);

		char aBuf[64];
		Row.VSplitLeft(ColWeapon, &Cell, &Row);
		Row.VSplitLeft(ColSpacing, nullptr, &Row);
		Ui()->DoLabel(&Cell, GetWeaponName(vWeapons[i].m_WeaponID), s_FontSize, TEXTALIGN_ML);

		Row.VSplitLeft(ColAngle, &Cell, &Row);
		Row.VSplitLeft(ColSpacing, nullptr, &Row);
		str_format(aBuf, sizeof(aBuf), "%.2f~%.2f", vWeapons[i].m_AngleStart, vWeapons[i].m_AngleEnd);
		Ui()->DoLabel(&Cell, aBuf, s_FontSize, TEXTALIGN_ML);

		Row.VSplitLeft(ColColor, &Cell, &Row);
		Row.VSplitLeft(ColSpacing, nullptr, &Row);
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(vWeapons[i].m_Color, false));
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Col.r, Col.g, Col.b, 1.0f);
		IGraphics::CQuadItem QuadColor(Cell.x + 5.0f, Cell.y + 5.0f, Cell.h - 10.0f, Cell.h - 10.0f);
		Graphics()->QuadsDrawTL(&QuadColor, 1);
		Graphics()->QuadsEnd();

		Ui()->DoLabel(&Row, vWeapons[i].m_aNote, s_FontSize, TEXTALIGN_ML);
	}

	if(!vWeapons.empty())
		ListBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
}

void CMenus::RenderRanbiDDNetMore(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;

	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, s_MarginBetweenViews);
	LeftView.VSplitLeft(s_MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(s_MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = s_MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	CUIRect Column = LeftView;
	CUIRect Label;

	Column.HSplitTop(s_Margin, nullptr, &Column);
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("DDNet fix"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcHammerFix, RCLocalize("Hammer fix"), &g_Config.m_RcHammerFix, &Column, s_LineSize);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	RightView = Column;

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + s_MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderRanbiInfo(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;

	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, s_MarginBetweenViews);
	LeftView.VSplitLeft(s_MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(s_MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = s_MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	CUIRect Column = LeftView;
	CUIRect Label, Button;

	Column.HSplitTop(s_Margin, nullptr, &Column);
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Ranbi Client"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	Column.HSplitTop(s_LineSize, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Based on TClient (TaterClient)."), s_FontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);
	Column.HSplitTop(s_LineSize, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Upstream: DDNet / Teeworlds."), s_FontSize, TEXTALIGN_ML);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	LeftView = Column;
	Column = RightView;

	Column.HSplitTop(s_Margin, nullptr, &Column);
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Links"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	static CButtonContainer s_GithubButton;
	Column.HSplitTop(s_LineSize, &Button, &Column);
	if(DoButton_Menu(&s_GithubButton, "GitHub", 0, &Button))
		Client()->ViewLink("https://github.com/Moistrocic/RanbiClient");
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	RightView = Column;

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + s_MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderRanbi(CUIRect MainView)
{
	static int s_CurTab = 0;

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(20.0f, &MainView);

	CUIRect TabBar, Button;
	MainView.HSplitTop(s_LineSize, &TabBar, &MainView);

	const float TabWidth = TabBar.w / NUMBER_OF_RANBI_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_RANBI_TABS] = {};
	const char *apTabNames[] = {
		RCLocalize("Settings"),
		RCLocalize("Weapons Settings"),
		RCLocalize("More DDNet"),
		RCLocalize("Info")};

	for(int Tab = 0; Tab < NUMBER_OF_RANBI_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == NUMBER_OF_RANBI_TABS - 1 ? IGraphics::CORNER_R :
												       IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurTab = Tab;
	}

	MainView.HSplitTop(s_Margin, nullptr, &MainView);

	if(s_CurTab == RANBI_TAB_SETTINGS)
		RenderRanbiSettings(MainView);
	else if(s_CurTab == RANBI_TAB_WEAPONS_SETTINGS)
		RenderRanbiWeaponsSettings(MainView);
	else if(s_CurTab == RANBI_TAB_DDNET_MORE)
		RenderRanbiDDNetMore(MainView);
	else if(s_CurTab == RANBI_TAB_INFO)
		RenderRanbiInfo(MainView);
}
