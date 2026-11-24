// The Controls page. See remappage.h.
// (CANE_SAFEHOUSE builds only: the code uses SafeHouse's shared platform layer)
#if defined(CANE_SAFEHOUSE)
#include "remappage.h"
#include "actionbind.h"
#include "../clock.h"
#include "../content.h"
#include "../font.h"
#include "../gl.h"
#include "../glshaders.h"
#include "../gui_layout.h"
#include "../Input.h"
#include "mouseinput.h"
#include "pcinput.h"
#include "prompt_glyphs.h"
#include "../sound.h"
#include <safehouse/button_prompts.h>
#include <safehouse/controller_family.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
using cane::input::Button;
using cane::input::Family;
namespace input = cane::sly1input;

enum ROWKIND { RK_Icons, RK_OriginalPs, RK_AutoCamera, RK_MouseInvertX, RK_MouseInvertY, RK_Advanced, RK_Header,
               RK_Binding, RK_CameraKey, RK_Reset, RK_Back };
struct ROW
{
    ROWKIND kind = RK_Binding;
    int index = 0;           // RK_Binding: the JOY_BUTTON (standard) or the action (advanced); RK_CameraKey: 0..3
    std::string label;
};

struct REMAP
{
    bool fOpen = false;
    int isel = 0, itop = 0;
    int col = 0;                     // 0 keyboard / mouse, 1 controller
    int irowCapture = -1;            // the row whose binding waits for an input
    bool fCaptureController = false;
    float tCapture = 0.0f;
    bool fWaitRelease = false;       // (after a capture: the captured button, still held, is no menu press)
    std::array<bool, GLFW_KEY_LAST + 1> afKeyPrev{};
};
REMAP s_remap;

constexpr int CROW_SHOWN = 11;

// ---- the page's text
const char* T(const char* en, const char* fr, const char* de, const char* es, const char* it)
{
    return ContentUiText(en, fr, de, es, it);
}
const char* PchzOnOff(bool f) { return f ? T("On", "Oui", "An", "Sí", "Sì") : T("Off", "Non", "Aus", "No", "No"); }

std::string StrIconStyle()
{
    if (g_promptStyle <= 0)
        return T("Automatic", "Automatique", "Automatisch", "Automático", "Automatico");
    if (g_promptStyle == cane::settings::Prompt_PlayStation)
        return "PlayStation 2 / 3";
    return cane::input::promptStyleName(g_promptStyle);
}

// The actions' names in the game's languages (English, French, German, Spanish, Italian), in kActions' order.
const char* const s_aszAction[][5] = {
    { "Move forward", "Avancer", "Vorwärts", "Avanzar", "Avanti" },
    { "Move backward", "Reculer", "Rückwärts", "Retroceder", "Indietro" },
    { "Move left", "Aller à gauche", "Nach links", "Ir a la izquierda", "Sinistra" },
    { "Move right", "Aller à droite", "Nach rechts", "Ir a la derecha", "Destra" },
    { "Jump", "Sauter", "Springen", "Saltar", "Salta" },
    { "Attack", "Attaquer", "Angreifen", "Atacar", "Attacca" },
    { "Action (sneak, grab, use)", "Action (discrétion, saisir, utiliser)", "Aktion (schleichen, greifen, benutzen)",
      "Acción (sigilo, agarrar, usar)", "Azione (furtività, afferra, usa)" },
    { "Use power-up", "Utiliser la technique", "Fähigkeit einsetzen", "Usar habilidad", "Usa abilità" },
    { "Previous power-up", "Technique précédente", "Vorherige Fähigkeit", "Habilidad anterior", "Abilità precedente" },
    { "Next power-up", "Technique suivante", "Nächste Fähigkeit", "Habilidad siguiente", "Abilità successiva" },
    { "Talk to Bentley (binocucom)", "Parler à Bentley (binocucom)", "Mit Bentley sprechen (Binocucom)",
      "Hablar con Bentley (binocucom)", "Parla con Bentley (binocucom)" },
    { "Skip dialogue", "Passer le dialogue", "Dialog überspringen", "Saltar diálogo", "Salta il dialogo" },
    { "Centre the camera", "Centrer la caméra", "Kamera zentrieren", "Centrar la cámara", "Centra la telecamera" },
    { "L3", "L3", "L3", "L3", "L3" },
    { "Pause", "Pause", "Pause", "Pausa", "Pausa" },
    { "Map", "Carte", "Karte", "Mapa", "Mappa" },
    { "Turret: fire", "Tourelle : tirer", "Geschütz: feuern", "Torreta: disparar", "Torretta: spara" },
    { "Turret: fire (second button)", "Tourelle : tirer (2e bouton)", "Geschütz: feuern (2. Taste)",
      "Torreta: disparar (segundo botón)", "Torretta: spara (secondo tasto)" },
    { "Murray's van: attack", "Camionnette de Murray : attaquer", "Murrays Van: angreifen", "Furgoneta de Murray: atacar",
      "Furgone di Murray: attacca" },
    { "Tank: get out", "Char : sortir", "Panzer: aussteigen", "Tanque: salir", "Carro armato: esci" },
    { "Puffer: fire", "Puffer : tirer", "Puffer: feuern", "Puffer: disparar", "Puffer: spara" },
    { "Puffer: fire (second button)", "Puffer : tirer (2e bouton)", "Puffer: feuern (2. Taste)",
      "Puffer: disparar (segundo botón)", "Puffer: spara (secondo tasto)" },
    { "Jetpack: thrust", "Jetpack : propulsion", "Jetpack: Schub", "Jetpack: propulsión", "Jetpack: spinta" },
    { "Jetpack: use a lucky charm", "Jetpack : utiliser un porte-bonheur", "Jetpack: Glücksbringer einsetzen",
      "Jetpack: usar un amuleto", "Jetpack: usa un portafortuna" },
    { "Rhythm: Square", "Rythme : Carré", "Rhythmus: Viereck", "Ritmo: Cuadrado", "Ritmo: Quadrato" },
    { "Rhythm: Triangle", "Rythme : Triangle", "Rhythmus: Dreieck", "Ritmo: Triángulo", "Ritmo: Triangolo" },
    { "Rhythm: Circle", "Rythme : Rond", "Rhythmus: Kreis", "Ritmo: Círculo", "Ritmo: Cerchio" },
    { "Rhythm: Cross", "Rythme : Croix", "Rhythmus: Kreuz", "Ritmo: Equis", "Ritmo: Croce" },
    { "Safe: Square", "Coffre : Carré", "Tresor: Viereck", "Caja fuerte: Cuadrado", "Cassaforte: Quadrato" },
    { "Safe: Triangle", "Coffre : Triangle", "Tresor: Dreieck", "Caja fuerte: Triángulo", "Cassaforte: Triangolo" },
    { "Safe: Circle", "Coffre : Rond", "Tresor: Kreis", "Caja fuerte: Círculo", "Cassaforte: Cerchio" },
    { "Safe: Cross", "Coffre : Croix", "Tresor: Kreuz", "Caja fuerte: Equis", "Cassaforte: Croce" },
};
static_assert(sizeof(s_aszAction) / sizeof(s_aszAction[0]) == input::kActionCount, "one line per Sly 1 action");

