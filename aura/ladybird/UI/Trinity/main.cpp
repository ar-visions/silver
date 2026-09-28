/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Atomic.h>
#include <AK/ByteString.h>
#include <AK/GenericLexer.h>
#include <AK/LexicalPath.h>
#include <AK/Utf8View.h>
#include <LibCore/Environment.h>
#include <LibCore/Notifier.h>
#include <LibCore/System.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/SharedImageBuffer.h>
#include <LibGfx/SystemTheme.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebView/Application.h>
#include <LibWebView/HeadlessWebView.h>
#include <LibWebView/URL.h>
#include <LibWebView/Utilities.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Ladybird for silver's browser element: rgba8 frames out through
// shared memory (TRINITY_BROWSER_SHM), input in as lines on stdin.

namespace Trinity {

static constexpr u32 frame_magic = 0x57425254; // "TRBW"
static constexpr int max_frame_side = 4096;

// the element reads w, h, stride after seq changes
struct FrameHeader {
    u32 magic;
    Atomic<u32> seq;
    u32 width;
    u32 height;
    u32 stride;
    u32 reserved[3];
};

class Application final : public WebView::Application {
    WEB_VIEW_APPLICATION(Application)

private:
    Application() = default;

    // each element owns its engine; never hand off to another
    virtual bool should_coordinate_browser_process() const override { return false; }
};

class View final : public WebView::HeadlessWebView {
public:
    static NonnullOwnPtr<View> create(Core::AnonymousBuffer theme, Compositing::DevicePixelSize size, u8* shm)
    {
        auto view = adopt_own(*new View(move(theme), size, shm));
        view->initialize_client(CreateNewClient::Yes);
        return view;
    }

    void publish_frame()
    {
        if (!m_client_state.has_usable_bitmap || !m_client_state.front_bitmap.shared_image_buffer)
            return;
        auto bitmap = m_client_state.front_bitmap.shared_image_buffer->bitmap_if_present();
        if (!bitmap)
            return;
        auto size = m_client_state.front_bitmap.last_painted_size.to_type<int>();
        auto width = min(size.width(), bitmap->width());
        auto height = min(size.height(), bitmap->height());
        if (width <= 0 || height <= 0 || width > max_frame_side || height > max_frame_side)
            return;
        auto* header = reinterpret_cast<FrameHeader*>(m_shm);
        auto* pixels = m_shm + sizeof(FrameHeader);
        auto stride = static_cast<size_t>(width) * 4;
        // trinity textures are rgba8; the backing store is bgra8
        for (int y = 0; y < height; ++y) {
            auto const* src = bitmap->scanline_u8(y);
            auto* dst = pixels + y * stride;
            for (int x = 0; x < width; ++x, src += 4, dst += 4) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
            }
        }
        header->width = width;
        header->height = height;
        header->stride = stride;
        header->magic = frame_magic;
        header->seq.fetch_add(1, AK::MemoryOrder::memory_order_release);
    }

    void resize(int width, int height)
    {
        reset_viewport_size({ width, height });
    }

private:
    View(Core::AnonymousBuffer theme, Compositing::DevicePixelSize size, u8* shm)
        : HeadlessWebView(move(theme), size)
        , m_shm(shm)
    {
        on_ready_to_paint = [this] { publish_frame(); };
    }

    u8* m_shm { nullptr };
};

// GLFW key codes, as trinity reports them, to Ladybird's
static Compositing::KeyCode key_from_glfw(int code)
{
    using enum Compositing::KeyCode;
    if (code >= 'A' && code <= 'Z')
        return static_cast<Compositing::KeyCode>(code);
    if (code >= '0' && code <= '9')
        return static_cast<Compositing::KeyCode>(code);
    switch (code) {
    case 32:
        return Key_Space;
    case 256:
        return Key_Escape;
    case 257:
    case 335:
        return Key_Return;
    case 258:
        return Key_Tab;
    case 259:
        return Key_Backspace;
    case 261:
        return Key_Delete;
    case 262:
        return Key_Right;
    case 263:
        return Key_Left;
    case 264:
        return Key_Down;
    case 265:
        return Key_Up;
    case 266:
        return Key_PageUp;
    case 267:
        return Key_PageDown;
    case 268:
        return Key_Home;
    case 269:
        return Key_End;
    case 340:
        return Key_LeftShift;
    case 344:
        return Key_RightShift;
    case 341:
        return Key_LeftControl;
    case 345:
        return Key_RightControl;
    case 342:
        return Key_LeftAlt;
    case 346:
        return Key_RightAlt;
    default:
        return Key_Invalid;
    }
}

