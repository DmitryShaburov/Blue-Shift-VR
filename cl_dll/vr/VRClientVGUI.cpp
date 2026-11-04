// VGUI2 in the client. The engine hands the client its VGUI2 interfaces through IClientVGUI, and the VR Options dialog
// is a VGUI2 window inside the game's own menu, with a tab for each page of VR settings (VRSettingsPages.h).
// Built with the VGUI2 SDK's headers (external/hl1_source_sdk) instead of the game's, see linux/Makefile.hl_cdll.

#include <algorithm>
#include <string>
#include <vector>

#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/tier1.h>
#include <tier1/KeyValues.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <IBaseUI.h>
#include <IClientVGUI.h>
#include <IEngineVGui.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/PropertyDialog.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>

#include "VRClientVGUI.h"
#include "VRSettingsPages.h"

namespace
{
// Why VGUI2 can't be used, nullptr once it's ready
const char* g_vguiError = "the engine didn't initialize the client's VGUI2 (IClientVGUI::Initialize wasn't called)";

// The settings' texts are UTF-8 (e.g. the degree sign), VGUI2 shows wide text
std::wstring ToWide(const std::string& text)
{
	std::vector<wchar_t> buffer(text.size() + 1); // never more wide characters than UTF-8 bytes
	V_UTF8ToUnicode(text.c_str(), buffer.data(), (int)(buffer.size() * sizeof(wchar_t)));
	return buffer.data();
}

// Scheme of the dialog: the game's TrackerScheme.res plus the color names our controls use, see the file.
// The SDK's scheme loader quits the game if the file is missing, so without it fall back to the game's scheme (it then looks unstyled).
constexpr const char* OptionsSchemeFile = "resource/VROptionsScheme.res";

// The game's dialogs scale with the screen from a 1280x720 base (TrackerScheme.res "ProportionalBaseWidthHD"), the engine's
// own base is 640x480. vgui_controls scale with this one instead, sizes here are in its units, taken from the game's Options
// dialog and its resource/OptionsSubVideo.res. Fonts are scaled by the engine, see resource/VROptionsScheme.res.
void GetProportionalBase(int& wide, int& tall)
{
	wide = 1280;
	tall = 720;
}

int Scaled(int value)
{
	return vgui2::scheme()->GetProportionalScaledValue(value);
}

vgui2::HScheme GetOptionsScheme()
{
	static vgui2::HScheme scheme = 0;
	if (scheme == 0 && g_pFullFileSystem->FileExists(OptionsSchemeFile))
		scheme = vgui2::scheme()->LoadSchemeFromFile(OptionsSchemeFile, "VROptionsScheme");
	if (scheme == 0)
		scheme = vgui2::scheme()->GetScheme("BaseUI"); // TrackerScheme.res, loaded by the engine
	return scheme;
}

// One tab: a row for each setting, a label on the left and a combo box (choice) or slider (stepper) on the right.
// Changes apply at once.
class CVROptionsPage : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CVROptionsPage, vgui2::PropertyPage);

public:
	// The rows are made when the page is first shown (OnPageShow), not here: the dialog only scales its children once
	// all pages are added, and the first page is made ready for use as it's added, so rows made now keep unscaled fonts.
	CVROptionsPage(vgui2::Panel* parent, int page) : BaseClass(parent, nullptr), m_page(page)
	{
	}

	void OnPageShow() override
	{
		BaseClass::OnPageShow();
		// first show, or the values may have been changed elsewhere (console)
		if (ItemsChanged())
			Rebuild();
		else
			RefreshValues();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		if (m_needsRebuild)
		{
			m_needsRebuild = false;
			Rebuild();
		}
	}

	// Laid out like the game's Options tabs (resource/OptionsSub*.res, in base units): labels right-aligned up to 20
	// before the control column, dropdowns 180 wide (Video), sliders with a value box after them (Video brightness, Aim
	// sensitivity), and checkboxes carrying their own text in the control column (Video). The label column is wider for
	// our longer names.
	void PerformLayout() override
	{
		BaseClass::PerformLayout();

		const int LabelX = Scaled(24);
		const int ControlX = Scaled(220);
		const int Gap = Scaled(8);
		const int LabelGap = Scaled(20);
		const int RowHeight = Scaled(24);
		const int ComboWidth = Scaled(180);
		const int SliderWidth = Scaled(196);
		const int SliderHeight = Scaled(32);
		const int ValueWidth = Scaled(56);
		const int CheckWidth = Scaled(270);

		int y = Scaled(14);
		for (Row& row : m_rows)
		{
			if (row.check)
			{
				row.check->SetBounds(ControlX - Scaled(4), y, CheckWidth, RowHeight); // the box sits in front of the column
				y += Scaled(26);
				continue;
			}
			row.label->SetBounds(LabelX, y, ControlX - LabelX - LabelGap, RowHeight);
			if (row.combo)
			{
				row.combo->SetBounds(ControlX, y, ComboWidth, RowHeight);
				y += Scaled(32);
			}
			else
			{
				row.slider->SetBounds(ControlX, y, SliderWidth, SliderHeight);
				row.value->SetBounds(ControlX + SliderWidth + Gap, y, ValueWidth, RowHeight);
				y += Scaled(38);
			}
		}
	}