std::string StrAction(size_t i)
{
    const char* const* asz = s_aszAction[i];
    if (std::strcmp(asz[0], input::kActions[i].label) != 0)
        return input::kActions[i].label;   // (the table is behind the actions: English)
    return T(asz[0], asz[1], asz[2], asz[3], asz[4]);
}

std::string StrGroup(const std::string& group)
{
    static const char* const s_aszGroup[][5] = {
        { "Movement", "Déplacement", "Bewegung", "Movimiento", "Movimento" },
        { "Sly", "Sly", "Sly", "Sly", "Sly" },
        { "Camera", "Caméra", "Kamera", "Cámara", "Telecamera" },
        { "Menus", "Menus", "Menüs", "Menús", "Menu" },
        { "Vehicles and minigames", "Véhicules et mini-jeux", "Fahrzeuge und Minispiele", "Vehículos y minijuegos",
          "Veicoli e minigiochi" },
    };
    for (const auto& asz : s_aszGroup)
        if (group == asz[0])
            return T(asz[0], asz[1], asz[2], asz[3], asz[4]);
    return group;
}

// The standard row labels: the common action on each PS2 button.
std::string StrStandardLabel(int button)
{
    for (size_t i = 0; i < input::kActionCount; ++i)
        if (input::kActions[i].context == input::Context::Any && input::kActions[i].pad == button)
            return StrAction(i);
    return "";
}