// the element's modifier bits: 1 shift, 2 ctrl, 4 alt, 8 super
static Compositing::KeyModifier modifiers_from(unsigned bits)
{
    unsigned result = 0;
    if (bits & 1)
        result |= Compositing::Mod_Shift;
    if (bits & 2)
        result |= Compositing::Mod_Ctrl;
    if (bits & 4)
        result |= Compositing::Mod_Alt;
    if (bits & 8)
        result |= Compositing::Mod_Super;
    return static_cast<Compositing::KeyModifier>(result);
}

// the element's buttons: 0 left, 1 right, 2 middle
static Compositing::MouseButton button_from(int button)
{
    switch (button) {
    case 0:
        return Compositing::MouseButton::Primary;
    case 1:
        return Compositing::MouseButton::Secondary;
    case 2:
        return Compositing::MouseButton::Middle;
    default:
        return Compositing::MouseButton::None;
    }
}

static void handle_line(View& view, StringView line)
{
    auto parts = line.split_view(' ');
    if (parts.is_empty())
        return;
    auto number = [&](size_t i) -> double {
        if (i >= parts.size())
            return 0;
        return parts[i].to_number<double>().value_or(0);
    };
    auto command = parts[0];

    if (command == "load"sv && parts.size() >= 2) {
        auto url_text = line.substring_view(5);
        if (auto url = WebView::sanitize_url(url_text); url.has_value())
            view.load(*url);
        return;
    }
    if (command == "size"sv) {
        view.resize(static_cast<int>(number(1)), static_cast<int>(number(2)));
        return;
    }
    // mouse <down|up|move|wheel> x y button buttons mods [dx dy]
    if (command == "mouse"sv && parts.size() >= 7) {
        using Type = Compositing::MouseEvent::Type;
        Type type = Type::MouseMove;
        if (parts[1] == "down"sv)
            type = Type::MouseDown;
        else if (parts[1] == "up"sv)
            type = Type::MouseUp;
        else if (parts[1] == "wheel"sv)
            type = Type::MouseWheel;
        Compositing::DevicePixelPoint position { static_cast<int>(number(2)), static_cast<int>(number(3)) };
        auto button = button_from(static_cast<int>(number(4)));
        unsigned held = 0;
        auto held_bits = static_cast<unsigned>(number(5));
        for (int b = 0; b < 3; ++b) {
            if (held_bits & (1u << b))
                held |= to_underlying(button_from(b));
        }
        // a step is 3 lines of 40 px; GLFW's up is Ladybird's minus
        auto wheel_x = -number(7) * 120;
        auto wheel_y = -number(8) * 120;
        view.enqueue_input_event(Compositing::MouseEvent {
            type, position, position,
            type == Type::MouseMove || type == Type::MouseWheel ? Compositing::MouseButton::None : button,
            static_cast<Compositing::MouseButton>(held),
            modifiers_from(static_cast<unsigned>(number(6))),
            wheel_x, wheel_y, Compositing::WheelDeltaPrecision::Discrete, Compositing::ScrollGesturePhase::None,
            type == Type::MouseDown || type == Type::MouseUp ? 1 : 0, nullptr });
        return;
    }
    // key <down|up> glfw_code mods
    if (command == "key"sv && parts.size() >= 4) {
        auto key = key_from_glfw(static_cast<int>(number(2)));
        if (key == Compositing::KeyCode::Key_Invalid)
            return;
        auto type = parts[1] == "down"sv ? Compositing::KeyEvent::Type::KeyDown : Compositing::KeyEvent::Type::KeyUp;
        u32 code_point = 0;
        if (key == Compositing::KeyCode::Key_Return)
            code_point = '\n';
        else if (key == Compositing::KeyCode::Key_Tab)
            code_point = '\t';
        else if (key == Compositing::KeyCode::Key_Backspace)
            code_point = 8;
        view.enqueue_input_event(Compositing::KeyEvent {
            type, key, modifiers_from(static_cast<unsigned>(number(3))), code_point, false,
            type == Compositing::KeyEvent::Type::KeyDown && code_point == '\n', nullptr });
        return;
    }
    // text <utf8>: typed characters, the rest of the line
    if (command == "text"sv && line.length() > 5) {
        for (auto code_point : Utf8View { line.substring_view(5) }) {
            view.enqueue_input_event(Compositing::KeyEvent { Compositing::KeyEvent::Type::KeyDown, Compositing::KeyCode::Key_Invalid, Compositing::Mod_None, code_point, false, true, nullptr });
            view.enqueue_input_event(Compositing::KeyEvent { Compositing::KeyEvent::Type::KeyUp, Compositing::KeyCode::Key_Invalid, Compositing::Mod_None, code_point, false, false, nullptr });
        }
        return;
    }
    // char codepoint mods: a typed character
    if (command == "char"sv && parts.size() >= 3) {
        auto code_point = static_cast<u32>(number(1));
        auto modifiers = modifiers_from(static_cast<unsigned>(number(2)));
        view.enqueue_input_event(Compositing::KeyEvent { Compositing::KeyEvent::Type::KeyDown, Compositing::KeyCode::Key_Invalid, modifiers, code_point, false, true, nullptr });
        view.enqueue_input_event(Compositing::KeyEvent { Compositing::KeyEvent::Type::KeyUp, Compositing::KeyCode::Key_Invalid, modifiers, code_point, false, false, nullptr });
        return;
    }
}

