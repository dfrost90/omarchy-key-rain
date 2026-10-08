#define main reader_main
#include "../reader.c"
#undef main
#include <assert.h>
int main(void) {
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names names = {.layout = "us"};
    struct xkb_keymap *map = xkb_keymap_new_from_names(ctx, &names, 0);
    state = xkb_state_new(map);
    assert(transition(1, KEY_LEFTSHIFT, 1));
    assert(!transition(1, KEY_LEFTSHIFT, 1));
    assert(transition(2, KEY_LEFTSHIFT, 1));
    assert(transition(1, KEY_LEFTSHIFT, 0));
    assert(xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE));
    assert(transition(2, KEY_LEFTSHIFT, 0));
    assert(!xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE));
    assert(!transition(1, KEY_MAX + 1, 1));
    count = 2; fds[1].fd = -1;
    transition(1, KEY_LEFTCTRL, 1); remove_device(1);
    assert(!references[KEY_LEFTCTRL]);
    assert(!xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE));
    xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(ctx);
    return 0;
}