std::vector<ROW> Arow()
{
    std::vector<ROW> a;
    a.push_back({RK_Icons, 0, T("Button Icons", "Icônes des boutons", "Tastensymbole", "Iconos de botones", "Icone dei pulsanti")});
    a.push_back({RK_OriginalPs, 0, T("Original PlayStation Icons", "Icônes PlayStation d'origine", "Original-PlayStation-Symbole",
                                     "Iconos originales de PlayStation", "Icone PlayStation originali")});
    a.push_back({RK_AutoCamera, 0, T("Auto Camera Rotation", "Rotation auto. de la caméra", "Automatische Kameradrehung",
                                     "Rotación automática de cámara", "Rotazione automatica telecamera")});
    a.push_back({RK_MouseInvertX, 0, T("Invert Mouse Horizontal", "Inverser la souris (horizontal)", "Maus horizontal umkehren",
                                       "Invertir ratón (horizontal)", "Inverti mouse (orizzontale)")});
    a.push_back({RK_MouseInvertY, 0, T("Invert Mouse Vertical", "Inverser la souris (vertical)", "Maus vertikal umkehren",
                                       "Invertir ratón (vertical)", "Inverti mouse (verticale)")});
    if (!sly1actions::profile.readOnly)
        a.push_back({RK_Advanced, 0, T("Advanced Input Mapping", "Commandes avancées", "Erweiterte Belegung",
                                       "Asignación avanzada", "Mappatura avanzata")});
    if (sly1actions::Active())
    {
        std::vector<std::string> groups;
        for (const input::ActionInfo& action : input::kActions)
            if (std::find(groups.begin(), groups.end(), action.group) == groups.end())
                groups.emplace_back(action.group);
        for (const std::string& group : groups)
        {
            a.push_back({RK_Header, 0, StrGroup(group)});
            for (size_t i = 0; i < input::kActionCount; ++i)
                if (group == input::kActions[i].group)
                    a.push_back({RK_Binding, int(i), StrAction(i)});
        }
    }
    else
    {
        a.push_back({RK_Header, 0, T("Controls", "Commandes", "Steuerung", "Controles", "Comandi")});
        for (int button = 0; button < BTN_MAX; ++button)
            a.push_back({RK_Binding, button, StrStandardLabel(button)});
    }
    static const char* const s_aszCamera[4][5] = {
        { "Camera left", "Caméra à gauche", "Kamera links", "Cámara a la izquierda", "Telecamera a sinistra" },
        { "Camera right", "Caméra à droite", "Kamera rechts", "Cámara a la derecha", "Telecamera a destra" },
        { "Camera up", "Caméra en haut", "Kamera hoch", "Cámara arriba", "Telecamera su" },
        { "Camera down", "Caméra en bas", "Kamera runter", "Cámara abajo", "Telecamera giù" },
    };
    a.push_back({RK_Header, 0, T("Camera keys", "Touches de caméra", "Kameratasten", "Teclas de cámara", "Tasti telecamera")});
    for (int i = 0; i < 4; ++i)
        a.push_back({RK_CameraKey, i, T(s_aszCamera[i][0], s_aszCamera[i][1], s_aszCamera[i][2], s_aszCamera[i][3], s_aszCamera[i][4])});
    a.push_back({RK_Reset, 0, T("Reset to Defaults", "Réinitialiser", "Standard wiederherstellen", "Restablecer", "Ripristina")});
    a.push_back({RK_Back, 0, T("Back", "Retour", "Zurück", "Atrás", "Indietro")});
    return a;
}
bool FSelectable(const ROW& r) { return r.kind != RK_Header; }
bool FOption(const ROW& r) { return r.kind <= RK_Advanced; }

// the binding shown in a cell (-2: no binding possible there, the right stick's fixed role)
int Binding(const ROW& r, bool controller)
{
    if (r.kind == RK_CameraKey)
        return controller ? -2 : g_cameraKeyBindings[size_t(r.index)];
    if (r.kind != RK_Binding)
        return -2;
    if (sly1actions::Active())
        return controller ? sly1actions::profile.gamepad[size_t(r.index)] : sly1actions::profile.keyboard[size_t(r.index)];
    return controller ? g_gamepadBindings[size_t(r.index)] : g_keyboardBindings[size_t(r.index)];
}

void SaveStandard() { PcInputSave(); }

// A new binding into a row (-1 clears; a standard controller binding returns to its default). Standard: a key /
// button another action has moves to the old one's place (the two swap); the camera keys take no mouse button.
void Bind(const ROW& r, bool controller, int binding)
{
    if (r.kind == RK_Binding && sly1actions::Active())
    {
        if (sly1actions::profile.readOnly)
            return;
        (controller ? sly1actions::profile.gamepad : sly1actions::profile.keyboard)[size_t(r.index)] = binding;
        sly1actions::Save();
        return;
    }
    if (r.kind == RK_Binding && controller)
    {
        if (binding < 0)
            binding = cane::settings::kDefaultGamepadBindings[r.index];
        const int old = g_gamepadBindings[size_t(r.index)];
        for (int& b : g_gamepadBindings)
            if (b == binding)
                b = old;
        g_gamepadBindings[size_t(r.index)] = binding;
        SaveStandard();
        return;
    }
    if (controller)
        return;
    const int key = binding < 0 ? GLFW_KEY_UNKNOWN : binding;
    if (r.kind == RK_CameraKey && FMouseBinding(key))
        return;
    int* pkey = r.kind == RK_Binding ? &g_keyboardBindings[size_t(r.index)] : &g_cameraKeyBindings[size_t(r.index)];
    const int old = *pkey;
    if (key != GLFW_KEY_UNKNOWN)
    {
        for (int& k : g_keyboardBindings)
            if (k == key)
                k = old;
        for (int& k : g_cameraKeyBindings)
            if (k == key)
                k = FMouseBinding(old) ? GLFW_KEY_UNKNOWN : old;
    }
    *pkey = key;
    SaveStandard();
}

void ResetDefaults()
{
    if (sly1actions::Active())
    {
        if (sly1actions::profile.readOnly)
            return;
        sly1actions::profile = input::defaults();
        sly1actions::profile.enabled = true;
        sly1actions::Save();
        return;
    }
    ResetKeyboardMouseBindings();
    ResetControllerBindings();
    g_cameraKeyBindings = { GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN };
    SaveStandard();
}

