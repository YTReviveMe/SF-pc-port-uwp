#include "text.hpp"

namespace sf::platform::launcher {
namespace {

constexpr LauncherText english_text{
    .shell =
        {
            .window_title = L"Syphon Filter PC",
            .launch_tab = L"Launch",
            .graphics_tab = L"Graphics",
            .controls_tab = L"Controls",
            .dossiers_tab = L"Dossiers",
            .play = L"Play",
            .close = L"Close",
            .browse = L"Browse...",
        },
    .launch =
        {
            .game_image = L"Game image (CUE)",
            .game_image_hint =
                L"Select the CUE file from your original Syphon Filter USA "
                L"v1.1 disc image.",
            .selected_image_prefix = L"Selected: ",
            .no_image_selected = L"No CUE file selected.",
            .text_language = L"Text language",
            .english_language = L"English",
            .russian_language = L"Russian",
            .cue_files = L"Syphon Filter disc images (*.cue)",
            .all_files = L"All files (*.*)",
        },
    .graphics =
        {
            .resolution = L"Internal resolution",
            .aspect_ratio = L"Aspect ratio",
            .antialiasing = L"Antialiasing",
            .frame_limit = L"Frame limit",
            .bilinear_filtering = L"Bilinear filtering",
            .trilinear_filtering = L"Trilinear filtering (mipmaps)",
            .anisotropic_filtering = L"Anisotropic filtering",
            .volumetric_effects = L"Volumetric effects (experimental)",
            .mission_skyboxes = L"New mission skyboxes",
            .vertical_sync = L"Vertical synchronization",
            .borderless_fullscreen = L"Borderless fullscreen",
            .disabled = L"Off",
            .smaa_ultra = L"SMAA Ultra",
            .msaa_2x = L"MSAA 2x",
            .msaa_4x = L"MSAA 4x",
            .msaa_8x = L"MSAA 8x",
            .unlimited = L"Unlimited",
            .fps_suffix = L" FPS",
            .adaptive_aspect = L"Adaptive (display aspect)",
            .original_aspect = L"Original PS1 (4:3)",
        },
    .controls =
        {
            .heading = L"INPUT CONFIGURATION",
            .input_device = L"Input device",
            .keyboard_mouse = L"Keyboard + Mouse",
            .controller = L"Controller",
            .controller_backend = L"Controller backend",
            .automatic_protocol = L"Automatic (recommended)",
            .xinput_protocol = L"XInput",
            .direct_input_protocol = L"DirectInput",
            .raw_input_protocol = L"Raw Input",
            .vibration = L"Controller vibration",
            .assignments = L"CONTROL ASSIGNMENTS",
            .binding_controls = L"BINDING CONTROL",
            .change_prefix = L"Change: ",
            .clear = L"Clear binding",
            .restore_defaults = L"Restore defaults",
            .stick_layout = L"Stick layout",
            .next_layout = L"Next layout",
            .select_action_hint = L"Select an action, then press Change.",
            .choose_button_hint =
                L"Choose a button, then assign it to an action.",
            .choose_layout_hint =
                L"Press Next layout to choose a stick scheme.",
            .movement_hint =
                L"Stealth: crouch + movement\r\nSide roll: roll + strafe",
            .waiting_for_input = L"Waiting for input...",
            .keyboard_capture_hint =
                L"Press any keyboard key, mouse button, or mouse-wheel "
                L"direction.",
            .waiting_for_controller = L"Waiting for controller input...",
            .controller_capture_hint =
                L"Press a controller button. ESC cancels.",
            .connect_controller_hint =
                L"Connect a controller and press a button. ESC cancels.",
            .no_controller = L"No controller detected.",
            .xbox_controller = L"Xbox",
            .playstation_controller = L"PlayStation",
            .nintendo_controller = L"Nintendo",
            .generic_controller = L"Generic controller",
            .binding_updated = L"Binding updated.",
            .binding_cleared = L"Binding cleared.",
            .capture_cancelled = L"Binding cancelled.",
            .defaults_restored = L"Default controls restored.",
            .layout_updated = L"Stick layout updated.",
            .controller_init_failed =
                L"Controller input could not be initialized.",
            .controller_disconnected = L"Controller disconnected.",
            .action_fallback = L"Action",
            .keyboard_actions =
                {
                    L"Move Forward",
                    L"Move Backward",
                    L"Turn Left",
                    L"Turn Right",
                    L"Strafe Left",
                    L"Strafe Right",
                    L"Run",
                    L"Roll",
                    L"Reload",
                    L"Aim",
                    L"Fire",
                    L"Crouch / Stealth",
                    L"Action / Interact",
                    L"Target Lock",
                    L"Quick Turn",
                    L"Quick Weapon Switch",
                    L"Previous Weapon",
                    L"Next Weapon",
                    L"Weapon Menu Previous",
                    L"Weapon Menu Next",
                    L"Pause Menu",
                    L"Quick Weapon 1",
                    L"Quick Weapon 2",
                    L"Quick Weapon 3",
                    L"Quick Weapon 4",
                    L"Quick Weapon 5",
                    L"Quick Weapon 6",
                    L"Quick Weapon 7",
                    L"Quick Weapon 8",
                    L"Quick Weapon 9",
                    L"Quick Weapon 10",
                },
            .controller_actions =
                {
                    L"Change Weapon",
                    L"Shoot",
                    L"Kneel",
                    L"Roll/Zoom Out",
                    L"Step Right",
                    L"Step Left",
                    L"Target Lock",
                    L"Use/Zoom In",
                    L"Aim",
                },
            .stick_layouts =
                {
                    L"Character Left / Camera Right",
                    L"Character Right / Camera Left",
                    L"Original (One Stick)",
                },
        },
    .dossiers =
        {
            .title = L"DOSSIERS",
            .subtitle = L"AGENCY / CLASSIFIED ARCHIVE",
            .previous = L"PREVIOUS",
            .next = L"NEXT",
            .file = L"FILE",
            .navigation_hint = L"A / D OR ARROWS / CHANGE FILE",
        },
    .validation =
        {
            .disc_image_required_title = L"Disc image required",
            .disc_image_required_message =
                L"Select the CUE file from your original Syphon Filter USA "
                L"v1.1 disc image. Keep every referenced BIN file in the "
                L"same folder.",
            .invalid_resolution_title = L"Invalid resolution",
            .invalid_resolution_message =
                L"Enter a resolution as WIDTH x HEIGHT. Minimum: 320 x 240.",
            .language_pack_missing_title = L"Language pack missing",
            .language_pack_missing_message =
                L"The Russian text pack is missing or incomplete. Reinstall "
                L"the full release package.",
            .dossiers_unavailable_title = L"Dossiers unavailable",
            .dossier_decoder_unavailable_message =
                L"Windows could not initialize the dossier image decoder.",
            .dossier_files_unavailable_message =
                L"The dossier images are missing or damaged.",
            .restricted_access_title = L"Restricted access",
            .unsupported_disc_title = L"Unsupported disc build",
            .startup_failed_title = L"Startup failed",
            .settings_save_failed_title = L"Settings not saved",
            .settings_save_failed_message =
                L"The launcher settings could not be saved completely. Check "
                L"write access to %LOCALAPPDATA%\\SyphonFilterPC.",
            .unexpected_error_title = L"Unexpected error",
        },
};

constexpr LauncherText russian_text{
    .shell =
        {
            .window_title = L"Syphon Filter PC",
            .launch_tab = L"Запуск",
            .graphics_tab = L"Графика",
            .controls_tab = L"Управление",
            .dossiers_tab = L"Досье",
            .play = L"Играть",
            .close = L"Закрыть",
            .browse = L"Обзор...",
        },
    .launch =
        {
            .game_image = L"Образ игры (CUE)",
            .game_image_hint =
                L"Выберите CUE-файл оригинального образа Syphon Filter USA "
                L"v1.1.",
            .selected_image_prefix = L"Выбран файл: ",
            .no_image_selected = L"CUE-файл не выбран.",
            .text_language = L"Язык текста",
            .english_language = L"English",
            .russian_language = L"Русский",
            .cue_files = L"Образы Syphon Filter (*.cue)",
            .all_files = L"Все файлы (*.*)",
        },
    .graphics =
        {
            .resolution = L"Внутреннее разрешение",
            .aspect_ratio = L"Соотношение сторон",
            .antialiasing = L"Сглаживание",
            .frame_limit = L"Ограничение кадров",
            .bilinear_filtering = L"Билинейная фильтрация",
            .trilinear_filtering = L"Трилинейная фильтрация (mip-карты)",
            .anisotropic_filtering = L"Анизотропная фильтрация",
            .volumetric_effects = L"Объёмные эффекты (экспериментально)",
            .mission_skyboxes = L"Новые скайбоксы миссий",
            .vertical_sync = L"Вертикальная синхронизация",
            .borderless_fullscreen = L"Полноэкранный режим без рамки",
            .disabled = L"Выкл.",
            .smaa_ultra = L"SMAA Ultra",
            .msaa_2x = L"MSAA 2x",
            .msaa_4x = L"MSAA 4x",
            .msaa_8x = L"MSAA 8x",
            .unlimited = L"Без ограничения",
            .fps_suffix = L" FPS",
            .adaptive_aspect = L"По экрану (адаптивно)",
            .original_aspect = L"Оригинал PS1 (4:3)",
        },
    .controls =
        {
            .heading = L"УПРАВЛЕНИЕ",
            .input_device = L"Устройство ввода",
            .keyboard_mouse = L"Клавиатура и мышь",
            .controller = L"Контроллер",
            .controller_backend = L"Протокол контроллера",
            .automatic_protocol = L"Автоматически (рекомендуется)",
            .xinput_protocol = L"XInput",
            .direct_input_protocol = L"DirectInput",
            .raw_input_protocol = L"Raw Input",
            .vibration = L"Вибрация",
            .assignments = L"НАЗНАЧЕНИЯ",
            .binding_controls = L"НАСТРОЙКА",
            .change_prefix = L"Изменить: ",
            .clear = L"Очистить",
            .restore_defaults = L"По умолчанию",
            .stick_layout = L"Схема стиков",
            .next_layout = L"Следующая схема",
            .select_action_hint = L"Выберите действие и нажмите «Изменить».",
            .choose_button_hint = L"Выберите кнопку для действия.",
            .choose_layout_hint = L"Нажмите «Следующая схема».",
            .movement_hint =
                L"Скрытность: присесть + движение\r\nКувырок: перекат + шаг",
            .waiting_for_input = L"Ожидание ввода...",
            .keyboard_capture_hint =
                L"Нажмите клавишу, кнопку мыши или прокрутите колесо.",
            .waiting_for_controller = L"Ожидание кнопки контроллера...",
            .controller_capture_hint = L"Нажмите кнопку. ESC — отмена.",
            .connect_controller_hint =
                L"Подключите контроллер и нажмите кнопку. ESC — отмена.",
            .no_controller = L"Контроллер не обнаружен.",
            .xbox_controller = L"Xbox",
            .playstation_controller = L"PlayStation",
            .nintendo_controller = L"Nintendo",
            .generic_controller = L"Обычный контроллер",
            .binding_updated = L"Назначение обновлено.",
            .binding_cleared = L"Назначение очищено.",
            .capture_cancelled = L"Назначение отменено.",
            .defaults_restored = L"Настройки по умолчанию восстановлены.",
            .layout_updated = L"Схема стиков обновлена.",
            .controller_init_failed =
                L"Не удалось инициализировать контроллер.",
            .controller_disconnected = L"Контроллер отключён.",
            .action_fallback = L"Действие",
            .keyboard_actions =
                {
                    L"Движение вперёд",
                    L"Движение назад",
                    L"Поворот влево",
                    L"Поворот вправо",
                    L"Шаг влево",
                    L"Шаг вправо",
                    L"Бег",
                    L"Перекат",
                    L"Перезарядка",
                    L"Прицеливание",
                    L"Огонь",
                    L"Присесть",
                    L"Действие",
                    L"Захват цели",
                    L"Быстрый разворот",
                    L"Быстрый выбор оружия",
                    L"Предыдущее оружие",
                    L"Следующее оружие",
                    L"Меню оружия: назад",
                    L"Меню оружия: вперёд",
                    L"Пауза",
                    L"Быстрое оружие 1",
                    L"Быстрое оружие 2",
                    L"Быстрое оружие 3",
                    L"Быстрое оружие 4",
                    L"Быстрое оружие 5",
                    L"Быстрое оружие 6",
                    L"Быстрое оружие 7",
                    L"Быстрое оружие 8",
                    L"Быстрое оружие 9",
                    L"Быстрое оружие 10",
                },
            .controller_actions =
                {
                    L"Сменить оружие",
                    L"Огонь",
                    L"Присесть",
                    L"Перекат / Уменьшить",
                    L"Шаг вправо",
                    L"Шаг влево",
                    L"Захват цели",
                    L"Действие / Увеличить",
                    L"Прицеливание",
                },
            .stick_layouts =
                {
                    L"Персонаж: левый / Камера: правый",
                    L"Персонаж: правый / Камера: левый",
                    L"Оригинальная: один стик",
                },
        },
    .dossiers =
        {
            .title = L"ДОСЬЕ",
            .subtitle = L"АГЕНТСТВО / СЕКРЕТНЫЙ АРХИВ",
            .previous = L"НАЗАД",
            .next = L"ДАЛЕЕ",
            .file = L"ФАЙЛ",
            .navigation_hint = L"A / D ИЛИ СТРЕЛКИ / СМЕНИТЬ ФАЙЛ",
        },
    .validation =
        {
            .disc_image_required_title = L"Требуется образ диска",
            .disc_image_required_message =
                L"Выберите CUE-файл оригинального образа Syphon Filter USA "
                L"v1.1. Все указанные в нём BIN-файлы должны находиться в "
                L"той же папке.",
            .invalid_resolution_title = L"Некорректное разрешение",
            .invalid_resolution_message =
                L"Введите разрешение в формате ШИРИНА × ВЫСОТА. Минимум: "
                L"320 × 240.",
            .language_pack_missing_title = L"Отсутствует языковой пакет",
            .language_pack_missing_message =
                L"Пакет русской локализации отсутствует или повреждён. "
                L"Переустановите полный пакет релиза.",
            .dossiers_unavailable_title = L"Досье недоступно",
            .dossier_decoder_unavailable_message =
                L"Не удалось запустить системный декодер изображений досье.",
            .dossier_files_unavailable_message =
                L"Изображения досье отсутствуют или повреждены.",
            .restricted_access_title = L"Доступ ограничен",
            .unsupported_disc_title = L"Неподдерживаемая версия диска",
            .startup_failed_title = L"Ошибка запуска",
            .settings_save_failed_title = L"Настройки не сохранены",
            .settings_save_failed_message =
                L"Не удалось полностью сохранить настройки лончера. Проверьте "
                L"доступ на запись к папке %LOCALAPPDATA%\\SyphonFilterPC.",
            .unexpected_error_title = L"Непредвиденная ошибка",
        },
};

} // namespace

const LauncherText &textFor(game::GameLanguage language) noexcept {
  switch (language) {
  case game::GameLanguage::russian_vit:
    return russian_text;
  case game::GameLanguage::english:
  default:
    return english_text;
  }
}

} // namespace sf::platform::launcher
