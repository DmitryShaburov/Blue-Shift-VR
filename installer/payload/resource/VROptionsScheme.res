// Scheme of the VR Options dialog (cl_dll/vr/VRClientVGUI.cpp).
// The game's dialogs use resource/TrackerScheme.res, which names its colors the GoldSrc way ("FgColor", "TitleBarFgColor", ...).
// The dialog's controls (hl1_source_sdk vgui_controls) look them up with Source 2013 names ("Frame.BgColor", ...), so this
// file takes TrackerScheme.res as its base and adds those names, pointing to its colors. It also adds the borders they use
// that TrackerScheme.res doesn't have, drawn like its RaisedBorder.
// The dialog uses scaled fonts, which the engine scales from a 480 high base while the game's dialogs scale from 720 (the "HD"
// base below), so the fonts the controls use are TrackerScheme.res's sizes times 480/720.
#base "TrackerScheme.res"

Scheme
{
	BaseSettings
	{
		"Panel.FgColor"						"ControlText"
		"Panel.BgColor"						"ControlBG"

		"Frame.BgColor"						"ControlBG"
		"Frame.OutOfFocusBgColor"			"ControlBG"

		"FrameTitleBar.TextColor"			"TitleText"
		"FrameTitleBar.BgColor"				"TitleBG"
		"FrameTitleBar.DisabledTextColor"	"TitleDimText"
		"FrameTitleBar.DisabledBgColor"		"TitleDimBG"

		"FrameTitleButton.FgColor"			"BorderBright"
		"FrameTitleButton.BgColor"			"ControlBG"
		"FrameTitleButton.DisabledFgColor"	"TitleDimText"
		"FrameTitleButton.DisabledBgColor"	"TitleDimBG"

		// the title bar icon: TrackerScheme.res "TitleBarIcon" / "TitleBarDisabledIcon", on a transparent button
		"FrameSystemButton.Icon"			"resource/icon_steam"
		"FrameSystemButton.DisabledIcon"	"resource/icon_steam_disabled"
		"FrameSystemButton.FgColor"			"0 0 0 0"
		"FrameSystemButton.BgColor"			"0 0 0 0"

		"FrameGrip.Color1"					"BorderBright"
		"FrameGrip.Color2"					"BorderDark"

		"Label.TextColor"					"ControlText"
		"Label.BgColor"						"0 0 0 0"
		"Label.TextBrightColor"				"BrightBaseText"
		"Label.TextDullColor"				"DimBaseText"
		"Label.SelectedTextColor"			"SelectedText"
		"Label.DisabledFgColor1"			"DisabledText1"
		"Label.DisabledFgColor2"			"DisabledText2"

		"Button.TextColor"					"ControlText"
		"Button.BgColor"					"ControlBG"
		"Button.ArmedTextColor"				"ControlText"
		"Button.ArmedBgColor"				"ControlBG"
		"Button.DepressedTextColor"			"BrightControlText"
		"Button.DepressedBgColor"			"ControlBG"
		"Button.SelectedTextColor"			"BrightControlText"
		"Button.SelectedBgColor"			"ControlBG"
		"Button.FocusBorderColor"			"BorderSelection"
		"Button.BlinkColor"					"BrightControlText"

		// checkboxes: TrackerScheme.res "CheckButtonBorder1/2", "CheckButtonCheck", "CheckBgColor"
		"CheckButton.TextColor"				"ControlText"
		"CheckButton.SelectedTextColor"		"ControlText"
		"CheckButton.HighlightFgColor"		"BrightBaseText"
		"CheckButton.BgColor"				"ListBG"
		"CheckButton.Border1"				"BorderDark"
		"CheckButton.Border2"				"BorderBright"
		"CheckButton.Check"					"BrightControlText"
		"CheckButton.DisabledFgColor"		"DisabledText1"
		"CheckButton.DisabledBgColor"		"ControlBG"
		"CheckButton.ArmedBgColor"			"ControlBG"
		"CheckButton.DepressedBgColor"		"ControlBG"

		"ComboBoxButton.ArrowColor"			"DimBaseText"
		"ComboBoxButton.ArmedArrowColor"	"BrightBaseText"
		"ComboBoxButton.BgColor"			"WindowBG"
		"ComboBoxButton.DisabledBgColor"	"WindowBG"

		"TextEntry.TextColor"				"BaseText"
		"TextEntry.BgColor"					"WindowBG"
		"TextEntry.CursorColor"				"BaseText"
		"TextEntry.DisabledTextColor"		"DimBaseText"
		"TextEntry.DisabledBgColor"			"ListBG"
		"TextEntry.SelectedTextColor"		"SelectedText"
		"TextEntry.SelectedBgColor"			"SelectionBG"
		"TextEntry.OutOfFocusSelectedBgColor"	"SelectionBG2"
		"TextEntry.FocusEdgeColor"			"BorderDark"

		"Menu.TextColor"					"DimBaseText"
		"Menu.BgColor"						"ControlBG"
		"Menu.ArmedTextColor"				"BrightBaseText"
		"Menu.ArmedBgColor"					"SelectionBG"
		"Menu.SeparatorColor"				"BorderDark"

		"PropertySheet.TextColor"			"ControlText"
		"PropertySheet.SelectedTextColor"	"BrightControlText"

		"Slider.NobColor"					"ControlBG"
		"Slider.NobFocusColor"				"BorderBright"
		"Slider.TextColor"					"ControlText"
		"Slider.TrackColor"					"SliderTrackColor"
		"Slider.DisabledTextColor1"			"DisabledText1"
		"Slider.DisabledTextColor2"			"DisabledText2"

		"ScrollBarButton.FgColor"			"DimBaseText"
		"ScrollBarButton.BgColor"			"ControlBG"
		"ScrollBarButton.ArmedFgColor"		"BrightBaseText"
		"ScrollBarButton.ArmedBgColor"		"ControlBG"
		"ScrollBarButton.DepressedFgColor"	"BrightBaseText"
		"ScrollBarButton.DepressedBgColor"	"ControlBG"

		"ScrollBarSlider.FgColor"			"ControlBG"
		"ScrollBarSlider.BgColor"			"ControlDarkBG"

		"Tooltip.TextColor"					"BaseText"
		"Tooltip.BgColor"					"SelectionBG2"
	}

	Fonts
	{
		"Default"			{ "1" { "tall" "11" } }	// 16
		"DefaultSmall"		{ "1" { "tall" "9" } }	// 13
		"DefaultVerySmall"	{ "1" { "tall" "8" } }	// 12
		"Marlett"			{ "1" { "tall" "9" } }	// 14
	}

	Borders
	{
		Default
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		FrameBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		ButtonBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		MenuBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		PropertySheetBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		TabBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		ScrollBarSliderBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}
	}
}
