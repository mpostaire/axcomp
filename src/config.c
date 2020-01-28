#include "effect.h"
#include "session.h"
#include "util.h"
#include "window.h"
#include <confuse.h>
#include <string.h>

static const char *event_names[] = {"map-effect", "unmap-effect", "create-effect", "destroy-effect", "resize-effect", "desktop-change-effect"};
event get_event_from_name(const char *name) {
    for (int i = 0; i < NUM_EVENTS; i++)
        if (strcmp(name, event_names[i]) == 0)
            return i;
    return EVENT_UNKOWN;
}

static int validate_unsigned_int(cfg_t *cfg, cfg_opt_t *opt) {
    int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
    if (value < 0) {
        cfg_error(cfg, "integer option '%s' must be positive in section '%s %s'",
                  opt->name, cfg->name, cfg_title(cfg));
        return -1;
    }
    return 0;
}

static int validate_unsigned_float(cfg_t *cfg, cfg_opt_t *opt) {
    double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);
    if (value < 0) {
        cfg_error(cfg, "float option '%s' must be positive in section '%s %s'",
                  opt->name, cfg->name, cfg_title(cfg));
        return -1;
    }
    return 0;
}

void config_get(void) {
    cfg_opt_t effect_opts[] = {
        CFG_STR("function", NULL, CFGF_NONE),
        CFG_FLOAT("step", 0.03, CFGF_NONE),
        CFG_END()};
    cfg_opt_t window_rules_wintype_opts[] = {
        CFG_STR("map-effect", NULL, CFGF_NONE),
        CFG_STR("unmap-effect", NULL, CFGF_NONE),
        CFG_END()};
    cfg_opt_t window_rules_opts[] = {
        CFG_SEC("wintype", window_rules_wintype_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()};
    cfg_opt_t opts[] = {
        CFG_INT("effect-delta", 10, CFGF_NONE),
        CFG_SEC("effect", effect_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_SEC("rules", window_rules_opts, CFGF_NONE),
        CFG_END()};
    cfg_t *cfg, *cfg_sec;

    cfg = cfg_init(opts, CFGF_NONE);

    cfg_set_validate_func(cfg, "effect-delta", validate_unsigned_int);
    cfg_set_validate_func(cfg, "effect|step", validate_unsigned_float);

    if (cfg_parse(cfg, "axcomp.conf") == CFG_PARSE_ERROR)
        exit(EXIT_FAILURE);

    s.effect_delta = cfg_getint(cfg, "effect-delta");

    for (int i = 0; i < cfg_size(cfg, "effect"); i++) {
        cfg_sec = cfg_getnsec(cfg, "effect", i);

        char *effect_function = cfg_getstr(cfg_sec, "function");
        if (!effect_function) // TODO put conf file path in error msg
            eprintf("(TODO conf file path here): option 'function' must be set in section 'effect %s'\n", cfg_title(cfg_sec));

        if (!effect_new(cfg_title(cfg_sec), effect_function, cfg_getfloat(cfg_sec, "step"))) // TODO put conf file path in error msg
            eprintf("(TODO conf file path here): option 'function' must be set in section 'effect %s'\n", cfg_title(cfg_sec));

        free(effect_function);
    }

    for (int i = 0; i < cfg_size(cfg, "rules|wintype"); i++) {
        cfg_sec = cfg_getnsec(cfg, "rules|wintype", i);

        const char *wintype_name = cfg_title(cfg_sec);
        wintype window_type = get_wintype_from_name(wintype_name);
        if (window_type == WINTYPE_UNKNOWN) // TODO put conf file path in error msg
            eprintf("(TODO conf file path here): wrong wintype '%s' in section rules\n", wintype_name);
        free((char *) wintype_name);

        char *effect_name = cfg_getstr(cfg_sec, "map-effect");
        effect_set(window_type, get_event_from_name("map-effect"), effect_find(effect_name));
        free(effect_name);

        effect_name = cfg_getstr(cfg_sec, "unmap-effect");
        effect_set(window_type, get_event_from_name("unmap-effect"), effect_find(effect_name));
        free(effect_name);
    }
}