void ChangeOption(const ROW& r, int dir)
{
    switch (r.kind)
    {
    case RK_Icons:
    {
        // (the VR families are chosen by the VR code, not here)
        constexpr int cstyle = cane::settings::Prompt_WiiU + 1;
        g_promptStyle = ((g_promptStyle + dir) % cstyle + cstyle) % cstyle;
        break;
    }
    case RK_OriginalPs: g_fAlwaysOriginalPsLayout = !g_fAlwaysOriginalPsLayout; break;
    case RK_AutoCamera: g_fAutoCameraRotation = !g_fAutoCameraRotation; break;
    case RK_MouseInvertX: g_mouseopt.fInvertX = !g_mouseopt.fInvertX; break;
    case RK_MouseInvertY: g_mouseopt.fInvertY = !g_mouseopt.fInvertY; break;
    case RK_Advanced:
        sly1actions::profile.enabled = !sly1actions::profile.enabled;
        sly1actions::Save();
        return;
    default: return;
    }
    SaveStandard();
}

// The family the controller column shows: the icon option, else the pad used last or connected, else Xbox.
Family PadFamily()
{
    if (g_fAlwaysOriginalPsLayout)
        return Family::PlayStation;
    if (g_promptStyle > 0)
        return cane::input::familyForStyle(cane::input::PromptStyle(g_promptStyle));
    const Family f = cane::input::tracker().family();
    if (f != Family::Keyboard)
        return f;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
        if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid) && !cane::input::ignoredSteamDuplicate(jid))
            return cane::input::identify(jid).family;
    return Family::Xbox;
}

void Sound() { StartSound(static_cast<SFXID>(121), nullptr, nullptr, nullptr, 3000.0f, 300.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr); }

void Clamp(const std::vector<ROW>& arow)
{
    const int c = int(arow.size());
    s_remap.isel = std::clamp(s_remap.isel, 0, c - 1);
    if (!FSelectable(arow[size_t(s_remap.isel)]))
        s_remap.isel = std::min(c - 1, s_remap.isel + 1);
    if (s_remap.isel < s_remap.itop)
        s_remap.itop = s_remap.isel;
    if (s_remap.isel >= s_remap.itop + CROW_SHOWN)
        s_remap.itop = s_remap.isel - CROW_SHOWN + 1;
    // (a group's heading stays in view above its first row)
    if (s_remap.itop > 0 && s_remap.itop == s_remap.isel && arow[size_t(s_remap.itop - 1)].kind == RK_Header)
        --s_remap.itop;
    s_remap.itop = std::clamp(s_remap.itop, 0, std::max(0, c - CROW_SHOWN));
}

void Step(const std::vector<ROW>& arow, int d)
{
    const int c = int(arow.size());
    int i = s_remap.isel;
    do i = (i + d + c) % c;
    while (!FSelectable(arow[size_t(i)]) && i != s_remap.isel);
    s_remap.isel = i;
    Sound();
}

// The first newly pressed key (Escape too) or mouse button / wheel this update, or -1.
int KeyPressed(GLFWwindow* window)
{
    int found = -1;
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
    {
        const bool f = glfwGetKey(window, key) == GLFW_PRESS;
        if (f && !s_remap.afKeyPrev[size_t(key)] && found < 0)
            found = key;
        s_remap.afKeyPrev[size_t(key)] = f;
    }
    if (found < 0)
        found = MouseBindingPressed(window);
    return found;
}

void StartCapture(int irow, bool controller)
{
    s_remap.irowCapture = irow;
    s_remap.fCaptureController = controller;
    s_remap.tCapture = g_clock.tReal;
    if (g_gl.window != nullptr)
    {
        KeyPressed(g_gl.window);   // (primed: the key or click that chose the cell is not taken)
        sly1actions::CaptureController();
    }
}

void Activate(const std::vector<ROW>& arow, int irow, int dir)
{
    const ROW& r = arow[size_t(irow)];
    if (FOption(r))
    {
        ChangeOption(r, dir);
        if (r.kind == RK_Advanced)
            s_remap.itop = 0;
        Sound();
        return;
    }
    switch (r.kind)
    {
    case RK_Reset: ResetDefaults(); break;
    case RK_Back: CloseRemapPage(); return;
    case RK_Binding:
    case RK_CameraKey:
        if (Binding(r, s_remap.col == 1) == -2)
            return;
        if (sly1actions::Active() && sly1actions::profile.readOnly && r.kind == RK_Binding)
            return;
        StartCapture(irow, s_remap.col == 1);
        break;
    default: return;
    }
    Sound();
}

// The menus' buttons: Cross confirms, Triangle goes back and Circle clears; the Japanese release confirms with Circle
// and goes back with Cross (prompt.cpp), so Triangle clears there.
JOY_BUTTON BtnConfirm() { return FJapaneseContent() ? BTN_CIRCLE : BTN_CROSS; }
JOY_BUTTON BtnBack() { return FJapaneseContent() ? BTN_CROSS : BTN_TRIANGLE; }
JOY_BUTTON BtnClear() { return FJapaneseContent() ? BTN_TRIANGLE : BTN_CIRCLE; }
char ChGlyph(JOY_BUTTON button) { return button == BTN_CIRCLE ? 'O' : button == BTN_TRIANGLE ? 'T' : 'X'; }

void HandleAll(JOY* pjoy)
{
    for (int b = 0; b < BTN_MAX; ++b)
        pjoy->SetHandled(static_cast<JOY_BUTTON>(b));
}

