
#include "vr_gl.h"

#include "VRCommon.h"
#include "VRGUIRenderer.h"
#include "VRTextureHelper.h"
#include "VRGameFunctions.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
// Buttons click when the pointer button is released. The trigger is also the fire button, so when a click closes the menu
// the trigger must not still be held down, or the weapon would fire.
#define NK_BUTTON_TRIGGER_ON_RELEASE

#pragma warning(push)
#pragma warning(disable: 4996)  // vsprintf and fopen used by nuklear.h give deprecation warning
#include "nuklear.h"
#pragma warning(pop)

namespace
{
    // would be nicer to have these as a members of VRGUIRenderer,
    // but with HL's include hell, nopenopenope
    struct nk_context ctx;
    struct nk_buffer cmds;
    struct nk_draw_null_texture nulltexture;
    struct nk_font_atlas atlas;
    unsigned int font_tex;
    struct nk_font* font_heading = nullptr;
    struct nk_font* font_text = nullptr;
    struct nk_font* font_text_smol = nullptr;

    struct nk_ogl_vertex
    {
        float position[2];
        float uv[2];
        nk_byte col[4];
    };

    void SafeSetFont(nk_font* font)
    {
        if (font)
        {
            nk_style_set_font(&ctx, &font->handle);
        }
        else if (font_text)
        {
            nk_style_set_font(&ctx, &font_text->handle);
        }
        else if (atlas.default_font)
        {
            nk_style_set_font(&ctx, &atlas.default_font->handle);
        }
        else
        {
            VRGameFunctions::PrintToConsole("SetFont failed: No font!\n");
        }
    }

// Colors follow the classic Steam / VGUI2 olive skin
#define HLVR_COLOR_MAP(NK_COLOR)\
    NK_COLOR(NK_COLOR_TEXT,                     216, 222, 211, 255) \
    NK_COLOR(NK_COLOR_WINDOW,                   62, 70, 55, 255) \
    NK_COLOR(NK_COLOR_HEADER,                   40, 46, 34, 255) \
    NK_COLOR(NK_COLOR_BORDER,                   136, 145, 128, 255) \
    NK_COLOR(NK_COLOR_BUTTON,                   76, 88, 68, 255) \
    NK_COLOR(NK_COLOR_BUTTON_HOVER,             90, 104, 80, 255) \
    NK_COLOR(NK_COLOR_BUTTON_ACTIVE,            40, 46, 34, 255) \
    NK_COLOR(NK_COLOR_TOGGLE,                   76, 88, 68, 255) \
    NK_COLOR(NK_COLOR_TOGGLE_HOVER,             90, 104, 80, 255) \
    NK_COLOR(NK_COLOR_TOGGLE_CURSOR,            196, 181, 80, 255) \
    NK_COLOR(NK_COLOR_SELECT,                   62, 70, 55, 255) \
    NK_COLOR(NK_COLOR_SELECT_ACTIVE,            150, 143, 53, 255) \
    NK_COLOR(NK_COLOR_SLIDER,                   40, 46, 34, 255) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR,            136, 145, 128, 255) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_HOVER,      160, 170, 149, 255) \
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_ACTIVE,     196, 181, 80, 255) \
    NK_COLOR(NK_COLOR_PROPERTY,                 40, 46, 34, 255) \
    NK_COLOR(NK_COLOR_EDIT,                     40, 46, 34, 255)  \
    NK_COLOR(NK_COLOR_EDIT_CURSOR,              216, 222, 211, 255) \
    NK_COLOR(NK_COLOR_COMBO,                    76, 88, 68, 255) \
    NK_COLOR(NK_COLOR_CHART,                    136, 145, 128, 255) \
    NK_COLOR(NK_COLOR_CHART_COLOR,              76, 88, 68, 255) \
    NK_COLOR(NK_COLOR_CHART_COLOR_HIGHLIGHT,    255, 0,  0, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR,                40, 46, 34, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR,         136, 145, 128, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_HOVER,   160, 170, 149, 255) \
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_ACTIVE,  196, 181, 80, 255) \
    NK_COLOR(NK_COLOR_TAB_HEADER,               40, 46, 34, 255)
    const struct nk_color
        hlvr_color_style[NK_COLOR_COUNT] = {
        #define NK_COLOR(a,b,c,d,e) {b,c,d,e},
            HLVR_COLOR_MAP(NK_COLOR)
        #undef NK_COLOR
    };
#undef HLVR_COLOR_MAP
}

VRGUIRenderer VRGUIRenderer::instance;
VRGUIRenderer& VRGUIRenderer::Instance()
{
    return VRGUIRenderer::instance;
}

void VRGUIRenderer::Init(int guiWidth, int guiHeight)
{
    if (isInitialized)
    {
        ShutDown();
    }

    // TODO: Make setting?
    static constexpr const float GuiWidgetScale = 1.2f;

    // GUI is designed for a 2048x2048 square, scale all size/widths/etc accordingly.
    m_guiWidth = guiWidth;
    m_guiHeight = guiHeight;
    m_guiScaleW = (guiWidth / 2048.f) * GuiWidgetScale;
    m_guiScaleH = (guiHeight / 2048.f) * GuiWidgetScale;

    nulltexture.texture.id = VRTextureHelper::Instance().GetTexture("null.png");
    nulltexture.uv = { 0.f, 0.f };

    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);

    std::string font_path = GetPathFor("/fonts/Roboto-Regular.ttf").u8string();

    font_heading = nk_font_atlas_add_from_file(&atlas, font_path.c_str(), 120 * m_guiScaleH, 0);
    font_text = nk_font_atlas_add_from_file(&atlas, font_path.c_str(), 80 * m_guiScaleH, 0);
    font_text_smol = nk_font_atlas_add_from_file(&atlas, font_path.c_str(), 60 * m_guiScaleH, 0);

    if (!font_heading || !font_text || !font_text_smol)
    {
        VRGameFunctions::PrintToConsole(("SetFont failed: Failed to load font from '" + font_path + "'!\n").c_str());
        nk_font_atlas_cleanup(&atlas);
        return;
    }

    int width, height;
    const void* image = nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32);

    glGenTextures(1, &font_tex);
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    nk_font_atlas_end(&atlas, nk_handle_id((int)font_tex), &nulltexture);

    nk_init_default(&ctx, font_text ? &font_text->handle : nullptr);
    nk_style_from_table(&ctx, hlvr_color_style);
    nk_buffer_init_default(&cmds);

    /*
    ctx.style.window.header.label_padding = nk_vec2(20, 20);
    ctx.style.window.header.padding = nk_vec2(20, 20);
    ctx.style.window.header.spacing = nk_vec2(20, 20);
    */

    nk_style_load_all_cursors(&ctx, atlas.cursors);
    nk_style_hide_cursor(&ctx);  // the laser beam shows where we point

    // Menu panels are placed manually in a free layout space, everything around them is transparent
    ctx.style.window.padding = nk_vec2(0, 0);
    ctx.style.window.spacing = nk_vec2(0, 0);
    ctx.style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));

    // The 1 px default button border is thinner than a texel of the overlay as seen in the headset and breaks up
    ctx.style.button.border = 3.f;

    /*
    // NK_WIDGET_STATE_HOVER
    nk_style_item hover = ctx.style.option.hover;
    nk_style_item cursor_hover = ctx.style.option.cursor_hover;
    nk_color text_hover = ctx.style.option.text_hover;

    // NK_WIDGET_STATE_ACTIVED
    //nk_style_item hover = ctx.style.option.hover;
    //nk_style_item cursor_hover = ctx.style.option.cursor_hover;
    nk_color text_active = ctx.style.option.text_active;

    // NK_WIDGET_STATE_NORMAL
    nk_style_item normal = ctx.style.option.normal;
    nk_style_item cursor_normal = ctx.style.option.cursor_normal;
    nk_color text_normal = ctx.style.option.text_normal;

    //ctx.style.option.active;

    ctx.style.option.normal = hover;
    ctx.style.option.cursor_normal = cursor_hover;
    ctx.style.option.text_normal = text_active;

    ctx.style.option.hover = normal;
    ctx.style.option.cursor_hover = cursor_normal;
    */

    /*
    ctx.style.option.text_normal = { 255, 0, 0, 255 };
    ctx.style.option.text_active = { 0, 255, 0, 255 };
    ctx.style.option.text_background = { 0, 0, 255, 255 };
    ctx.style.option.text_hover = { 255, 0, 255, 255 };

    ctx.style.option.cursor_normal.data.color = { 255, 0, 0, 255 };
    ctx.style.option.cursor_hover.data.color = { 0, 255, 0, 255 };
    */


    SafeSetFont(font_text);

    isInitialized = true;
}

