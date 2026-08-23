-- Capture the launch main menu (BL: main-menu QOL). The harness starts in-game;
-- verify.show_menu(true) re-enters the menu screen so we can capture it.
verify.show_menu(true)
verify.capture("main_menu_warmup")
verify.capture("main_menu")