// ---- the page's geometry (framebuffer pixels), set as it is drawn
struct LAYOUT
{
    float x0 = 0, x1 = 0, yTitle = 0, yHead = 0, yRows = 0, dyRow = 0, yFoot = 0, y1 = 0;
    float xKeyboard = 0, xController = 0, wCell = 0, u = 1;
};
LAYOUT s_layout;
} // namespace

bool FRemapPageOpen() { return s_remap.fOpen; }

void OpenRemapPage()
{
    s_remap = REMAP{};
    s_remap.fOpen = true;
    s_remap.col = PromptKeyboardActive() ? 0 : 1;
    s_remap.fWaitRelease = true;
    sly1actions::Load();
    const std::vector<ROW> arow = Arow();
    Clamp(arow);
}

void CloseRemapPage()
{
    if (!s_remap.fOpen)
        return;
    s_remap.fOpen = false;
    s_remap.irowCapture = -1;
    Sound();
}

void UpdateRemapPage(JOY* pjoy)
{
    if (!s_remap.fOpen || pjoy == nullptr)
        return;
    std::vector<ROW> arow = Arow();
    Clamp(arow);
    GLFWwindow* window = g_gl.window;
    if (s_remap.irowCapture >= 0)
    {
        HandleAll(pjoy);
        const int key = window != nullptr ? KeyPressed(window) : GLFW_KEY_ESCAPE;
        const bool fTimeout = s_remap.fCaptureController && g_clock.tReal - s_remap.tCapture > 6.0f;
        if (key == GLFW_KEY_ESCAPE || fTimeout || s_remap.irowCapture >= int(arow.size()))
        {
            s_remap.irowCapture = -1;
            s_remap.fWaitRelease = true;
            return;
        }
        const ROW& r = arow[size_t(s_remap.irowCapture)];
        if (key == GLFW_KEY_DELETE)
        {
            Bind(r, s_remap.fCaptureController, -1);
            s_remap.irowCapture = -1;
            s_remap.fWaitRelease = true;
            return;
        }
        const int binding = s_remap.fCaptureController ? sly1actions::CaptureController() : key;
        if (binding >= 0)
        {
            if (std::getenv("SLY1_REMAPLOG"))
                std::printf("remap: row %d %s <- %d\n", s_remap.irowCapture, s_remap.fCaptureController ? "pad" : "key", binding);
            Bind(r, s_remap.fCaptureController, binding);
            s_remap.irowCapture = -1;
            s_remap.fWaitRelease = true;
            Sound();
        }
        return;
    }
    if (window != nullptr)
        KeyPressed(window);   // (keeps the key states current for the next capture)
    if (s_remap.fWaitRelease)
    {
        bool fHeld = false;
        for (int b = 0; b < BTN_MAX; ++b)
            fHeld = fHeld || pjoy->current[b];
        HandleAll(pjoy);
        if (fHeld)
            return;
        s_remap.fWaitRelease = false;
    }
    // Triangle / Start / the right mouse button: back
    if (pjoy->IsPressed(BtnBack()) || pjoy->IsPressed(BTN_START) || FMouseRightPressed())
    {
        HandleAll(pjoy);
        ConsumeMouseMenuClicks();
        CloseRemapPage();
        return;
    }
    // the mouse: hover selects the row and the column, a click changes, the wheel scrolls
    float xc = 0.0f, yc = 0.0f;
    if (MouseMenuCursor(&xc, &yc))
    {
        const LAYOUT& l = s_layout;
        if (xc >= l.x0 && xc < l.x1 && yc >= l.yRows && yc < l.yRows + l.dyRow * CROW_SHOWN && l.dyRow > 0.0f)
        {
            const int i = s_remap.itop + int((yc - l.yRows) / l.dyRow);
            if (i < int(arow.size()) && FSelectable(arow[size_t(i)]))
            {
                const int col = xc >= (l.xKeyboard + l.xController) * 0.5f ? 1 : 0;
                if (FMouseMenuMoved() || FMouseLeftPressed())
                {
                    if (i != s_remap.isel)
                        Sound();
                    s_remap.isel = i;
                    s_remap.col = col;
                }
                if (FMouseLeftPressed())
                {
                    ConsumeMouseMenuClicks();
                    HandleAll(pjoy);
                    Activate(arow, i, 1);
                    return;
                }
            }
        }
        if (const int wheel = MouseWheelNotches(); wheel != 0)
        {
            s_remap.itop = std::clamp(s_remap.itop - wheel, 0, std::max(0, int(arow.size()) - CROW_SHOWN));
            s_remap.isel = std::clamp(s_remap.isel, s_remap.itop, s_remap.itop + CROW_SHOWN - 1);
            Clamp(arow);
        }
    }
    if (const int d = pjoy->DySelectionJoy(g_clock.tReal); d != 0)
        Step(arow, d > 0 ? 1 : -1);
    if (const int d = pjoy->DxSelectionJoy(g_clock.tReal); d != 0)
    {
        const ROW& r = arow[size_t(s_remap.isel)];
        if (FOption(r))
            Activate(arow, s_remap.isel, d);
        else if (r.kind == RK_Binding || r.kind == RK_CameraKey)
        {
            const int col = std::clamp(s_remap.col + d, 0, 1);
            if (col != s_remap.col)
            {
                s_remap.col = col;
                Sound();
            }
        }
    }
    if (pjoy->IsPressed(BtnConfirm()))
    {
        HandleAll(pjoy);
        Activate(arow, s_remap.isel, 1);
        return;
    }
    const bool fDelete = window != nullptr && glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
    if ((pjoy->IsPressed(BtnClear()) || fDelete) && (arow[size_t(s_remap.isel)].kind == RK_Binding ||
                                                    arow[size_t(s_remap.isel)].kind == RK_CameraKey) &&
        Binding(arow[size_t(s_remap.isel)], s_remap.col == 1) != -2)
    {
        Bind(arow[size_t(s_remap.isel)], s_remap.col == 1, -1);
        s_remap.fWaitRelease = fDelete;
        Sound();
    }
    HandleAll(pjoy);   // (nothing else reaches the menu under the page)
    arow = Arow();
    Clamp(arow);
}