void VRGUIRenderer::ShutDown()
{
    if (isInitialized)
    {
        nk_free(&ctx);
        isInitialized = false;
    }
}

void VRGUIRenderer::UpdateControllerState(VRControllerState& state)
{
    lastState = currentState;
    currentState = state;
}

void VRGUIRenderer::UpdateGUI(int guiWidth, int guiHeight, bool isInGame)
{
    if (!isInitialized || guiWidth != m_guiWidth || guiHeight != m_guiHeight)
    {
        Init(guiWidth, guiHeight);
    }

    if (!ctx.style.font)
    {
        if (atlas.default_font)
        {
            VRGameFunctions::PrintToConsole("UpdateGUI: ctx.style.font was null, falling back to default!\n");
            ctx.style.font = &atlas.default_font->handle;
        }
        else
        {
            VRGameFunctions::PrintToConsole("UpdateGUI failed: ctx.style.font is null and no default font!\n");
            return;
        }
    }

    UpdateVRCursor();

    UpdateGUIElements(isInGame);

    DrawGUI();

    nk_clear(&ctx);
}

void VRGUIRenderer::UpdateVRCursor()
{
    nk_input_begin(&ctx);
    if (currentState.isOverGUI)
    {
        int x = (int)(currentState.x * m_guiWidth);
        int y = (int)(currentState.y * m_guiHeight);
        nk_input_motion(&ctx, x, y);
        nk_input_button(&ctx, nk_buttons::NK_BUTTON_LEFT, x, y, currentState.isPressed);
    }
    else
    {
        nk_input_motion(&ctx, -100, -100);
        nk_input_button(&ctx, nk_buttons::NK_BUTTON_LEFT, -100, -100, false);
    }
    nk_input_end(&ctx);
}

namespace
{
    // Menu panels ("boxes") are stacked left to right. The newest one is on top and sits at the anchor
    // position, older ones are shifted left. Opening a page slides a new box in, going back slides it out.
    enum class MenuPage { Main, NewGame, Load, Options, Game, Graphics, Controls, Interaction, Comfort, Interface, Quit };

    struct MenuPanel
    {
        MenuPage page;
        float x;
        float w;
        bool leaving;
    };

    enum class MenuItemType { Button, Choice, Stepper };

    struct MenuItem
    {
        std::string label;
        std::function<void()> action;
        bool enabled = true;
        bool smallFont = false;

        // Choice: getValue/setValue use the index into options (one button per option). Stepper: they use the value.
        MenuItemType type = MenuItemType::Button;
        std::function<float()> getValue;
        std::function<void(float)> setValue;
        std::vector<std::string> options;

        // Stepper: a draggable bar with the value, and minus and plus buttons on its sides
        float min = 0.f;
        float max = 1.f;
        float step = 0.1f;
        int decimals = 1;
        std::string suffix;
        float displayScale = 1.f;

        // Shown as a tooltip when the pointer rests on the row. A choice can also describe each option.
        std::string description;
        std::vector<std::string> optionDescriptions;
    };

    MenuItem MakeStepper(const std::string& label, std::function<float()> getValue, std::function<void(float)> setValue,
        float min, float max, float step, int decimals, const std::string& suffix = "", float displayScale = 1.f)
    {
        MenuItem item;
        item.label = label;
        item.type = MenuItemType::Stepper;
        item.getValue = std::move(getValue);
        item.setValue = std::move(setValue);
        item.min = min;
        item.max = max;
        item.step = step;
        item.decimals = decimals;
        item.suffix = suffix;
        item.displayScale = displayScale;
        return item;
    }

    MenuItem MakeChoice(const std::string& label, std::vector<std::string> options, std::function<float()> getValue, std::function<void(float)> setValue)
    {
        MenuItem item;
        item.label = label;
        item.type = MenuItemType::Choice;
        item.options = std::move(options);
        item.getValue = std::move(getValue);
        item.setValue = std::move(setValue);
        return item;
    }

    // Choice that sets a cvar to the index of the option (0, 1, 2...), for an On/Off setting use { "Off", "On" }
    MenuItem MakeCvarChoice(const std::string& label, const std::string& cvar, std::vector<std::string> options)
    {
        return MakeChoice(label, std::move(options),
            [cvar] { return VRGameFunctions::GetCVar(cvar.c_str()); },
            [cvar](float index) { VRGameFunctions::SetCVar(cvar.c_str(), index); });
    }

    std::vector<MenuPanel> menuPanels;
    std::function<void()> menuPendingAction;  // run after drawing, actions modify menuPanels
    bool menuWasInGame = false;
    float menuSpawnX = 0.f;
    std::string menuDragKey;  // the slider that is being dragged (it was pressed, and the button is still held)