static ErrorOr<u8*> map_frame_memory()
{
    auto path = Core::Environment::get("TRINITY_BROWSER_SHM"sv);
    if (!path.has_value())
        return Error::from_string_literal("TRINITY_BROWSER_SHM is not set");
    auto size = sizeof(FrameHeader) + static_cast<size_t>(max_frame_side) * max_frame_side * 4;
    auto fd = open(ByteString { *path }.characters(), O_RDWR | O_CREAT, 0600);
    if (fd < 0)
        return Error::from_errno(errno);
    if (ftruncate(fd, static_cast<off_t>(size)) < 0)
        return Error::from_errno(errno);
    auto* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (memory == MAP_FAILED)
        return Error::from_errno(errno);
    return static_cast<u8*>(memory);
}

}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    auto app = TRY(Trinity::Application::create(arguments));
    if (app->should_exit_after_profile_coordination())
        return 0;

    auto* shm = TRY(Trinity::map_frame_memory());
    auto const& options = Trinity::Application::browser_options();
    auto theme_path = LexicalPath::join(WebView::s_ladybird_resource_root, "themes"sv, "Default.ini"sv);
    auto theme = TRY(Gfx::load_system_theme(theme_path.string()));
    auto view = Trinity::View::create(move(theme), { options.window_width, options.window_height }, shm);
    if (!options.urls.is_empty())
        view->load(options.urls.first());

    // one text line per command from the element
    ByteBuffer pending;
    auto input = Core::Notifier::construct(STDIN_FILENO, Core::Notifier::Type::Read);
    input->on_activation = [&] {
        u8 chunk[4096];
        auto count = read(STDIN_FILENO, chunk, sizeof(chunk));
        if (count <= 0) {
            // the element is gone: so is its browser
            Core::EventLoop::current().quit(0);
            return;
        }
        pending.append(chunk, static_cast<size_t>(count));
        while (true) {
            auto newline = StringView { pending.bytes() }.find('\n');
            if (!newline.has_value())
                break;
            auto line = StringView { pending.bytes().slice(0, *newline) };
            Trinity::handle_line(*view, line);
            pending = MUST(ByteBuffer::copy(pending.bytes().slice(*newline + 1)));
        }
    };

    return app->execute();
}