private:
	struct Row
	{
		VRSettingItem item;
		vgui2::Label* label = nullptr;		 // Choice, Stepper: the name in the label column
		vgui2::CheckButton* check = nullptr; // On/Off choice, with the name as its text
		int onIndex = 1;					 // On/Off choice: the option index of "On"
		vgui2::ComboBox* combo = nullptr;	 // other choices
		vgui2::Slider* slider = nullptr;	 // Stepper
		vgui2::Label* value = nullptr;		 // Stepper: the value as text

		template <typename Function>
		void ForEachPanel(Function function) const
		{
			for (vgui2::Panel* panel : {(vgui2::Panel*)label, (vgui2::Panel*)check, (vgui2::Panel*)combo, (vgui2::Panel*)slider, (vgui2::Panel*)value})
			{
				if (panel)
					function(panel);
			}
		}
	};

	// An On/Off choice (in either order) is shown as a checkbox
	static bool IsOnOff(const VRSettingItem& item, int& onIndex)
	{
		if (item.type != VRSettingItem::Type::Choice || item.options.size() != 2)
			return false;
		for (int i = 0; i < 2; i++)
		{
			if (item.options[i] == "On" && item.options[1 - i] == "Off")
			{
				onIndex = i;
				return true;
			}
		}
		return false;
	}

	static int StepCount(const VRSettingItem& item)
	{
		return item.step > 0.f ? (int)((item.max - item.min) / item.step + 0.5f) : 0;
	}

	static void SetValueText(Row& row, float value)
	{
		char text[64];
		snprintf(text, sizeof(text), "%.0f%s", value * row.item.displayScale + 0.f, row.item.suffix.c_str()); // + 0 turns -0 into 0
		row.value->SetText(ToWide(text).c_str());
	}

	// Sets the row's control to the setting's current value. Combo boxes and sliders don't send a change message for it.
	// A checkbox does, so it's only set when it changes, its message then sets the value the setting already has.
	static void ShowValue(Row& row)
	{
		const float current = row.item.getValue ? row.item.getValue() : 0.f;
		const int index = (int)(current + 0.5f);
		if (row.check)
		{
			const bool selected = index == row.onIndex;
			if (row.check->IsSelected() != selected)
				row.check->SetSelected(selected);
		}
		else if (row.combo)
		{
			if (index >= 0 && index < (int)row.item.options.size())
				row.combo->SilentActivateItemByRow(index);
		}
		else if (row.slider)
		{
			row.slider->SetValue((int)((current - row.item.min) / row.item.step + 0.5f), false);
			SetValueText(row, current);
		}
	}

	void RefreshValues()
	{
		for (Row& row : m_rows)
			ShowValue(row);
	}

	void Rebuild()
	{
		for (const Row& row : m_rows)
			row.ForEachPanel([](vgui2::Panel* panel)
				{
					panel->MarkForDeletion();
				});
		m_rows.clear();

		for (VRSettingItem& item : VRGetSettingsPageItems(m_page))
		{
			Row row;
			row.item = std::move(item);

			if (IsOnOff(row.item, row.onIndex))
			{
				row.check = new vgui2::CheckButton(this, nullptr, "");
				row.check->SetText(ToWide(row.item.label).c_str());
			}
			else
			{
				row.label = new vgui2::Label(this, nullptr, ToWide(row.item.label).c_str());
				row.label->SetContentAlignment(vgui2::Label::a_east);
			}

			if (row.check == nullptr && row.item.type == VRSettingItem::Type::Choice)
			{
				row.combo = new vgui2::ComboBox(this, nullptr, (int)row.item.options.size(), false);
				for (size_t i = 0; i < row.item.options.size(); i++)
				{
					KeyValues* data = new KeyValues("option", "index", (int)i);
					row.combo->AddItem(ToWide(row.item.options[i]).c_str(), data);
					data->deleteThis();
				}
			}
			else if (row.check == nullptr)
			{
				const int steps = StepCount(row.item);
				row.slider = new vgui2::Slider(this, nullptr);
				row.slider->SetRange(0, steps);
				row.slider->SetNumTicks((std::min)(steps, 10)); // tick marks like the game's sliders
				row.slider->SetDragOnRepositionNob(true);
				row.value = new vgui2::Label(this, nullptr, "");
			}

			// the change messages only come once this page is a target, so setting the start value sends nothing
			ShowValue(row);
			for (vgui2::Panel* control : {(vgui2::Panel*)row.check, (vgui2::Panel*)row.combo, (vgui2::Panel*)row.slider})
			{
				if (control)
					control->AddActionSignalTarget(this);
			}

			row.ForEachPanel([&](vgui2::Panel* panel)
				{
					panel->SetProportional(IsProportional()); // new panels don't take this from their parent: scaled fonts
				});
			m_rows.push_back(std::move(row));
		}
		InvalidateLayout();
	}

	MESSAGE_FUNC_PARAMS(OnCheckButtonChecked, "CheckButtonChecked", data)
	{
		vgui2::Panel* panel = (vgui2::Panel*)data->GetPtr("panel");
		for (Row& row : m_rows)
		{
			if (row.check == nullptr || row.check != panel)
				continue;
			if (row.item.setValue)
				row.item.setValue((float)(0 != data->GetInt("state") ? row.onIndex : 1 - row.onIndex));
			RebuildIfItemsChanged();
			return;
		}
	}

	// Some settings show or hide others
	bool ItemsChanged() const
	{
		const std::vector<VRSettingItem> items = VRGetSettingsPageItems(m_page);
		if (items.size() != m_rows.size())
			return true;
		for (size_t i = 0; i < items.size(); i++)
		{
			if (items[i].label != m_rows[i].item.label)
				return true;
		}
		return false;
	}

	// Rebuild the page later, not while the control that changed is sending its message
	void RebuildIfItemsChanged()
	{
		if (ItemsChanged())
			m_needsRebuild = true;
	}

	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel)
	{
		for (Row& row : m_rows)
		{
			if (row.combo == nullptr || row.combo != panel)
				continue;
			KeyValues* data = row.combo->GetActiveItemUserData();
			if (data && row.item.setValue)
				row.item.setValue((float)data->GetInt("index"));
			RebuildIfItemsChanged();
			return;
		}
	}

	MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", data)
	{
		vgui2::Panel* panel = (vgui2::Panel*)data->GetPtr("panel");
		for (Row& row : m_rows)
		{
			if (row.slider == nullptr || row.slider != panel)
				continue;
			const float value = row.item.min + data->GetInt("position") * row.item.step;
			if (row.item.setValue)
				row.item.setValue(value);
			SetValueText(row, value);
			RebuildIfItemsChanged();
			return;
		}
	}

	int m_page;
	std::vector<Row> m_rows;
	bool m_needsRebuild = false;
};