    constexpr size_t SavesPerPage = 5;
    int menuLoadPage = 0;
    std::vector<VRGameFunctions::SaveFile> menuSaves;
    bool menuSavesDirty = true;

    // The save list is cached, it is read again whenever the menu is opened (see MenuReset) or the Load page is
    const std::vector<VRGameFunctions::SaveFile>& GetMenuSaves()
    {
        if (menuSavesDirty)
        {
            menuSaves = VRGameFunctions::GetSaveFiles();
            menuSavesDirty = false;
        }
        return menuSaves;
    }

    float MenuPageWidthFraction(MenuPage page)
    {
        if (page == MenuPage::Quit)
            return 0.4f;
        return (page == MenuPage::Load || page == MenuPage::Game || page == MenuPage::Graphics || page == MenuPage::Controls || page == MenuPage::Interaction || page == MenuPage::Comfort || page == MenuPage::Interface) ? 0.45f : 0.33f;
    }

    void MenuPush(MenuPage page)
    {
        menuPanels.push_back({ page, menuSpawnX, std::round(MenuPageWidthFraction(page) * menuSpawnX), false });
    }

    void MenuBack()
    {
        int active = 0;
        for (const auto& panel : menuPanels)
            if (!panel.leaving) active++;
        if (active <= 1)
            return;
        for (int i = (int)menuPanels.size() - 1; i >= 0; i--)
        {
            if (!menuPanels[i].leaving)
            {
                menuPanels[i].leaving = true;
                return;
            }
        }
    }

    void MenuReset()
    {
        menuSavesDirty = true;
        menuPanels.clear();
    }

    // Tooltip texts of the settings, found by the label of the row (and the page it is on)
    struct MenuTip
    {
        const char* label;
        const char* text;
        std::vector<const char*> optionTexts;  // optional, one per option
    };

    const std::vector<MenuTip>& GetMenuTips(MenuPage page)
    {
        static const std::vector<MenuTip> none;
        static const std::vector<MenuTip> game = {
            { "Difficulty", "How hard the enemies hit. Applies from the next map or new game.", {} },
            { "Volume", "Volume of sound effects and voices.", {} },
            { "Music", "Volume of the music.", {} },
        };
        static const std::vector<MenuTip> controls = {
            { "Left-handed", "Swap the hands: the weapon goes into your left hand.", {} },
            { "Move direction", "Walking follows where your off hand, weapon hand or head points.", {} },
            { "Flashlight", "Which hand, or your head, the flashlight beam comes from.", {} },
            { "Teleporter", "Which hand, or your head, the teleporter aims with.", {} },
        };
        static const std::vector<MenuTip> interaction = {
            { "Grenade speed", "How fast grenades fly when you let go.", { "Grenades fly with the speed of your hand.", "Grenades always fly at the same speed, as in the original game." } },
            { "Laser sight", "Shows a laser from ranged weapons to help you aim.", {} },
            { "Ladders", "How you climb ladders.", { "Grab the ladder with your hands or climb with the movement stick.", "Climb ladders only by pulling yourself up with your hands.", "Climb ladders only with the movement stick, as in the original game." } },
            { "Aim at NPCs", "How friendly NPCs react when you point a weapon at them.", { "They do not react.", "They react but do not get angry.", "They react and get angry." } },
            { "Melee speed", "The slowest swing that still hurts enemies. Lower values are more sensitive.", {} },
        };
        static const std::vector<MenuTip> comfort = {
            { "Turning", "How the view turns with the turn stick.", { "Turn smoothly while the stick is pushed.", "Turn in fixed steps, which helps against motion sickness." } },
            { "Snap angle", "How far each snap turn rotates the view.", {} },
            { "Turn speed", "How fast smooth turning is, in degrees per second.", {} },
            { "Instant move", "On: you start and stop moving instantly instead of sliding. Helps against motion sickness.", {} },
            { "Turn with trains", "What happens to your view on trains and platforms that rotate.", { "Your view turns together with the train or platform.", "Your view direction stays fixed while the train or platform turns." } },
            { "Height offset", "Raises or lowers your view, in game units. Useful when sitting.", {} },
            { "Real crouch", "Crouch in the game when you crouch in real life.", {} },
            { "Crouch height", "The game crouches when your headset is lower than this, in game units.", {} },
            { "Auto crouch", "Crouch automatically where you would not fit standing.", {} },
        };
        static const std::vector<MenuTip> graphics = {
            { "Models", "Look of characters, weapons and objects. Changing it restarts the game.", { "The standard models. Selecting it restarts the game.", "HD models. Selecting it restarts the game." } },
            { "Textures", "How textures are filtered.", { "The original pixelated look.", "Smooth filtering." } },
            { "World scale", "Size of the world compared to you. Larger values make the world bigger.", {} },
            { "Grab debris", "Debris from broken objects that you can pick up and throw. Turn off for better performance.", {} },
        };
        static const std::vector<MenuTip> interfaceTips = {
            { "HUD", "Where the health and ammo display is shown.", { "No HUD.", "Shown on your controllers." } },
            { "Health", "Show the health display.", {} },
            { "Ammo", "Show the ammo display.", {} },
            { "Menu size", "Size of this menu. Applies when the menu is opened or recentered.", {} },
            { "Menu opacity", "How see-through this menu is.", {} },
            { "Menu height", "Height of this menu relative to your eyes. Applies when the menu is opened or recentered.", {} },
        };

        switch (page)
        {
        case MenuPage::Game: return game;
        case MenuPage::Controls: return controls;
        case MenuPage::Interaction: return interaction;
        case MenuPage::Comfort: return comfort;
        case MenuPage::Graphics: return graphics;
        case MenuPage::Interface: return interfaceTips;
        default: return none;
        }
    }

    void AddMenuTips(MenuPage page, std::vector<MenuItem>& items)
    {
        for (MenuItem& item : items)
        {
            for (const MenuTip& tip : GetMenuTips(page))
            {
                if (item.label == tip.label)
                {
                    item.description = tip.text;
                    for (const char* optionText : tip.optionTexts)
                        item.optionDescriptions.push_back(optionText);
                    break;
                }
            }
        }
    }

