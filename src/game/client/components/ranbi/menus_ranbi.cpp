#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <vector>

static constexpr int RANBI_TAB_MAIN = 0;
static constexpr int RANBI_TAB_INFO = 1;
static constexpr int NUMBER_OF_RANBI_TABS = 2;

static const float s_FontSize = 14.0f;
static const float s_LineSize = 20.0f;
static const float s_HeadlineFontSize = 20.0f;
static const float s_HeadlineHeight = s_HeadlineFontSize + 0.0f;
static const float s_Margin = 10.0f;
static const float s_MarginSmall = 5.0f;
static const float s_MarginBetweenSections = 30.0f;
static const float s_MarginBetweenViews = 30.0f;

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
		"Ranbi Client",
		"Info"};

	for(int Tab = 0; Tab < NUMBER_OF_RANBI_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == NUMBER_OF_RANBI_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurTab = Tab;
	}

	MainView.HSplitTop(s_Margin, nullptr, &MainView);

	if(s_CurTab == RANBI_TAB_MAIN)
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
		Ui()->DoLabel(&Label, "Ranbi Client", s_HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);

		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Welcome to Ranbi Client!", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "A modified DDNet game client.", s_FontSize, TEXTALIGN_ML);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, "Features", s_HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);

		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Custom client-side modifications", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Enhanced gameplay experience", s_FontSize, TEXTALIGN_ML);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		LeftView = Column;
		Column = RightView;

		Column.HSplitTop(s_Margin, nullptr, &Column);
		Column.HSplitTop(s_Margin, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, "Info", s_HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);

		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Based on TClient (TaterClient).", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Upstream: DDNet / Teeworlds.", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Client-side modification only.", s_FontSize, TEXTALIGN_ML);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		s_ScrollRegion.End();
	}
	else if(s_CurTab == RANBI_TAB_INFO)
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
		Ui()->DoLabel(&Label, "Project", s_HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);

		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Project: RanbiClient", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Fork: TClient (TaterClient)", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Upstream: DDNet / Teeworlds", s_FontSize, TEXTALIGN_ML);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		LeftView = Column;
		Column = RightView;

		Column.HSplitTop(s_Margin, nullptr, &Column);
		Column.HSplitTop(s_Margin, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, "Description", s_HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);

		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Client-side modification only.", s_FontSize, TEXTALIGN_ML);
		Column.HSplitTop(s_MarginSmall, nullptr, &Column);
		Column.HSplitTop(s_LineSize, &Label, &Column);
		Ui()->DoLabel(&Label, "Cross-platform client.", s_FontSize, TEXTALIGN_ML);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		s_ScrollRegion.End();
	}
}