class CVROptionsDialog : public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(CVROptionsDialog, vgui2::PropertyDialog);

public:
	CVROptionsDialog() : BaseClass(nullptr, "VROptionsDialog")
	{
		// A child of the game's menu, so it shows and hides with it
		SetParent(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL));
		// Set before the controls are made, they take their parent's scheme
		SetScheme(GetOptionsScheme());
		SetDeleteSelfOnClose(true);
		SetSizeable(false);
		SetBounds(0, 0, Scaled(512), Scaled(430)); // the game's Options dialog is 512x406, a bit taller for our longest page

		// vgui_controls' Frame draws its title bar and its icon at fixed pixel sizes, the game's dialogs scale them.
		// So the frame's own title and icon are off and these are laid out scaled instead (PerformLayout, GetClientArea).
		SetTitle("", false);
		SetMenuButtonVisible(false);
		m_titleIcon = new vgui2::ImagePanel(this, "TitleIcon");
		m_titleIcon->SetShouldScaleImage(true);
		m_titleIcon->SetMouseInputEnabled(false);
		m_titleLabel = new vgui2::Label(this, "TitleLabel", "VR Options");
		m_titleLabel->SetMouseInputEnabled(false); // dragging the title bar moves the dialog

		const std::vector<std::string> pages = VRGetSettingsPageNames();
		for (size_t i = 0; i < pages.size(); i++)
			AddPage(new CVROptionsPage(this, (int)i), pages[i].c_str());

		// changes apply at once, so there is nothing to cancel or apply
		SetOKButtonText("Close");
		SetCancelButtonVisible(false);
		SetApplyButtonVisible(false);

		// scaled fonts and layout, like the game's dialogs. Applies to all panels made so far, the pages pass it on to rows they make later.
		SetProportional(true);
	}

