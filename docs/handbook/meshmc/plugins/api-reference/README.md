# MMCO Plugin API Reference

> Complete reference for every function pointer in the `MMCOContext` struct.

The MeshMC Plugin API is organized into **14 sections**, each covering a
distinct area of launcher functionality. Every API function is accessed
through the `MMCOContext*` pointer passed to `mmco_init()`.

## How to Read This Reference

Each section page documents:

| Column | Meaning |
|--------|---------|
| **Signature** | The C function-pointer signature exactly as declared in `PluginAPI.h` |
| **Parameters** | Every parameter with its type, purpose, and valid values |
| **Return Value** | What the function returns on success and failure |
| **Thread Safety** | Whether the function must be called from the main (GUI) thread |
| **Ownership** | Who owns returned pointers and when they become invalid |
| **Example** | Minimal C++ snippet showing correct usage |
| **Errors** | Common error conditions and how to handle them |
| **Implementation Notes** | How the trampoline works internally (for contributors) |

## Sections

| # | Section | Directory | Description |
|---|---------|-----------|-------------|
| S01 | [Lifecycle, Logging & Hooks](s01-lifecycle/) | `s01-lifecycle/` | Module handle, `log()`, `log_error()`, `hook_register()`, `hook_unregister()` |
| S02 | [Settings](s02-settings/) | `s02-settings/` | `setting_get()`, `setting_set()`, `setting_get_int()`, `setting_set_int()`, `setting_get_bool()`, `setting_set_bool()` |
| S03 | [Instance Query](s03-instances/) | `s03-instances/` | `instance_count()`, `instance_get_id()`, `instance_get_name()`, `instance_get_game_dir()`, `instance_get_icon_key()`, `instance_get_type()` |
| S04 | [Instance Management](s04-instance-management/) | `s04-instance-management/` | `instance_launch()`, `instance_kill()`, `instance_is_running()` |
| S05 | [Mods](s05-mods/) | `s05-mods/` | `instance_mod_count()`, `instance_mod_name()`, `instance_mod_version()`, `instance_mod_enabled()`, `instance_mod_set_enabled()`, `instance_mod_filename()` |
| S06 | [Worlds](s06-worlds/) | `s06-worlds/` | `instance_world_count()`, `instance_world_name()`, `instance_world_size()` |
| S07 | [Accounts & Java](s07-accounts-java/) | `s07-accounts-java/` | `account_count()`, `account_name()`, `account_uuid()`, `account_type()`, `java_count()`, `java_path()`, `java_version()`, `java_arch()` |
| S08 | [HTTP](s08-http/) | `s08-http/` | `http_get()`, `http_post()` |
| S09 | [Zip / Archive](s09-zip/) | `s09-zip/` | `zip_create_from_dir()`, `zip_extract_to_dir()` |
| S10 | [Filesystem](s10-filesystem/) | `s10-filesystem/` | `fs_read_file()`, `fs_write_file()`, `fs_list_dir()`, `fs_mkdir()`, `fs_remove()`, `fs_exists()` |
| S11 | [Instance Pages](s11-instance-pages/) | `s11-instance-pages/` | `ui_register_instance_page()` (via hook), `ui_register_instance_action()` |
| S12 | [UI Dialogs](s12-ui-dialogs/) | `s12-ui-dialogs/` | `ui_show_message()`, `ui_add_menu_item()`, `ui_file_open_dialog()`, `ui_file_save_dialog()`, `ui_input_dialog()`, `ui_confirm_dialog()` |
| S13 | [UI Page Builder](s13-ui-page-builder/) | `s13-ui-page-builder/` | `ui_page_create()`, `ui_page_add_to_list()`, `ui_layout_create()`, `ui_layout_add_widget()`, `ui_layout_add_layout()`, `ui_layout_add_spacer()`, `ui_page_set_layout()` |
| S14 | [Utility](s14-utility/) | `s14-utility/` | `get_app_version()`, `get_app_dir()`, `get_data_dir()`, `get_timestamp()` |

## Related Documentation

- [MMCO Module Format](../mmco-format.md) — Binary format, loading, validation
- [Hooks Reference](../hooks-reference/) — All hook IDs and payload structures
- [SDK Setup Guide](../sdk-guide/) — Build system setup for in-tree and out-of-tree plugins
- [Tutorials](../tutorials/) — Hello World, BackupSystem case study

## Conventions

### Module Handle (`void* mh`)

Every API function takes `void* mh` as its first parameter. This is the
**module handle** — an opaque pointer that identifies your plugin to the
host. Always pass `ctx->module_handle` which is set during `mmco_init()`.

### String Ownership

- **Input strings** (`const char*`): The plugin owns these. The API copies
  them internally. You can free/modify after the call returns.
- **Output strings** (`const char*` returns): Owned by the runtime. Valid
  until the **next API call on the same module**. Copy immediately if you
  need to keep the value.

### Error Handling

- Functions returning `int` use `0` for success, non-zero for error
  (unless documented otherwise, e.g. `ui_confirm_dialog` returns 1=Yes).
- Functions returning `const char*` return `nullptr` on error.
- Functions returning counts return `0` if the query is invalid.

### Thread Safety

All API functions **must be called from the main (GUI) thread** unless
explicitly documented as thread-safe. The `mmco_init()` and `mmco_unload()`
entry points are always called from the main thread.