    std::vector<MenuItem> BuildMenuItems(MenuPage page, bool isInGame)
    {
        std::vector<MenuItem> items;
        switch (page)
        {
        case MenuPage::Main:
        {
            if (isInGame)
            {
                items.push_back({ "Resume game", [] { VRGameFunctions::CloseMenu(); } });
            }
            else if (!GetMenuSaves().empty())
            {
                // no game is running (or the player is dead): continue with the newest save
                const std::string newestSave = GetMenuSaves().front().name;
                items.push_back({ "Resume game", [newestSave] { VRGameFunctions::LoadGame(newestSave); MenuReset(); } });
            }
            items.push_back({ "New game", [] { MenuPush(MenuPage::NewGame); } });
            items.push_back({ "Load game", [] { menuLoadPage = 0; menuSavesDirty = true; MenuPush(MenuPage::Load); } });
            if (VRGameFunctions::IsGameLoaded())
            {
                // shown but disabled when saving is not possible (dead, intermission)
                items.push_back({ "Save game", [] { VRGameFunctions::SaveGame(); VRGameFunctions::CloseMenu(); MenuReset(); }, isInGame });
            }
            items.push_back({ "Options", [] { MenuPush(MenuPage::Options); } });
            items.push_back({ "Quit", [] { MenuPush(MenuPage::Quit); } });
            break;
        }

        case MenuPage::Quit:
            items.push_back({ "Quit game", nullptr, false });
            items.push_back({ "Do you wish to stop playing now?", nullptr, false, true });
            if (isInGame)
                items.push_back({ "Unsaved progress is lost!", nullptr, false, true });
            items.push_back({ "Quit game", [] { VRGameFunctions::QuitGame(); } });
            items.push_back({ "Cancel", [] { MenuBack(); } });
            break;

        case MenuPage::NewGame:
            items.push_back({ "Training room", [] { VRGameFunctions::StartHazardCourse(); MenuReset(); } });
            items.push_back({ "Easy", [] { VRGameFunctions::SetCVar("skill", 1.f); VRGameFunctions::StartNewGame(); MenuReset(); } });
            items.push_back({ "Medium", [] { VRGameFunctions::SetCVar("skill", 2.f); VRGameFunctions::StartNewGame(); MenuReset(); } });
            items.push_back({ "Hard", [] { VRGameFunctions::SetCVar("skill", 3.f); VRGameFunctions::StartNewGame(); MenuReset(); } });
            items.push_back({ "Back", [] { MenuBack(); } });
            break;

        case MenuPage::Load:
        {
            const auto& saves = GetMenuSaves();
            const int pageCount = ((int)saves.size() + (int)SavesPerPage - 1) / (int)SavesPerPage;
            if (menuLoadPage >= pageCount)
                menuLoadPage = (std::max)(0, pageCount - 1);

            if (saves.empty())
            {
                items.push_back({ "No saved games", nullptr, false });
            }
            for (size_t i = menuLoadPage * SavesPerPage; i < saves.size() && i < (menuLoadPage + 1) * SavesPerPage; i++)
            {
                const std::string name = saves[i].name;
                items.push_back({ name, [name] { VRGameFunctions::LoadGame(name); MenuReset(); }, true, true });
            }
            if (menuLoadPage > 0)
                items.push_back({ "Previous page", [] { menuLoadPage--; } });
            if (menuLoadPage + 1 < pageCount)
                items.push_back({ "Next page", [] { menuLoadPage++; } });
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Options:
        {
            items.push_back({ "Game", [] { MenuPush(MenuPage::Game); } });
            items.push_back({ "Graphics", [] { MenuPush(MenuPage::Graphics); } });
            items.push_back({ "Controls", [] { MenuPush(MenuPage::Controls); } });
            items.push_back({ "Comfort", [] { MenuPush(MenuPage::Comfort); } });
            items.push_back({ "Interaction", [] { MenuPush(MenuPage::Interaction); } });
            items.push_back({ "Interface", [] { MenuPush(MenuPage::Interface); } });
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Graphics:
        {
            items.push_back(MakeChoice("Models", { "SD", "HD" },
                [] { return (float)VRGameFunctions::GetModelsMode(); },
                [](float index) { VRGameFunctions::SetModelsMode((int)index); }));

            items.push_back(MakeChoice("Textures", { "Classic", "Smooth" },
                [] { return (float)VRGameFunctions::GetTexturesMode(); },
                [](float index) { VRGameFunctions::SetTexturesMode((int)index); }));

            items.push_back(MakeStepper("World scale",
                [] { return VRGameFunctions::GetCVar("vr_world_scale"); },
                [](float value) { VRGameFunctions::SetCVar("vr_world_scale", value); },
                0.5f, 1.5f, 0.01f, 0, "%", 100.f));

            items.push_back(MakeCvarChoice("Grab debris", "vr_enable_interactive_debris", { "Off", "On" }));

            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Controls:
        {
            items.push_back(MakeCvarChoice("Left-handed", "vr_lefthand_mode", { "Off", "On" }));
            items.push_back(MakeCvarChoice("Move direction", "vr_movement_attachment", { "Off Hand", "Weapon", "Head" }));
            items.push_back(MakeCvarChoice("Flashlight", "vr_flashlight_attachment", { "Off Hand", "Weapon", "Head" }));
            items.push_back(MakeCvarChoice("Teleporter", "vr_teleport_attachment", { "Off Hand", "Weapon", "Head" }));
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Interaction:
        {
            items.push_back(MakeCvarChoice("Grenade speed", "vr_weapon_grenade_mode", { "Hand", "Fixed" }));  // thrown with the speed of the hand, or at a fixed speed
            items.push_back(MakeCvarChoice("Laser sight", "vr_enable_aim_laser", { "Off", "On" }));
            items.push_back(MakeCvarChoice("Ladders", "vr_ladder_mode", { "Both", "Hands", "Classic" }));  // combined, immersive, legacy
            items.push_back(MakeCvarChoice("Aim at NPCs", "vr_npc_gunpoint", { "Ignore", "React", "Angry" }));  // how NPCs react when aimed at
            items.push_back(MakeStepper("Melee speed",  // the slowest swing that still hurts
                [] { return VRGameFunctions::GetCVar("vr_melee_swing_speed"); },
                [](float value) { VRGameFunctions::SetCVar("vr_melee_swing_speed", value); },
                20.f, 300.f, 10.f, 0));
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Comfort:
        {
            const bool snapTurn = VRGameFunctions::GetCVar("vr_move_snapturn_enabled") != 0.f;
            items.push_back(MakeCvarChoice("Turning", "vr_move_snapturn_enabled", { "Smooth", "Snap" }));
            if (snapTurn)
            {
                items.push_back(MakeStepper("Snap angle",
                    [] { return VRGameFunctions::GetCVar("vr_move_snapturn_angle"); },
                    [](float value) { VRGameFunctions::SetCVar("vr_move_snapturn_angle", value); },
                    15.f, 90.f, 15.f, 0, "\xC2\xB0"));
            }
            else
            {
                items.push_back(MakeStepper("Turn speed",
                    [] { return VRGameFunctions::GetCVar("vr_yawspeed"); },
                    [](float value) { VRGameFunctions::SetCVar("vr_yawspeed", value); },
                    60.f, 360.f, 10.f, 0));
            }

            // acceleration and deceleration are one setting here
            items.push_back(MakeChoice("Instant move", { "Off", "On" },
                [] { return (VRGameFunctions::GetCVar("vr_move_instant_accelerate") != 0.f && VRGameFunctions::GetCVar("vr_move_instant_decelerate") != 0.f) ? 1.f : 0.f; },
                [](float index) {
                    VRGameFunctions::SetCVar("vr_move_instant_accelerate", index > 0.5f ? 1.f : 0.f);
                    VRGameFunctions::SetCVar("vr_move_instant_decelerate", index > 0.5f ? 1.f : 0.f);
                }));

            // standing on rotating trains and platforms: turn with them or keep the view direction
            items.push_back(MakeCvarChoice("Turn with trains", "vr_dont_rotate_with_trains", { "On", "Off" }));

            // both of these are in game units
            items.push_back(MakeStepper("Height offset",
                [] { return VRGameFunctions::GetCVar("vr_headset_offset"); },
                [](float value) { VRGameFunctions::SetCVar("vr_headset_offset", value); },
                -50.f, 50.f, 1.f, 0));

            items.push_back(MakeCvarChoice("Real crouch", "vr_rl_ducking_enabled", { "Off", "On" }));
            if (VRGameFunctions::GetCVar("vr_rl_ducking_enabled") != 0.f)
            {
                items.push_back(MakeStepper("Crouch height",
                    [] { return VRGameFunctions::GetCVar("vr_rl_duck_height"); },
                    [](float value) { VRGameFunctions::SetCVar("vr_rl_duck_height", value); },
                    10.f, 60.f, 1.f, 0));
            }
            items.push_back(MakeCvarChoice("Auto crouch", "vr_autocrouch_enabled", { "Off", "On" }));
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Interface:
        {
            // vr_hud_mode: 0 off, 1 in view (not offered), 2 on the controllers
            items.push_back(MakeChoice("HUD", { "Off", "On" },
                [] { return VRGameFunctions::GetCVar("vr_hud_mode") != 0.f ? 1.f : 0.f; },
                [](float index) { VRGameFunctions::SetCVar("vr_hud_mode", index != 0.f ? 2.f : 0.f); }));
            if (VRGameFunctions::GetCVar("vr_hud_mode") != 0.f)
            {
                items.push_back(MakeCvarChoice("Health", "vr_hud_health", { "Off", "On" }));
                items.push_back(MakeCvarChoice("Ammo", "vr_hud_ammo", { "Off", "On" }));
            }

            // Size and height are applied when the menu is placed again (opened or recentered), the opacity at once
            items.push_back(MakeStepper("Menu size",
                [] { return VRGameFunctions::GetCVar("vr_menu_scale"); },
                [](float value) { VRGameFunctions::SetCVar("vr_menu_scale", value); },
                0.5f, 2.f, 0.1f, 0, "%", 100.f));
            items.push_back(MakeStepper("Menu opacity",
                [] { return VRGameFunctions::GetCVar("vr_menu_opacity"); },
                [](float value) { VRGameFunctions::SetCVar("vr_menu_opacity", value); },
                0.3f, 1.f, 0.05f, 0, "%", 100.f));
            items.push_back(MakeStepper("Menu height",  // relative to eye level
                [] { return VRGameFunctions::GetCVar("vr_menu_height"); },
                [](float value) { VRGameFunctions::SetCVar("vr_menu_height", value); },
                -0.6f, 0.3f, 0.05f, 0, " cm", 100.f));
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }

        case MenuPage::Game:
        {
            // Difficulty only takes effect when the next map is loaded
            items.push_back(MakeChoice("Difficulty", { "Easy", "Medium", "Hard" },
                [] { return (float)(std::clamp((int)VRGameFunctions::GetCVar("skill"), 1, 3) - 1); },
                [](float index) { VRGameFunctions::SetCVar("skill", index + 1.f); }));
            items.push_back(MakeStepper("Volume",
                [] { return VRGameFunctions::GetCVar("volume"); },
                [](float value) { VRGameFunctions::SetCVar("volume", value); },
                0.f, 1.f, 0.05f, 0, "%", 100.f));
            items.push_back(MakeStepper("Music",
                [] { return VRGameFunctions::GetCVar("mp3volume"); },
                [](float value) { VRGameFunctions::SetCVar("mp3volume", value); },
                0.f, 1.f, 0.05f, 0, "%", 100.f));
            items.push_back({ "Back", [] { MenuBack(); } });
            break;
        }
        }
        AddMenuTips(page, items);
        return items;
    }
}

namespace
{
    // Classic Steam style bevel: light top/left edge, dark bottom/right edge, swapped when sunken
    constexpr float BevelThickness = 3.f;
    const struct nk_color BevelLight = { 136, 145, 128, 255 };
    const struct nk_color BevelDark = { 40, 46, 34, 255 };

    void DrawBevel(struct nk_command_buffer* out, struct nk_rect r, bool raised)
    {
        const float t = BevelThickness;
        const struct nk_color topLeft = raised ? BevelLight : BevelDark;
        const struct nk_color bottomRight = raised ? BevelDark : BevelLight;
        nk_fill_rect(out, nk_rect(r.x, r.y, r.w, t), 0.f, topLeft);
        nk_fill_rect(out, nk_rect(r.x, r.y, t, r.h), 0.f, topLeft);
        nk_fill_rect(out, nk_rect(r.x, r.y + r.h - t, r.w, t), 0.f, bottomRight);
        nk_fill_rect(out, nk_rect(r.x + r.w - t, r.y, t, r.h), 0.f, bottomRight);
    }

    // Cuts a label off with "..." so it fits into maxWidth with the current font
    std::string FitText(const char* label, float maxWidth)
    {
        std::string shown = label;
        const struct nk_user_font* font = ctx.style.font;
        if (font->width(font->userdata, font->height, shown.c_str(), (int)shown.size()) <= maxWidth)
            return shown;

        while (!shown.empty())
        {
            shown.pop_back();
            while (!shown.empty() && (shown.back() & 0xC0) == 0x80)  // don't cut a UTF-8 sequence in half
                shown.pop_back();
            const std::string candidate = shown + "...";
            if (font->width(font->userdata, font->height, candidate.c_str(), (int)candidate.size()) <= maxWidth)
                return candidate;
        }
        return shown;
    }

    // Replacement for nk_button_label with a bevel, uses the colors of the button style.
    // Place it with nk_layout_space_push first. A selected button is drawn sunken (for a choice of options).
    bool BevelButton(struct nk_command_buffer* out, const char* label, bool selected = false)
    {
        struct nk_rect bounds;
        const enum nk_widget_layout_states state = nk_widget(&bounds, &ctx);
        if (state == NK_WIDGET_INVALID)
            return false;

        const struct nk_input* in = (state == NK_WIDGET_ROM || (ctx.current->layout->flags & NK_WINDOW_ROM)) ? nullptr : &ctx.input;
        nk_flags buttonState = 0;
        const bool clicked = nk_button_behavior(&buttonState, bounds, in, NK_BUTTON_DEFAULT) != 0;
        const bool down = selected || (buttonState & NK_WIDGET_STATE_ACTIVED) != 0;
        const bool hovered = (buttonState & NK_WIDGET_STATE_HOVER) != 0;

        const struct nk_style_button& style = ctx.style.button;
        const struct nk_style_item& background = down ? style.active : hovered ? style.hover : style.normal;
        nk_fill_rect(out, bounds, 0.f, background.data.color);
        DrawBevel(out, bounds, !down);

        struct nk_text text;
        text.padding = nk_vec2(0, 0);
        text.background = nk_rgba(0, 0, 0, 0);
        text.text = down ? style.text_active : hovered ? style.text_hover : style.text_normal;
        const std::string shown = FitText(label, bounds.w - 4.f * BevelThickness);

        const float pressOffset = down ? 2.f : 0.f;
        nk_widget_text(out, nk_rect(bounds.x + pressOffset, bounds.y + pressOffset, bounds.w, bounds.h), shown.c_str(), (int)shown.size(), &text, NK_TEXT_CENTERED, ctx.style.font);
        return clicked;
    }
}

namespace
{
    // Draws a tooltip box next to the pointer, the text is wrapped to the width of the box
    void DrawTooltip(struct nk_command_buffer* out, const std::string& tip, float W, float H)
    {
        SafeSetFont(font_text_smol);
        const struct nk_user_font* font = ctx.style.font;
        const float maxTextWidth = 0.38f * W;
        const float padX = 0.015f * W;
        const float padY = 0.012f * H;
        const float lineHeight = std::round(font->height * 1.25f);

        auto textWidth = [&](const std::string& text) { return font->width(font->userdata, font->height, text.c_str(), (int)text.size()); };

        std::vector<std::string> lines;
        std::string line;
        size_t position = 0;
        while (position < tip.size())
        {
            size_t end = tip.find(' ', position);
            if (end == std::string::npos) end = tip.size();
            const std::string word = tip.substr(position, end - position);
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (line.empty() || textWidth(candidate) <= maxTextWidth)
            {
                line = candidate;
            }
            else
            {
                lines.push_back(line);
                line = word;
            }
            position = end + 1;
        }
        if (!line.empty())
            lines.push_back(line);

        float widest = 0.f;
        for (const std::string& l : lines)
            widest = (std::max)(widest, textWidth(l));

        const float boxW = std::round(widest + 2.f * padX);
        const float boxH = std::round(lines.size() * lineHeight + 2.f * padY);

        // below and to the right of the pointer, moved to stay inside the texture
        const struct nk_vec2 pointer = ctx.input.mouse.pos;
        float x = pointer.x + 0.02f * W;
        float y = pointer.y + 0.05f * H;
        if (x + boxW > W - 4.f) x = W - 4.f - boxW;
        if (y + boxH > H - 4.f) y = pointer.y - boxH - 0.02f * H;
        x = (std::max)(4.f, std::round(x));
        y = (std::max)(4.f, std::round(y));

        const struct nk_rect box = nk_rect(x, y, boxW, boxH);
        nk_fill_rect(out, box, 0.f, nk_rgba(40, 46, 34, 255));
        DrawBevel(out, box, true);

        struct nk_text text;
        text.padding = nk_vec2(0, 0);
        text.background = nk_rgba(0, 0, 0, 0);
        text.text = ctx.style.text.color;
        for (size_t i = 0; i < lines.size(); i++)
            nk_widget_text(out, nk_rect(x + padX, y + padY + i * lineHeight, boxW - padX, lineHeight), lines[i].c_str(), (int)lines[i].size(), &text, NK_TEXT_LEFT, ctx.style.font);
    }
}

void VRGUIRenderer::ResetMenu()
{
    MenuReset();
}

void VRGUIRenderer::UpdateGUIElements(bool isInGame)
{
    const float W = (float)m_guiWidth;
    const float H = (float)m_guiHeight;

    // Layout, relative to the texture size
    // Everything is snapped to whole pixels, edges at fractional positions look uneven
    const float pad = std::round(0.025f * H);
    const float baseRowH = std::round(0.085f * H);
    const float baseRowGap = std::round(0.017f * H);
    const float gapX = std::round(0.025f * W);
    const float topY = std::round(0.12f * H);
    const float compactW = std::round(0.24f * W);  // width of the boxes that are not the newest one
    const float maxTotalW = 0.99f * W;
    menuSpawnX = W;

    if (isInGame != menuWasInGame)
    {
        menuWasInGame = isInGame;
        MenuReset();
    }
    if (menuPanels.empty())
    {
        const float mainW = std::round(MenuPageWidthFraction(MenuPage::Main) * W);
        menuPanels.push_back({ MenuPage::Main, std::round((W - mainW) / 2.f), mainW, false });
    }

    int topIndex = -1;
    for (int i = 0; i < (int)menuPanels.size(); i++)
        if (!menuPanels[i].leaving) topIndex = i;

    // Place the boxes: the newest one is full size at the right, older ones are narrower and sit to its left.
    // The boxes that fit are centered as a group, the rest wait outside on the left.
    std::vector<float> targetX(menuPanels.size(), W);
    std::vector<float> targetW(menuPanels.size());
    {
        std::vector<int> visible;
        float total = 0.f;
        for (int i = topIndex; i >= 0; i--)
        {
            if (menuPanels[i].leaving)
                continue;
            const float w = (i == topIndex) ? std::round(MenuPageWidthFraction(menuPanels[i].page) * W) : compactW;
            if (!visible.empty() && total + gapX + w > maxTotalW)
            {
                targetX[i] = -compactW - gapX;
                targetW[i] = compactW;
                continue;
            }
            total += (visible.empty() ? 0.f : gapX) + w;
            targetW[i] = w;
            visible.insert(visible.begin(), i);
        }

        float cursor = std::round((W - total) / 2.f);
        for (int i : visible)
        {
            targetX[i] = cursor;
            cursor += targetW[i] + gapX;
        }
        for (int i = 0; i < (int)menuPanels.size(); i++)
        {
            if (menuPanels[i].leaving)
                targetW[i] = menuPanels[i].w;
        }
    }

    static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const float dt = (std::min)(0.1f, std::chrono::duration<float>(now - lastTime).count());
    lastTime = now;
    const float ease = 1.f - std::exp(-14.f * dt);

    for (int i = 0; i < (int)menuPanels.size(); i++)
    {
        menuPanels[i].x += (targetX[i] - menuPanels[i].x) * ease;
        menuPanels[i].w += (targetW[i] - menuPanels[i].w) * ease;
    }
    for (int i = (int)menuPanels.size() - 1; i >= 0; i--)
    {
        if (menuPanels[i].leaving && menuPanels[i].x > W - 2.f)
        {
            menuPanels.erase(menuPanels.begin() + i);
            if (i < topIndex) topIndex--;
        }
    }

    SafeSetFont(font_text);

    if (nk_begin(&ctx, "HLVRMainMenu", nk_rect(0, 0, W, H), NK_WINDOW_NO_SCROLLBAR))
    {
        if (!ctx.input.mouse.buttons[NK_BUTTON_LEFT].down)
            menuDragKey.clear();

        nk_layout_space_begin(&ctx, NK_STATIC, H, 64);
        struct nk_command_buffer* canvas = nk_window_get_canvas(&ctx);

        // The description of the row or option the pointer is on, shown after it rested there for a moment
        std::string hoveredTipKey;
        std::string hoveredTip;

        for (int i = 0; i < (int)menuPanels.size(); i++)
        {
            const MenuPanel panel = menuPanels[i];
            const bool interactive = (i == topIndex);
            const std::vector<MenuItem> items = BuildMenuItems(panel.page, isInGame);

            // A box with many rows gets smaller rows so it still fits into the texture
            const float neededH = 2.f * pad + items.size() * baseRowH + (items.size() - 1) * baseRowGap;
            const float fit = (std::min)(1.f, (H - topY - std::round(0.02f * H)) / neededH);
            const float rowH = std::round(baseRowH * fit);
            const float rowGap = std::round(baseRowGap * fit);

            const float panelX = std::round(panel.x);
            const float pageW = std::round(panel.w);
            const float panelH = 2.f * pad + items.size() * rowH + (items.size() - 1) * rowGap;
            const struct nk_rect panelRect = nk_rect(panelX, topY, pageW, panelH);
            nk_fill_rect(canvas, panelRect, 0.f, nk_rgba(76, 88, 68, 255));
            DrawBevel(canvas, panelRect, true);

            // The label column of choices and steppers fits the longest label of the box, so all their controls line up
            float choiceLabelW = 0.f;
            SafeSetFont(font_text_smol);
            for (const MenuItem& item : items)
            {
                if (item.type == MenuItemType::Choice || item.type == MenuItemType::Stepper)
                    choiceLabelW = (std::max)(choiceLabelW, ctx.style.font->width(ctx.style.font->userdata, ctx.style.font->height, item.label.c_str(), (int)item.label.size()) + 0.02f * W);
            }
            choiceLabelW = std::round(choiceLabelW);

            for (size_t k = 0; k < items.size(); k++)
            {
                const MenuItem& item = items[k];
                const float rowX = panelX + pad;
                const float rowY = topY + pad + k * (rowH + rowGap);
                const float rowW = pageW - 2.f * pad;
                SafeSetFont((item.smallFont || !interactive) ? font_text_smol : font_text);

                const bool usable = interactive && item.enabled;
                if (usable && !item.description.empty() && nk_input_is_mouse_hovering_rect(&ctx.input, nk_rect(rowX, rowY, rowW, rowH)))
                {
                    hoveredTipKey = std::to_string(i) + item.label;
                    hoveredTip = item.description;
                }
                float labelW = std::round(0.42f * rowW);
                if (item.type == MenuItemType::Choice || item.type == MenuItemType::Stepper)
                    labelW = choiceLabelW;

                if (item.type == MenuItemType::Button || !usable)
                {
                    if (item.type == MenuItemType::Button && usable)
                    {
                        nk_layout_space_push(&ctx, nk_rect(rowX, rowY, rowW, rowH));
                        if (BevelButton(canvas, item.label.c_str()))
                            menuPendingAction = item.action;
                    }
                    else if (item.type == MenuItemType::Button)
                    {
                        // older boxes and unavailable entries are shown dimmed and are not clickable
                        nk_layout_space_push(&ctx, nk_rect(rowX, rowY, rowW, rowH));
                        nk_label_colored(&ctx, FitText(item.label.c_str(), rowW).c_str(), NK_TEXT_CENTERED, nk_rgba(117, 128, 106, 255));
                    }
                    else
                    {
                        // inactive choice/stepper: only the dimmed label, at the same place as when active
                        SafeSetFont(font_text_smol);
                        nk_layout_space_push(&ctx, nk_rect(rowX, rowY, labelW, rowH));
                        nk_label_colored(&ctx, FitText(item.label.c_str(), labelW).c_str(), NK_TEXT_LEFT, nk_rgba(117, 128, 106, 255));
                    }
                    continue;
                }

                SafeSetFont(font_text_smol);
                const struct nk_rect fieldRect = nk_rect(rowX + labelW, rowY, rowW - labelW, rowH);
                nk_layout_space_push(&ctx, nk_rect(rowX, rowY, labelW, rowH));
                nk_label_colored(&ctx, item.label.c_str(), NK_TEXT_LEFT, ctx.style.text.color);

                if (item.type == MenuItemType::Stepper)
                {
                    // [-] slider [+]: the small buttons change the value by one step, the bar can be dragged
                    const float buttonW = std::round(0.8f * rowH);
                    const float value = item.getValue();
                    const struct nk_rect barRect = nk_rect(fieldRect.x + buttonW, rowY, fieldRect.w - 2.f * buttonW, rowH);

                    auto setSnapped = [&](float newValue) {
                        newValue = std::round(newValue / item.step) * item.step;
                        item.setValue(std::clamp(newValue, item.min, item.max));
                    };

                    nk_layout_space_push(&ctx, nk_rect(fieldRect.x, rowY, buttonW, rowH));
                    if (BevelButton(canvas, "-"))
                        setSnapped(value - item.step);

                    nk_layout_space_push(&ctx, nk_rect(fieldRect.x + fieldRect.w - buttonW, rowY, buttonW, rowH));
                    if (BevelButton(canvas, "+"))
                        setSnapped(value + item.step);

                    // Dragging starts with the press on the bar (so a bar that moves under an already held pointer
                    // is not grabbed) and follows the pointer until the button is released.
                    const std::string dragKey = std::to_string(i) + item.label;
                    if (nk_input_is_mouse_pressed(&ctx.input, NK_BUTTON_LEFT) && nk_input_is_mouse_hovering_rect(&ctx.input, barRect))
                        menuDragKey = dragKey;
                    if (menuDragKey == dragKey)
                    {
                        if (ctx.input.mouse.buttons[NK_BUTTON_LEFT].down)
                        {
                            const float t = std::clamp((ctx.input.mouse.pos.x - barRect.x) / barRect.w, 0.f, 1.f);
                            setSnapped(item.min + t * (item.max - item.min));
                        }
                        else
                        {
                            menuDragKey.clear();
                        }
                    }

                    const float shown = item.getValue();
                    const float fill = std::clamp((shown - item.min) / (item.max - item.min), 0.f, 1.f);
                    nk_layout_space_push(&ctx, barRect);
                    // colors of the slider style: normal, hovered, being dragged (the old nk_progress look, with its padding)
                    const struct nk_style_progress& sliderStyle = ctx.style.progress;
                    const bool dragging = (menuDragKey == dragKey);
                    const bool hovered = nk_input_is_mouse_hovering_rect(&ctx.input, barRect);
                    const struct nk_style_item& fillColor = dragging ? sliderStyle.cursor_active : hovered ? sliderStyle.cursor_hover : sliderStyle.cursor_normal;
                    const float inset = 4.f;
                    nk_fill_rect(canvas, barRect, 0.f, sliderStyle.normal.data.color);
                    nk_fill_rect(canvas, nk_rect(barRect.x + inset, barRect.y + inset, std::round((barRect.w - 2.f * inset) * fill), barRect.h - 2.f * inset), 0.f, fillColor.data.color);
                    DrawBevel(canvas, barRect, false);

                    char valueText[32];
                    snprintf(valueText, sizeof(valueText), "%.*f%s", item.decimals, shown * item.displayScale + 0.f, item.suffix.c_str());  // + 0 turns -0 into 0
                    struct nk_text text;
                    text.padding = nk_vec2(0, 0);
                    text.background = nk_rgba(0, 0, 0, 0);
                    text.text = ctx.style.text.color;
                    nk_widget_text(canvas, barRect, valueText, (int)strlen(valueText), &text, NK_TEXT_CENTERED, ctx.style.font);
                }
                else
                {
                    // one button per option, as wide as its text needs (scaled to fill the row), the current one is shown pressed
                    const int current = (int)std::lround(item.getValue());  // out of range: no option is selected
                    const struct nk_user_font* optionFont = ctx.style.font;
                    std::vector<float> optionWidths;
                    float optionsTotal = 0.f;
                    for (const std::string& option : item.options)
                    {
                        optionWidths.push_back(optionFont->width(optionFont->userdata, optionFont->height, option.c_str(), (int)option.size()) + 0.015f * W);
                        optionsTotal += optionWidths.back();
                    }
                    float optionX = fieldRect.x;
                    for (size_t j = 0; j < item.options.size(); j++)
                    {
                        const float w = (j + 1 == item.options.size()) ? fieldRect.x + fieldRect.w - optionX : std::round(optionWidths[j] * fieldRect.w / optionsTotal);
                        nk_layout_space_push(&ctx, nk_rect(optionX, rowY, w, rowH));
                        if (j < item.optionDescriptions.size() && nk_input_is_mouse_hovering_rect(&ctx.input, nk_rect(optionX, rowY, w, rowH)))
                        {
                            hoveredTipKey = std::to_string(i) + item.label + "#" + std::to_string(j);
                            hoveredTip = item.optionDescriptions[j];
                        }
                        if (BevelButton(canvas, item.options[j].c_str(), (int)j == current))
                            item.setValue((float)j);
                        optionX += w;
                    }
                }
            }
        }

        // tooltip, it is hidden while a button is held (clicking, dragging a bar)
        static std::string tooltipKey;
        static std::chrono::steady_clock::time_point tooltipSince;
        if (hoveredTip.empty() || ctx.input.mouse.buttons[NK_BUTTON_LEFT].down)
        {
            tooltipKey.clear();
        }
        else
        {
            if (hoveredTipKey != tooltipKey)
            {
                tooltipKey = hoveredTipKey;
                tooltipSince = now;
            }
            if (now - tooltipSince > std::chrono::milliseconds(400))
                DrawTooltip(canvas, hoveredTip, W, H);
        }

        nk_layout_space_end(&ctx);
    }
    nk_end(&ctx);

    if (menuPendingAction)
    {
        auto action = menuPendingAction;
        menuPendingAction = nullptr;
        action();
    }
}

// Adapted from nuklear_sdl_gl2.h
void VRGUIRenderer::DrawGUI()
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, m_guiWidth, m_guiHeight, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    {
        /* fill converting configuration */
        static const struct nk_draw_vertex_layout_element vertex_layout[] =
        {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_ogl_vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_ogl_vertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_ogl_vertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };

        struct nk_convert_config config;
        memset(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(struct nk_ogl_vertex);
        config.vertex_alignment = NK_ALIGNOF(struct nk_ogl_vertex);
        config.null = nulltexture;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;
        config.global_alpha = 1.0f;
        config.shape_AA = NK_ANTI_ALIASING_ON;
        config.line_AA = NK_ANTI_ALIASING_ON;

        /* convert shapes into vertexes */
        struct nk_buffer vbuf, ebuf;
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ebuf);
        nk_convert(&ctx, &cmds, &vbuf, &ebuf, &config);

        /* setup vertex buffer pointer */
        {
            GLsizei vs = sizeof(struct nk_ogl_vertex);
            size_t vp = offsetof(struct nk_ogl_vertex, position);
            size_t vt = offsetof(struct nk_ogl_vertex, uv);
            size_t vc = offsetof(struct nk_ogl_vertex, col);
            const void* vertices = nk_buffer_memory_const(&vbuf);
            glVertexPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vp));
            glTexCoordPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vt));
            glColorPointer(4, GL_UNSIGNED_BYTE, vs, (const void*)((const nk_byte*)vertices + vc));
        }

        /* iterate over and execute each draw command, twice:
         * pass 0 blends the colors, pass 1 accumulates alpha (src + dst * (1 - src)) so the menu texture
         * has correct transparency around the panels. */
        for (int pass = 0; pass < 2; pass++)
        {
            if (pass == 0)
            {
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            }

            const nk_draw_index* offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);
            const struct nk_draw_command* cmd;
            nk_draw_foreach(cmd, &ctx, &cmds)
            {
                if (!cmd->elem_count) continue;
                glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
                glScissor((GLint)(cmd->clip_rect.x), (GLint)(m_guiHeight - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)), (GLint)(cmd->clip_rect.w), (GLint)(cmd->clip_rect.h));
                glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
                offset += cmd->elem_count;
            }
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        nk_clear(&ctx);
        nk_buffer_clear(&cmds);
        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
}