// ---- drawing
namespace
{
glm::vec4 Gs(int r, int g, int b, int a = 128)
{
    return glm::vec4(r / 128.0f, g / 128.0f, b / 128.0f, a / 128.0f);
}

void Rect(float x0, float y0, float x1, float y1, const glm::vec4& rgba)
{
    glBlotShader.Use();
    g_pfont->SetupDraw();
    BindBlotTexture(whiteTex);
    RenderGlyphQuad(x0, y0, x1 - x0, y1 - y0, 0.0f, 0.0f, 1.0f, 1.0f, rgba);
}

// text at (x, y) (its top), size: the font's line height in pixels; jh left / centre / right of x
float Text(const std::string& text, float x, float y, float size, glm::vec4 rgba, JH jh = JH_Left, float dxMax = 0.0f)
{
    CFontBrx* pfont = g_pfont;
    const float k = size / std::max(1.0f, static_cast<float>(pfont->m_dyUnscaled));
    pfont->PushScaling(k * 1.15f, k);
    CRichText rich(text.c_str(), pfont);
    float dx = rich.Dx();
    if (dxMax > 0.0f && dx > dxMax)
    {
        // (too long for its column: narrower)
        pfont->PopScaling();
        const float f = dxMax / dx;
        pfont->PushScaling(k * 1.15f * f, k);
        dx = dxMax;
    }
    CRichText draw(text.c_str(), pfont);
    CTextBox box;
    const float xLeft = jh == JH_Center ? x - dx * 0.5f : jh == JH_Right ? x - dx : x;
    box.SetPos(xLeft, y);
    box.SetSize(dx + 4.0f, size * 1.5f);
    box.SetTextColor(&rgba);
    box.SetHorizontalJust(JH_Left);
    box.SetVerticalJust(JV_Top);
    glBlotShader.Use();
    pfont->SetupDraw();
    draw.Draw(&box, nullptr);
    pfont->PopScaling();
    return dx;
}

// an icon h pixels high, centred at (cx, cy)
void Icon(unsigned tex, const cane::input::IconUV& uv, float cx, float cy, float h, float alpha = 1.0f)
{
    if (tex == 0 || !uv.valid)
        return;
    glBlotShader.Use();
    g_pfont->SetupDraw();
    BindBlotTexture(tex);
    RenderGlyphQuad(cx - h * 0.5f, cy - h * 0.5f, h, h, uv.u0, uv.v0, uv.u1, uv.v1, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
}

// a cell: the keycap / the button's icon, its name when there is no icon (or a text for unbound / fixed)
void Cell(const ROW& r, bool controller, float cx, float cy, bool fSel, Family family)
{
    const LAYOUT& l = s_layout;
    const auto& atlas = cane::input::promptAtlas();
    const int binding = Binding(r, controller);
    const glm::vec4 rgba = fSel ? Gs(120, 110, 60) : Gs(96, 96, 96);
    const float h = l.dyRow * 0.86f, size = l.dyRow * 0.62f;
    if (binding == -2)
    {
        Icon(atlas.texture(family), atlas.uv(family, Button::RS), cx, cy, h, 0.55f);
        return;
    }
    if (binding < 0)
    {
        Text(T("None", "Aucune", "Keine", "Ninguna", "Nessuna"), cx, cy - size * 0.55f, size, Gs(60, 60, 60), JH_Center);
        return;
    }
    if (controller)
    {
        const Button b = cane::input::fromGlfwBinding(binding);
        const auto uv = atlas.uv(family, b);
        // (PlayStation: the game's own glyphs for the face buttons and shoulders; its d-pad arrows are too small)
        const bool fGameGlyph = family == Family::PlayStation && b != Button::DpadUp && b != Button::DpadDown &&
                                b != Button::DpadLeft && b != Button::DpadRight;
        if (atlas.texture(family) != 0 && uv.valid && !fGameGlyph)
            Icon(atlas.texture(family), uv, cx, cy, h);
        else if (const char ch = cane::input::psGlyphFor(b); ch != '\0' && family == Family::PlayStation)
            Text(std::string("&2") + ch + "&.", cx, cy - size * 0.62f, size * 1.2f, Gs(128, 128, 128), JH_Center);
        else
            Text(PchzGamepadBindingName(binding, family), cx, cy - size * 0.55f, size, rgba, JH_Center, l.wCell * 0.9f);
        return;
    }
    const auto uv = atlas.keyUV(cane::input::promptKeyForBinding(binding));
    if (atlas.keyboardTexture() != 0 && uv.valid)
        Icon(atlas.keyboardTexture(), uv, cx, cy, h);
    else
        Text(PchzKeyboardKeyName(binding), cx, cy - size * 0.55f, size, rgba, JH_Center, l.wCell * 0.9f);
}
}

void DrawRemapPage()
{
    if (!s_remap.fOpen || g_pfont == nullptr)
        return;
    const std::vector<ROW> arow = Arow();
    Clamp(arow);
    // A 4:3 page at the picture's height (up to 1.5 x as wide on a wide picture), as Sly 2's.
    const float width = g_gl.width, height = g_gl.height;
    LAYOUT& l = s_layout;
    l.u = height / 448.0f;
    const float halfW = std::min(width * 0.5f - 12.0f * l.u, std::max(300.0f * l.u, std::min(width * 0.5f - 40.0f * l.u, 480.0f * l.u)));
    l.x0 = width * 0.5f - halfW;
    l.x1 = width * 0.5f + halfW;
    l.yTitle = 14.0f * l.u;
    l.yHead = 58.0f * l.u;
    l.yRows = 88.0f * l.u;
    l.dyRow = 28.0f * l.u;
    l.yFoot = 412.0f * l.u;
    l.y1 = height;
    l.wCell = std::clamp((l.x1 - l.x0) * 0.2f, 110.0f * l.u, 170.0f * l.u);
    l.xController = l.x1 - 16.0f * l.u - l.wCell * 0.5f;
    l.xKeyboard = l.xController - l.wCell;

    // the dark page over the picture, the title and column bands, the bottom band
    Rect(0.0f, 0.0f, width, height, Gs(0, 0, 0, 72));
    Rect(l.x0, 0.0f, l.x1, height, Gs(0, 0, 0, 100));
    Rect(l.x0, 0.0f, l.x1, l.yHead - 4.0f * l.u, Gs(0, 14, 30, 120));
    Rect(l.x0, l.yHead - 4.0f * l.u, l.x1, l.yHead - 2.0f * l.u, Gs(44, 60, 80, 128));
    Rect(l.x0, l.yHead, l.x1, l.yRows - 3.0f * l.u, Gs(0, 14, 30, 120));
    Rect(l.x0, l.yFoot, l.x1, height, Gs(18, 36, 58, 120));
    Rect(l.x0, l.yFoot, l.x1, l.yFoot + 2.0f * l.u, Gs(44, 60, 80, 128));

    const glm::vec4 rgbaTitle = Gs(128, 112, 60), rgbaHead = Gs(84, 100, 120), rgbaRow = Gs(100, 100, 100),
                    rgbaSel = Gs(128, 118, 50), rgbaGroup = Gs(110, 88, 40), rgbaDim = Gs(60, 60, 60);
    const Family family = PadFamily();
    Text(T("Controls", "Commandes", "Steuerung", "Controles", "Comandi"), l.x0 + 18.0f * l.u, l.yTitle, 30.0f * l.u, rgbaTitle);
    Text(cane::input::familyDisplayName(family), l.x1 - 18.0f * l.u, l.yTitle + 10.0f * l.u, 17.0f * l.u, rgbaHead, JH_Right);
    Text(T("Action", "Action", "Aktion", "Acción", "Azione"), l.x0 + 26.0f * l.u, l.yHead + 4.0f * l.u, 20.0f * l.u, rgbaHead);
    Text(T("Keyboard / Mouse", "Clavier / souris", "Tastatur / Maus", "Teclado / ratón", "Tastiera / mouse"), l.xKeyboard,
         l.yHead + 6.0f * l.u, 16.0f * l.u, rgbaHead, JH_Center, l.wCell - 6.0f * l.u);
    Text(T("Controller", "Manette", "Controller", "Mando", "Controller"), l.xController, l.yHead + 6.0f * l.u, 16.0f * l.u,
         rgbaHead, JH_Center, l.wCell - 6.0f * l.u);

    const int crow = std::min<int>(CROW_SHOWN, int(arow.size()) - s_remap.itop);
    const float sizeRow = 21.0f * l.u;
    for (int k = 0; k < crow; ++k)
    {
        const int i = s_remap.itop + k;
        const ROW& r = arow[size_t(i)];
        const float y = l.yRows + l.dyRow * float(k), cy = y + l.dyRow * 0.5f, yText = cy - sizeRow * 0.55f;
        const bool fSel = i == s_remap.isel;
        if (fSel)
        {
            Rect(l.x0 + 8.0f * l.u, y + 1.0f, l.x1 - 8.0f * l.u, y + l.dyRow - 1.0f, Gs(30, 30, 30, 110));
            if (r.kind == RK_Binding || r.kind == RK_CameraKey)
            {
                const float xc = s_remap.col ? l.xController : l.xKeyboard;
                Rect(xc - l.wCell * 0.45f, y + 2.0f, xc + l.wCell * 0.45f, y + l.dyRow - 2.0f, Gs(54, 54, 54, 110));
            }
        }
        const glm::vec4 rgba = fSel ? rgbaSel : rgbaRow;
        switch (r.kind)
        {
        case RK_Header:
            Text(r.label, l.x0 + 18.0f * l.u, yText, sizeRow, rgbaGroup);
            break;
        case RK_Icons:
        case RK_OriginalPs:
        case RK_AutoCamera:
        case RK_MouseInvertX:
        case RK_MouseInvertY:
        case RK_Advanced:
        {
            const std::string value = r.kind == RK_Icons ? StrIconStyle()
                : PchzOnOff(r.kind == RK_OriginalPs ? g_fAlwaysOriginalPsLayout
                          : r.kind == RK_AutoCamera ? g_fAutoCameraRotation
                          : r.kind == RK_MouseInvertX ? g_mouseopt.fInvertX
                          : r.kind == RK_MouseInvertY ? g_mouseopt.fInvertY
                          : sly1actions::Active());
            Text(r.label, l.x0 + 26.0f * l.u, yText, sizeRow, rgba, JH_Left, l.xKeyboard - l.wCell * 0.5f - l.x0 - 30.0f * l.u);
            Text(std::string("< ") + value + " >", (l.xKeyboard + l.xController) * 0.5f, yText, sizeRow * 0.9f, rgba, JH_Center,
                 l.wCell * 2.0f - 8.0f * l.u);
            break;
        }
        case RK_Reset:
        case RK_Back:
            Text(r.label, l.x0 + 26.0f * l.u, yText, sizeRow, rgba);
            break;
        case RK_Binding:
        case RK_CameraKey:
        {
            Text(r.label, l.x0 + 26.0f * l.u, yText, sizeRow, rgba, JH_Left, l.xKeyboard - l.wCell * 0.5f - l.x0 - 30.0f * l.u);
            for (int c = 0; c < 2; ++c)
            {
                const float xc = c ? l.xController : l.xKeyboard;
                if (s_remap.irowCapture == i && s_remap.fCaptureController == (c == 1))
                {
                    const bool fBlink = std::fmod(g_clock.tReal, 0.8f) < 0.5f;
                    Text("...", xc, yText, sizeRow, fBlink ? rgbaSel : rgbaDim, JH_Center);
                }
                else
                    Cell(r, c == 1, xc, cy, fSel && s_remap.col == c, family);
            }
            break;
        }
        }
    }
    // the scroll position
    if (s_remap.itop > 0)
        Text("^", l.x1 - 10.0f * l.u, l.yRows, 16.0f * l.u, rgbaHead, JH_Right);
    if (s_remap.itop + CROW_SHOWN < int(arow.size()))
        Text("v", l.x1 - 10.0f * l.u, l.yRows + l.dyRow * CROW_SHOWN - 18.0f * l.u, 16.0f * l.u, rgbaHead, JH_Right);
    // the bottom band: the capture line, or the buttons
    std::string hint;
    if (s_remap.irowCapture >= 0)
        hint = s_remap.fCaptureController
            ? T("Press a controller button (Escape: cancel)", "Appuyez sur un bouton de la manette (Échap : annuler)",
                "Controllertaste drücken (Esc: abbrechen)", "Pulsa un botón del mando (Esc: cancelar)",
                "Premi un pulsante del controller (Esc: annulla)")
            : T("Press a key or mouse button (Escape: cancel, Delete: none)",
                "Appuyez sur une touche ou un bouton de souris (Échap : annuler, Suppr : aucune)",
                "Taste oder Maustaste drücken (Esc: abbrechen, Entf: keine)",
                "Pulsa una tecla o botón del ratón (Esc: cancelar, Supr: ninguna)",
                "Premi un tasto o pulsante del mouse (Esc: annulla, Canc: nessuno)");
    else
        hint = std::string("&2") + ChGlyph(BtnConfirm()) + "&. " + T("Change", "Modifier", "Ändern", "Cambiar", "Cambia") +
               "     &2" + ChGlyph(BtnClear()) + "&. " + T("Clear", "Effacer", "Löschen", "Borrar", "Cancella") +
               "     &2" + ChGlyph(BtnBack()) + "&. " + T("Back", "Retour", "Zurück", "Atrás", "Indietro");
    char ach[32];
    std::snprintf(ach, sizeof(ach), "%d / %d", s_remap.isel + 1, int(arow.size()));
    Text(hint, l.x0 + 20.0f * l.u, l.yFoot + 9.0f * l.u, 17.0f * l.u, rgbaHead, JH_Left, l.x1 - l.x0 - 110.0f * l.u);
    Text(ach, l.x1 - 18.0f * l.u, l.yFoot + 9.0f * l.u, 17.0f * l.u, rgbaHead, JH_Right);
}
#endif