protected:
	void ApplySchemeSettings(vgui2::IScheme* scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		m_titleIcon->SetImage(vgui2::scheme()->GetImage(scheme->GetResourceString("FrameSystemButton.Icon"), false));
	}

	// Below the scaled title bar, with the game's dialog's margins (base units)
	void GetClientArea(int& x, int& y, int& wide, int& tall) override
	{
		GetSize(wide, tall);
		x = Scaled(6);
		y = Scaled(30);
		wide -= 2 * x;
		tall -= y + Scaled(6);
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();

		m_titleIcon->SetBounds(Scaled(8), Scaled(7), Scaled(16), Scaled(16));
		m_titleLabel->SetBounds(Scaled(30), Scaled(5), GetWide() - Scaled(80), Scaled(20));

		// The area that drags the dialog covers the whole scaled title bar (Frame makes it 23 pixels tall).
		// The close button, which Frame already scales, stays on top of it.
		if (vgui2::Panel* caption = FindChildByName("frame_caption"))
			caption->SetBounds(Scaled(5), Scaled(2), GetWide() - Scaled(10), Scaled(26));

		// the dialog's buttons at the game's size: 72x24 in the bottom right corner (PropertyDialog uses those numbers unscaled)
		int x, y, wide, tall;
		GetClientArea(x, y, wide, tall);
		if (vgui2::Panel* ok = FindChildByName("OKButton"))
			ok->SetBounds(x + wide - Scaled(80), y + tall - Scaled(28), Scaled(72), Scaled(24));
	}

private:
	vgui2::ImagePanel* m_titleIcon;
	vgui2::Label* m_titleLabel;
};

vgui2::DHANDLE<CVROptionsDialog> g_optionsDialog;

class CVRClientVGUI : public IClientVGUI
{
public:
	void Initialize(CreateInterfaceFn* factories, int count) override
	{
		ConnectTier1Libraries(factories, count);
		ConnectTier2Libraries(factories, count);
		vgui2::VGui_SetProportionalBaseCallback(&GetProportionalBase);
		if (!vgui2::VGui_InitInterfacesList("CLIENT", factories, count))
			g_vguiError = "the engine's VGUI2 interfaces weren't found";
		else if (g_pEngineVGui == nullptr)
			g_vguiError = "the engine's VEngineVGui001 interface wasn't found";
		else
			g_vguiError = nullptr;
	}

	void Start() override {}
	void SetParent(vgui2::VPANEL parent) override {}

	// The client's HUD menus (scoreboard, spectator, ...) are still VGUI1
	int UseVGUI1() override { return 1; }

	void HideScoreBoard() override {}
	void HideAllVGUIMenu() override {}
	void ActivateClientUI() override {}
	void HideClientUI() override {}
	void Shutdown() override {}
};

CVRClientVGUI g_clientVGUI;
}

bool VRActivateGameMenu()
{
	if (g_pBaseUI == nullptr)
		return false;
	g_pBaseUI->ActivateGameUI();
	return true;
}

// vgui_controls asks the module using it for the scheme new panels get. None: panels then use their parent's,
// the dialog sets the menu's. Loading a scheme file here isn't an option, the SDK's loader quits the game when the file doesn't exist.
vgui2::HScheme vgui2::VGui_GetDefaultScheme()
{
	return 0;
}

void* VRGetClientVGUI()
{
	return static_cast<IClientVGUI*>(&g_clientVGUI);
}

const char* VROpenOptionsDialog()
{
	if (g_vguiError)
		return g_vguiError;

	// The game's menu closes itself before it runs an "engine" command (and resumes the game), and the dialog is inside
	// the menu, so open the menu again. This runs in the same frame the menu closed, before anything is drawn.
	VRActivateGameMenu();

	if (!g_optionsDialog.Get())
	{
		g_optionsDialog = new CVROptionsDialog();
		g_optionsDialog->MoveToCenterOfScreen();
	}
	g_optionsDialog->Activate();
	return nullptr;
}
